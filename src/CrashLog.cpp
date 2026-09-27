#include "CrashLog.h"

#include <windows.h>
#include <dbghelp.h>
#include <shlobj.h>

#include <cstdio>

namespace elkmhc {
namespace {

// В обработчике падения нельзя трогать кучу и Qt: используем только Win32,
// иначе само логирование может зависнуть или упасть повторно.
LONG WINAPI crashHandler(EXCEPTION_POINTERS* info)
{
    // Лог пишем в %LOCALAPPDATA%: рядом с exe лежит Program Files, куда
    // обычному пользователю писать нельзя и лог не создался бы.
    wchar_t path[MAX_PATH]{};
    const DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* lastSlash = nullptr;
    for (DWORD i = len; i > 0; --i) {
        if (path[i - 1] == L'\\' || path[i - 1] == L'/') {
            lastSlash = path + i - 1;
            break;
        }
    }
    wchar_t* fileName = lastSlash ? lastSlash + 1 : path;
    wchar_t folder[MAX_PATH]{};
    if (lastSlash) {
        // Папка программы нужна только как признак: что exe запущен из
        // Program Files. Копируем вручную, без wmemcpy из wchar.h.
        wchar_t* d = folder;
        for (const wchar_t* s = path; s != lastSlash && d < folder + MAX_PATH - 1; ++s, ++d)
            *d = *s;
        *d = L'\0';
    }
    wchar_t* target = nullptr;
    if (folder[0] && SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &target))) {
        wcscat_s(target, MAX_PATH, L"\\Magic Home Controller");
        CreateDirectoryW(target, nullptr);
        wcscat_s(target, MAX_PATH, L"\\crash.log");
    } else {
        wcscpy_s(fileName, MAX_PATH - (fileName - path), L"crash.log");
        target = path;
    }

    HANDLE file = CreateFileW(target, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        // Каталога могло не быть (первый запуск) — пробуем рядом с программой.
        wchar_t fallback[MAX_PATH]{};
        const DWORD n = GetModuleFileNameW(nullptr, fallback, MAX_PATH);
        wchar_t* slash = nullptr;
        for (DWORD i = n; i > 0; --i) {
            if (fallback[i - 1] == L'\\' || fallback[i - 1] == L'/') {
                slash = fallback + i - 1;
                break;
            }
        }
        if (!slash) return EXCEPTION_CONTINUE_SEARCH;
        wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - fallback), L"crash.log");
        file = CreateFileW(fallback, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return EXCEPTION_CONTINUE_SEARCH;
    }

    char text[4096]{};
    int at = 0;

    SYSTEMTIME now{};
    GetLocalTime(&now);
    at += std::snprintf(text + at, sizeof(text) - at,
                        "==== crash %04u-%02u-%02u %02u:%02u:%02u ====\n", now.wYear, now.wMonth,
                        now.wDay, now.wHour, now.wMinute, now.wSecond);
    at += std::snprintf(text + at, sizeof(text) - at, "code=0x%08lX address=%p\n",
                        info && info->ExceptionRecord
                            ? info->ExceptionRecord->ExceptionCode
                            : 0,
                        info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress
                                                      : nullptr);

    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(info && info->ExceptionRecord
                                                       ? info->ExceptionRecord->ExceptionAddress
                                                       : nullptr),
                       &module);
    wchar_t modulePath[MAX_PATH]{};
    const DWORD moduleLen = module ? GetModuleFileNameW(module, modulePath, MAX_PATH) : 0;

    // Символы берём из .pdb того же модуля: без него в стеке только адреса.
    char modulePathA[MAX_PATH]{};
    if (moduleLen > 0) WideCharToMultiByte(CP_UTF8, 0, modulePath, -1, modulePathA, MAX_PATH,
                                            nullptr, nullptr);
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    const BOOL symbols = SymInitialize(GetCurrentProcess(), modulePathA, moduleLen > 0);

    void* frames[62]{};
    const WORD captured = CaptureStackBackTrace(2, 62, frames, nullptr);

    SYMBOL_INFO symbol{};
    symbol.SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol.MaxNameLen = 511;

    for (WORD i = 0; i < captured; ++i) {
        const auto address = reinterpret_cast<DWORD64>(frames[i]);
        char name[512]{};
        if (symbols && SymFromAddr(GetCurrentProcess(), address, nullptr, &symbol)) {
            std::snprintf(name, sizeof(name) - 1, "%s", symbol.Name);
        } else {
            std::snprintf(name, sizeof(name), "%p", frames[i]);
        }
        if (at + 260 >= static_cast<int>(sizeof(text))) break;
        at += std::snprintf(text + at, sizeof(text) - at, "#%02u %p %s\n", i, frames[i], name);
    }
    if (captured == 0) at += std::snprintf(text + at, sizeof(text) - at, "(stack not captured)\n");
    std::snprintf(text + at, sizeof(text) - at, "\n");

    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(at), &written, nullptr);
    CloseHandle(file);
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

void installCrashLog()
{
    SetUnhandledExceptionFilter(crashHandler);
}

} // namespace elkmhc
