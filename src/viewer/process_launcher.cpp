#include "stdafx.h"

#include "viewer/process_launcher.h"

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
    STARTUPINFOW startUpInfo{};
    startUpInfo.cb = sizeof(STARTUPINFOW);

    PROCESS_INFORMATION procInfo{};

    // Use the exe's own directory as the working directory — many apps
    // assume it matches their location and fail to find assets/DLLs otherwise
    const auto lastSep = exePath.find_last_of(L"\\/");
    const std::wstring workingDir =
        (lastSep != std::wstring::npos) ? exePath.substr(0, lastSep + 1) : L"";

    // Create the target process suspended so we can inject before it runs
    if (!CreateProcessW(exePath.c_str(), nullptr, nullptr, nullptr, FALSE,
                        CREATE_SUSPENDED | CREATE_DEFAULT_ERROR_MODE, nullptr,
                        workingDir.empty() ? nullptr : workingDir.c_str(),
                        &startUpInfo, &procInfo))
    {
        return false;
    }

    // Allocate space in the target process for the DLL path string
    const size_t pathBytes = (dllPath.size() + 1) * sizeof(wchar_t);
    LPVOID remotePath = VirtualAllocEx(procInfo.hProcess, nullptr, pathBytes,
                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    bool injected = false;

    if (remotePath &&
        WriteProcessMemory(procInfo.hProcess, remotePath, dllPath.c_str(),
                           pathBytes, nullptr))
    {
        // kernel32.dll is mapped at the same address in every process, so our
        // LoadLibraryW pointer is valid in the target process too
        auto* loadLibraryW = reinterpret_cast<LPTHREAD_START_ROUTINE>(
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));

        if (loadLibraryW)
        {
            HANDLE remoteThread = CreateRemoteThread(
                procInfo.hProcess, nullptr, 0, loadLibraryW, remotePath, 0,
                nullptr);

            if (remoteThread)
            {
                // Wait for LoadLibraryW (and our DllMain) to complete
                WaitForSingleObject(remoteThread, INFINITE);

                // Return value is truncated to 32 bits, but non-zero = success
                DWORD exitCode = 0;
                GetExitCodeThread(remoteThread, &exitCode);
                injected = (exitCode != 0);

                CloseHandle(remoteThread);
            }
        }
    }

    if (remotePath)
        VirtualFreeEx(procInfo.hProcess, remotePath, 0, MEM_RELEASE);

    if (!injected)
    {
        TerminateProcess(procInfo.hProcess, 1);
        CloseHandle(procInfo.hThread);
        CloseHandle(procInfo.hProcess);
        return false;
    }

    // DLL is loaded — resume the main thread and let the app run
    ResumeThread(procInfo.hThread);

    // Fire and forget, we don't need to hold onto these
    CloseHandle(procInfo.hThread);
    CloseHandle(procInfo.hProcess);

    return true;
}

} // namespace Tattler
