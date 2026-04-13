#include "common.h"
#include "window.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    tocachat::ScopedCoInit com_scope(COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(com_scope.status())) {
        return 1;
    }

    tocachat::MainWindow window(instance);
    if (!window.Create()) {
        return 2;
    }

    return window.Run();
}
