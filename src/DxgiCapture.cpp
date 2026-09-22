#include "DxgiCapture.h"

#include <QDebug>

#include <QtCore/qglobal.h>

#include "AppLog.h"

#ifdef Q_OS_WIN

#include <wrl/client.h>
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>

using Microsoft::WRL::ComPtr;

namespace elkbledom {

struct DxgiCapture::Impl
{
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGIOutputDuplication> duplication;
    ComPtr<IDXGISurface> surface;

    DXGI_OUTDUPL_DESC desc{};
    DXGI_MAPPED_RECT mapped{};
    bool mappedValid = false;
    int lostCount = 0;

    int screenIndex = -1;
    QRect screenRect;
};

namespace {

bool isSupportedFormat(DXGI_FORMAT format)
{
    return format == DXGI_FORMAT_B8G8R8A8_UNORM
        || format == DXGI_FORMAT_R10G10B10A2_UNORM;
}

QColor average10(const DXGI_MAPPED_RECT& r, int pitch, int x0, int y0, int w, int h)
{
    quint64 rr = 0, gg = 0, bb = 0;
    int n = 0;
    const int sx = qMax(1, w / 8);
    const int sy = qMax(1, h / 8);
    for (int y = y0; y < y0 + h; y += sy) {
        const BYTE* row = r.pBits + size_t(y) * pitch;
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

DxgiCapture::DxgiCapture() : impl_(std::make_unique<Impl>()) {}

DxgiCapture::~DxgiCapture()
{
    shutdown();
}

bool DxgiCapture::active() const noexcept
{
    return impl_ && impl_->duplication.Get() != nullptr;
}

QSize DxgiCapture::size() const noexcept
{
    if (!impl_ || !impl_->duplication) {
        return {};
    }
    return QSize(static_cast<int>(impl_->desc.ModeDesc.Width),
                 static_cast<int>(impl_->desc.ModeDesc.Height));
}

int DxgiCapture::width() const noexcept
{
    return size().width();
}

int DxgiCapture::height() const noexcept
{
    return size().height();
}

bool DxgiCapture::start(int screenIndex, const QRect& physScreenRect)
{
    shutdown();

    if (screenIndex < 0) {
        return false;
    }

    impl_->screenIndex = screenIndex;
    impl_->screenRect = physScreenRect;
    impl_->lostCount = 0;

    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL gotLevel = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags,
                                   levels, UINT(std::size(levels)), D3D11_SDK_VERSION,
                                   &impl_->device, &gotLevel, &impl_->context);
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createFlags,
                               levels, UINT(std::size(levels)), D3D11_SDK_VERSION,
                               &impl_->device, &gotLevel, &impl_->context);
    }
    if (FAILED(hr)) {
        return false;
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(impl_->device.As(&dxgiDevice))) {
        return false;
    }

    ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgiDevice->GetAdapter(&adapter))) {
        return false;
    }

    ComPtr<IDXGIOutput> chosen;
    for (UINT i = 0;; ++i) {
        ComPtr<IDXGIOutput> output;
        if (adapter->EnumOutputs(i, &output) != S_OK) {
            break;
        }
        DXGI_OUTPUT_DESC outDesc{};
        if (FAILED(output->GetDesc(&outDesc))) {
            continue;
        }
        QRect outRect(outDesc.DesktopCoordinates.left, outDesc.DesktopCoordinates.top,
                      outDesc.DesktopCoordinates.right - outDesc.DesktopCoordinates.left,
                      outDesc.DesktopCoordinates.bottom - outDesc.DesktopCoordinates.top);
        if (!outRect.intersected(physScreenRect).isEmpty()) {
            chosen = output;
            break;
        }
    }
    if (!chosen) {
        AppLog::logline(QStringLiteral("DXGI: вывод для экрана %1 не найден — задействован GDI-захват")
                            .arg(screenIndex));
        return false;
    }

    ComPtr<IDXGIOutput1> output1;
    if (FAILED(chosen.As(&output1))) {
        return false;
    }
    hr = output1->DuplicateOutput(impl_->device.Get(), &impl_->duplication);
    if (FAILED(hr)) {
        impl_->duplication.Reset();
        AppLog::logline(QStringLiteral("DXGI: DuplicateOutput не удался (0x%1) — задействован GDI-захват")
                            .arg(QString::number(unsigned(hr), 16)));
        return false;
    }
    impl_->duplication->GetDesc(&impl_->desc);

    if (!isSupportedFormat(impl_->desc.ModeDesc.Format)) {
        AppLog::logline(QStringLiteral("DXGI: неподдерживаемый формат кадра (0x%1) — задействован GDI-захват")
                            .arg(QString::number(unsigned(impl_->desc.ModeDesc.Format), 16)));
        impl_->duplication.Reset();
        return false;
    }

    return true;
}

