#include "stdafx.h"

#include "viewer/process_launcher.h"

#include <detours/detours.h>
#include <string>

namespace Tattler
{

auto ProcessLauncher::Launch(HWND parentHwnd) -> bool
{
    const auto exe = PickExecutable(parentHwnd);

    // User closed file picker
    if (exe.empty())
        return false;

    const auto hook = GetHookDllPath();

    return InjectAndLaunch(exe, hook);
}

auto ProcessLauncher::PickExecutable(HWND parentHwnd) -> std::wstring
{
    wchar_t buffer[MAX_PATH]{0};

    OPENFILENAMEW filename{};
    filename.lStructSize = sizeof(OPENFILENAMEW);
    filename.hwndOwner = parentHwnd;
    filename.lpstrFilter = L"Executables\0*.exe\0All Files\0*.*\0";
    filename.lpstrFile = buffer;
    filename.nMaxFile = MAX_PATH;
    filename.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&filename))
    {
        // User cancelled or error, return empty string
        return std::wstring{};
    }

    return std::wstring(buffer);
}

auto ProcessLauncher::GetHookDllPath() -> std::wstring
{
    wchar_t buffer[MAX_PATH]{0};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);

    std::wstring path(buffer);
    const auto workingDir = path.find_last_of(L"\\/");
    // Get absolute path up until last separator before file name
    auto fullPath = path.substr(0, workingDir + 1);

    // Append our hook to get full path
    fullPath += L"tattler_hook.dll";

    return fullPath;
}

auto ProcessLauncher::InjectAndLaunch(const std::wstring& exePath,
                                      const std::wstring& dllPath) -> bool
{
    // DetourCreateProcessWithDllEx takes a char*, so convert to narrow string
    int size = WideCharToMultiByte(CP_UTF8, 0, dllPath.c_str(), -1, nullptr, 0,
                                   nullptr, nullptr);

    std::string narrowDll(size - 1, '\0'); // size includes null terminator

    WideCharToMultiByte(CP_UTF8, 0, dllPath.c_str(), -1, narrowDll.data(), size,
                        nullptr, nullptr);

    STARTUPINFOW startUpInfo{};
    startUpInfo.cb = sizeof(STARTUPINFOW);

    PROCESS_INFORMATION procInfo{};

    auto success = DetourCreateProcessWithDllExW(exePath.c_str(), // Application name
                                  nullptr,         // Command line args
                                  nullptr,         // Process attributes
                                  nullptr,         // Thread attributes
                                  FALSE, // Whether it inherits handles
        CREATE_DEFAULT_ERROR_MODE, // Don't inherit error mode from us
        nullptr, // Inherit parent's environment
        nullptr, // Inherit parent's cwd
        &startUpInfo,
        &procInfo,
        narrowDll.c_str(),
        nullptr // Use real CreateProcessW
    );

    if (!success)
        return false;

    // Fire and forget, we don't need to hold onto these
    CloseHandle(procInfo.hThread);
    CloseHandle(procInfo.hProcess);

    return true;
}

} // namespace Tattler
