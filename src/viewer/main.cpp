#include "stdafx.h"

#include "viewer/viewer_app.h"

#include <cstdlib>

auto WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR,
                     _In_ int) -> int
{
    Tattler::ViewerApp app;
    if (!app.Init(hInstance, TEXT("Tattler")))
        return EXIT_FAILURE;

    app.Run();
    app.Shutdown();

    return EXIT_SUCCESS;
}