bool DxgiCapture::frameAvailable()
{
    if (!active()) {
        return false;
    }

    Q_ASSERT(!impl_->mappedValid);

    DXGI_OUTDUPL_FRAME_INFO info{};
    ComPtr<IDXGIResource> resource;
    HRESULT hr = impl_->duplication->AcquireNextFrame(0, &info, &resource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        return false;
    }
    if (hr == DXGI_ERROR_ACCESS_LOST) {
        // Resolution/mode changed (e.g. a game flipped exclusive fullscreen).
        // A single context switch is normal, but a storm of losses means the
        // duplication cannot be kept alive (exclusive fullscreen) — fall back
        // to GDI instead of recreating the duplicator forever.
        ++impl_->lostCount;
        if (impl_->lostCount >= 3) {
            AppLog::logline(QStringLiteral("DXGI: частые потери кадра — включен GDI-захват"));
            shutdown();
        }
        return false;
    }
    if (FAILED(hr)) {
        return false;
    }

    if (FAILED(resource.As(&impl_->surface))) {
        impl_->duplication->ReleaseFrame();
        return false;
    }
    IDXGISurface* surf = impl_->surface.Get();
    if (FAILED(surf->Map(&impl_->mapped, DXGI_MAP_READ))) {
        impl_->surface.Reset();
        impl_->duplication->ReleaseFrame();
        return false;
    }
    impl_->mappedValid = true;
    impl_->lostCount = 0;
    return true;
}

QColor DxgiCapture::sampleArea(const QRect& area) const
{
    if (!impl_ || !impl_->mappedValid) {
        return QColor();
    }

    const int width = static_cast<int>(impl_->desc.ModeDesc.Width);
    const int height = static_cast<int>(impl_->desc.ModeDesc.Height);
    const int pitch = static_cast<int>(impl_->mapped.Pitch);
    const int pitchX = pitch / 4;

    int x0 = qBound(0, area.x(), qMax(0, width - 1));
    int y0 = qBound(0, area.y(), qMax(0, height - 1));
    int x1 = qBound(0, area.x() + area.width(), qMax(0, width - 1));
    int y1 = qBound(0, area.y() + area.height(), qMax(0, height - 1));
    if (x1 <= x0 || y1 <= y0) {
        return QColor();
    }

    if (impl_->desc.ModeDesc.Format == DXGI_FORMAT_R10G10B10A2_UNORM) {
        return average10(impl_->mapped, pitch, x0, y0, x1 - x0, y1 - y0);
    }

    quint64 rr = 0, gg = 0, bb = 0;
    quint32 n = 0;
    const int sx = qMax(1, (x1 - x0) / 8);
    const int sy = qMax(1, (y1 - y0) / 8);
    for (int y = y0; y < y1; y += sy) {
        const BYTE* row = impl_->mapped.pBits + size_t(y) * pitch;
        for (int x = x0; x < x1; x += sx) {
            const BYTE* p = row + size_t(x) * pitchX * 4;
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

void DxgiCapture::releaseFrame()
{
    if (!impl_) {
        return;
    }
    if (impl_->mappedValid) {
        impl_->surface->Unmap();
        impl_->mappedValid = false;
    }
    impl_->surface.Reset();
    if (impl_->duplication) {
        impl_->duplication->ReleaseFrame();
    }
}

void DxgiCapture::shutdown() noexcept
{
    if (!impl_) {
        return;
    }
    if (impl_->mappedValid) {
        impl_->surface->Unmap();
        impl_->mappedValid = false;
    }
    impl_->surface.Reset();
    impl_->duplication.Reset();
    impl_->context.Reset();
    impl_->device.Reset();
}

} // namespace elkbledom

#endif // Q_OS_WIN