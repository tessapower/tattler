#include "stdafx.h"

#include "viewer/viewer_app.h"

#include <cstdlib>
#include <stdexcept>

auto WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR,
                     _In_ int) -> int
{
    try
    {
        Tattler::ViewerApp app;
        if (!app.Init(hInstance, TEXT("Tattler")))
            return EXIT_FAILURE;

        app.Run();
        app.Shutdown();
    }
    catch (const std::exception& e)
    {
        MessageBoxA(nullptr, e.what(), "Tattler: Fatal Error",
                    MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }
    catch (...)
    {
        MessageBoxA(nullptr, "Unknown exception", "Tattler: Fatal Error",
                    MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
