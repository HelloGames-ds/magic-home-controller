#include "WgcCapture.h"

#include <QtCore/qglobal.h>

#include "AppLog.h"

#ifdef Q_OS_WIN

#include <windows.h>
#include <d3d11.h>
#include <unknwn.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Graphics.Capture.h>

#include <windows.graphics.directx.direct3d11.interop.h>
#include <Windows.Graphics.Capture.Interop.h>

namespace wgc = winrt::Windows::Graphics::Capture;
namespace wd3d = winrt::Windows::Graphics::DirectX::Direct3D11;
using DxgiAccess = ::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess;

namespace elkbledom {

struct WgcCapture::Impl
{
    winrt::com_ptr<ID3D11Device> device;
    winrt::com_ptr<ID3D11DeviceContext> context;
    wd3d::IDirect3DDevice d3dDevice{nullptr};

    wgc::Direct3D11CaptureFramePool pool{nullptr};
    wgc::GraphicsCaptureSession session{nullptr};
    wgc::GraphicsCaptureItem item{nullptr};
    wgc::Direct3D11CaptureFrame frameObj{nullptr};

    winrt::com_ptr<IDXGISurface> mappedSurface;
    DXGI_MAPPED_RECT mapped{};
    bool mappedValid = false;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    int screenIndex = -1;
    QRect screenRect;
    QSize captureSize;

    quint64 startedMs = 0;
    quint64 framesNoData = 0;
    bool firstFrameLogged = false;
    bool noFrameLogged = false;
};

namespace {

struct MonitorSearch
{
    const QRect* wanted = nullptr;
    HMONITOR result = nullptr;
};

BOOL CALLBACK monitorEnumCallback(HMONITOR monitor, HDC, LPRECT, LPARAM lparam)
{
    auto* search = reinterpret_cast<MonitorSearch*>(lparam);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        return TRUE;
    }
    const RECT& r = info.rcMonitor;
    const QRect rc(r.left, r.top, r.right - r.left, r.bottom - r.top);
    if (rc == *search->wanted) {
        search->result = monitor;
        return FALSE;
    }
    return TRUE;
}

HMONITOR monitorForRect(const QRect& physRect)
{
    MonitorSearch search;
    search.wanted = &physRect;
    EnumDisplayMonitors(nullptr, nullptr, monitorEnumCallback, reinterpret_cast<LPARAM>(&search));
    return search.result;
}

QColor averageBgra(const DXGI_MAPPED_RECT& mapped, int pitch, int x0, int y0, int w, int h)
{
    quint64 rr = 0, gg = 0, bb = 0;
    quint32 n = 0;
    const int sx = qMax(1, w / 8);
    const int sy = qMax(1, h / 8);
    for (int y = y0; y < y0 + h; y += sy) {
        const BYTE* row = mapped.pBits + size_t(y) * pitch;
        for (int x = x0; x < x0 + w; x += sx) {
            const BYTE* p = row + size_t(x) * 4;
            rr += p[2];
            gg += p[1];
            bb += p[0];
            ++n;
        }
    }
    if (!n) {
        return QColor();
    }
    return QColor(int(rr / n), int(gg / n), int(bb / n));
}

QColor average10(const DXGI_MAPPED_RECT& mapped, int pitch, int x0, int y0, int w, int h)
{
    quint64 rr = 0, gg = 0, bb = 0;
    int n = 0;
    const int sx = qMax(1, w / 8);
    const int sy = qMax(1, h / 8);
    for (int y = y0; y < y0 + h; y += sy) {
        const BYTE* row = mapped.pBits + size_t(y) * pitch;
        for (int x = x0; x < x0 + w; x += sx) {
            const BYTE* p = row + size_t(x) * 4;
            const quint32 v = *reinterpret_cast<const quint32*>(p);
            rr += (v & 0x000003ffu);
            gg += (v >> 10) & 0x000003ffu;
            bb += (v >> 20) & 0x000003ffu;
            ++n;
        }
    }
    if (!n) {
        return QColor();
    }
    return QColor(int(rr * 255 / (n * 1023u)),
                  int(gg * 255 / (n * 1023u)),
                  int(bb * 255 / (n * 1023u)));
}

} // namespace

WgcCapture::WgcCapture() : impl_(std::make_unique<Impl>()) {}

WgcCapture::~WgcCapture()
{
    shutdown();
}

bool WgcCapture::active() const noexcept
{
    return impl_ && impl_->session && impl_->pool && impl_->item;
}

QSize WgcCapture::size() const noexcept
{
    return impl_ ? impl_->captureSize : QSize();
}

bool WgcCapture::start(int screenIndex, const QRect& physScreenRect)
{
    shutdown();

    if (screenIndex < 0 || physScreenRect.isEmpty()) {
        return false;
    }

    // WinRT requires an initialized apartment on this thread. If Qt/COM has
    // already initialized it in the same mode the call is a harmless no-op;
    // a differing mode just means the existing (likely MTA) one is used.
    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
    } catch (...) {
    }

    impl_->screenIndex = screenIndex;
    impl_->screenRect = physScreenRect;

    winrt::com_ptr<ID3D11Device> device;
    winrt::com_ptr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL gotLevel = D3D_FEATURE_LEVEL_11_0;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    const UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags,
                                   levels, UINT(std::size(levels)), D3D11_SDK_VERSION,
                                   device.put(), &gotLevel, context.put());
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createFlags,
                               levels, UINT(std::size(levels)), D3D11_SDK_VERSION,
                               device.put(), &gotLevel, context.put());
    }
    if (FAILED(hr)) {
        AppLog::logline(QStringLiteral("WGC: D3D11-устройство недоступно — задействован GDI-захват"));
        return false;
    }
    impl_->device = device;
    impl_->context = context;

    winrt::com_ptr<IDXGIDevice> dxgiDevice = device.as<IDXGIDevice>();
    if (!dxgiDevice) {
        AppLog::logline(QStringLiteral("WGC: нет IDXGIDevice — задействован GDI-захват"));
        return false;
    }

    winrt::com_ptr<::IInspectable> d3dObject;
    hr = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), d3dObject.put());
    if (FAILED(hr)) {
        AppLog::logline(QStringLiteral("WGC: обернуть D3D11-устройство не удалось (0x%1) — задействован GDI-захват")
                            .arg(QString::number(unsigned(hr), 16)));
        return false;
    }
    impl_->d3dDevice = d3dObject.as<wd3d::IDirect3DDevice>();

    const HMONITOR monitor = monitorForRect(physScreenRect);
    if (!monitor) {
        AppLog::logline(QStringLiteral("WGC: монитор для экрана %1 не найден — задействован GDI-захват")
                            .arg(screenIndex));
        return false;
    }

    wgc::GraphicsCaptureItem item{nullptr};
    try {
        auto factory = winrt::get_activation_factory<wgc::GraphicsCaptureItem>();
        auto interop = factory.as<::IGraphicsCaptureItemInterop>();
        hr = interop->CreateForMonitor(monitor,
                                       winrt::guid_of<winrt::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
                                       winrt::put_abi(item));
    } catch (const winrt::hresult_error&) {
        hr = E_FAIL;
    }
    if (FAILED(hr)) {
        AppLog::logline(QStringLiteral("WGC: CreateForMonitor не удался (0x%1) — задействован GDI-захват")
                            .arg(QString::number(unsigned(hr), 16)));
        return false;
    }
    impl_->item = item;

    try {
        impl_->pool = wgc::Direct3D11CaptureFramePool::CreateFreeThreaded(
            impl_->d3dDevice,
            winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            winrt::Windows::Graphics::SizeInt32{physScreenRect.width(), physScreenRect.height()});
        if (!impl_->pool) {
            return false;
        }
        impl_->session = impl_->pool.CreateCaptureSession(impl_->item);
        if (!impl_->session) {
            return false;
        }
        try {
            impl_->session.IsCursorCaptureEnabled(false); // Убираем курсор из кадров.
        } catch (...) {
        }
        impl_->session.StartCapture();
    } catch (const winrt::hresult_error&) {
        AppLog::logline(QStringLiteral("WGC: не удалось начать захват — задействован GDI-захват"));
        shutdown();
        return false;
    }

    impl_->captureSize = physScreenRect.size();
    impl_->startedMs = GetTickCount64();
    AppLog::logline(QStringLiteral("WGC: захват начат (экран %1, %2x%3)")
                        .arg(screenIndex)
                        .arg(physScreenRect.width())
                        .arg(physScreenRect.height()));
    return true;
}

bool WgcCapture::frameAvailable()
{
    if (!active()) {
        return false;
    }
    Impl& d = *impl_;
    if (d.mappedValid) {
        releaseFrame();
    }

    try {
        wgc::Direct3D11CaptureFrame frame = d.pool.TryGetNextFrame();
        if (!frame) {
            // Экран не меняется — пул кадров пуст. Через 5 секунд без единого
            // кадра сообщаем: DWM не отдаёт содержимое (возможно, true exclusive).
            if (!d.noFrameLogged && !d.firstFrameLogged
                && GetTickCount64() - d.startedMs > 5000) {
                d.noFrameLogged = true;
                AppLog::logline(QStringLiteral(
                    "WGC: кадров нет уже 5 с — захват монитора не получает содержимое"));
            }
            ++d.framesNoData;
            return false;
        }
        d.frameObj = frame;

        auto surface = frame.Surface();
        if (!surface) {
            d.frameObj = nullptr;
            return false;
        }

        ::IUnknown* unknown = winrt::get_unknown(surface);
        winrt::com_ptr<DxgiAccess> access = nullptr;
        HRESULT hr = unknown->QueryInterface(__uuidof(DxgiAccess),
                                             reinterpret_cast<void**>(access.put()));
        if (FAILED(hr)) {
            d.frameObj = nullptr;
            return false;
        }

        winrt::com_ptr<IDXGISurface> dxgiSurface;
        hr = access->GetInterface(__uuidof(IDXGISurface), dxgiSurface.put_void());
        if (FAILED(hr) || !dxgiSurface) {
            d.frameObj = nullptr;
            return false;
        }

        DXGI_SURFACE_DESC desc{};
        dxgiSurface->GetDesc(&desc);
        if (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM
            && desc.Format != DXGI_FORMAT_R10G10B10A2_UNORM) {
            AppLog::logline(QStringLiteral("WGC: неподдерживаемый формат кадра (0x%1)")
                                .arg(QString::number(unsigned(desc.Format), 16)));
            d.frameObj = nullptr;
            return false;
        }

        if (FAILED(dxgiSurface->Map(&d.mapped, DXGI_MAP_READ))) {
            d.frameObj = nullptr;
            return false;
        }
        d.mappedSurface = dxgiSurface;
        d.mappedValid = true;
        d.format = desc.Format;
        d.captureSize = QSize(static_cast<int>(desc.Width), static_cast<int>(desc.Height));
        if (!d.firstFrameLogged) {
            d.firstFrameLogged = true;
            AppLog::logline(QStringLiteral("WGC: первый кадр пришёл (%1x%2, формат 0x%3)")
                                .arg(desc.Width)
                                .arg(desc.Height)
                                .arg(QString::number(unsigned(desc.Format), 16)));
        }
        return true;
    } catch (const winrt::hresult_error&) {
        if (d.mappedValid) {
            d.mappedValid = false;
        }
        d.frameObj = nullptr;
        return false;
    }
}

QColor WgcCapture::sampleArea(const QRect& area) const
{
    if (!impl_ || !impl_->mappedValid || !impl_->mappedSurface) {
        return QColor();
    }

    const int width = impl_->captureSize.width();
    const int height = impl_->captureSize.height();
    const int pitch = static_cast<int>(impl_->mapped.Pitch);

    const int x0 = qBound(0, area.x(), qMax(0, width - 1));
    const int y0 = qBound(0, area.y(), qMax(0, height - 1));
    const int x1 = qBound(0, area.x() + area.width(), qMax(0, width - 1));
    const int y1 = qBound(0, area.y() + area.height(), qMax(0, height - 1));
    if (x1 <= x0 || y1 <= y0) {
        return QColor();
    }

    if (impl_->format == DXGI_FORMAT_R10G10B10A2_UNORM) {
        return average10(impl_->mapped, pitch, x0, y0, x1 - x0, y1 - y0);
    }
    return averageBgra(impl_->mapped, pitch, x0, y0, x1 - x0, y1 - y0);
}

void WgcCapture::releaseFrame()
{
    if (!impl_) {
        return;
    }
    Impl& d = *impl_;
    if (d.mappedValid && d.mappedSurface) {
        d.mappedSurface->Unmap();
        d.mappedValid = false;
    }
    d.mappedSurface = nullptr;
    d.mapped = {};
    if (d.frameObj) {
        try {
            d.frameObj.Close();
        } catch (...) {
        }
        d.frameObj = nullptr;
    }
}

void WgcCapture::shutdown() noexcept
{
    try {
        if (!impl_) {
            return;
        }
        Impl& d = *impl_;
        if (d.mappedValid && d.mappedSurface) {
            d.mappedSurface->Unmap();
            d.mappedValid = false;
        }
        d.mappedSurface = nullptr;
        d.mapped = {};
        if (d.frameObj) {
            d.frameObj.Close();
            d.frameObj = nullptr;
        }
        if (d.session) {
            d.session.Close();
            d.session = nullptr;
        }
        if (d.pool) {
            d.pool.Close();
            d.pool = nullptr;
        }
        d.item = nullptr;
        d.d3dDevice = nullptr;
        d.device = nullptr;
        d.context = nullptr;
        d.captureSize = {};
        d.screenIndex = -1;
        d.screenRect = {};
        d.format = DXGI_FORMAT_UNKNOWN;
    } catch (...) {
    }
}

} // namespace elkbledom

#endif // Q_OS_WIN