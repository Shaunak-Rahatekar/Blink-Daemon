#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shellapi.h>

// Custom message identifier for system tray interactions
#define WM_USER_TRAY_ICON (WM_USER + 1)

// Menu identifiers
#define ID_TRAY_EXIT 1001

// Global variables
NOTIFYICONDATA nid = {};

// Window Procedure forward declaration
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Entry point for Windows GUI applications
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // 1. Register the Window Class
    const wchar_t CLASS_NAME[] = L"BlinkDaemonMessageClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClass(&wc)) {
        return 0;
    }

    // 2. Create a Message-Only Window
    // HWND_MESSAGE as parent makes it a message-only window, entirely invisible to the user
    // and hidden from broadcasts.
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Blink Daemon Hidden Window",
        0, // No window styles needed for a message-only window
        0, 0, 0, 0, // Position and size (ignored)
        HWND_MESSAGE, // Parent window (Specifies a message-only window)
        NULL, // Menu
        hInstance,
        NULL // Additional application data
    );

    if (hwnd == NULL) {
        return 0;
    }

    // 3. Initialize and Add the System Tray Icon
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1; // Unique ID for our icon
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER_TRAY_ICON;
    // Load the standard application icon (can be customized later)
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    lstrcpy(nid.szTip, L"Blink Daemon");

    // Add the icon to the system tray
    Shell_NotifyIcon(NIM_ADD, &nid);

    // 4. Run the Message Loop
    MSG msg = {};
    // GetMessage blocks until a message arrives in the queue
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_USER_TRAY_ICON: {
            // Handle tray icon events. 
            // lParam holds the specific mouse event, wParam holds the icon ID.
            if (LOWORD(lParam) == WM_RBUTTONUP) {
                // Get current mouse position for the context menu
                POINT pt;
                GetCursorPos(&pt);

                // Create a dynamic popup menu
                HMENU hMenu = CreatePopupMenu();
                AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit Blink Daemon");

                // Critical: Window must be set to foreground before displaying the menu, 
                // otherwise the menu won't disappear when clicking outside of it.
                SetForegroundWindow(hwnd);

                // Display the menu and block until the user makes a selection or dismisses it
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
                
                // Process the menu selection
                if (cmd == ID_TRAY_EXIT) {
                    PostMessage(hwnd, WM_CLOSE, 0, 0);
                }

                DestroyMenu(hMenu);
            }
            return 0;
        }

        case WM_CLOSE: {
            // Remove the system tray icon before closing the application
            Shell_NotifyIcon(NIM_DELETE, &nid);
            DestroyWindow(hwnd);
            return 0;
        }

        case WM_DESTROY: {
            // Post a WM_QUIT message to break the GetMessage loop
            PostQuitMessage(0);
            return 0;
        }
    }

    // Default processing for messages we don't explicitly handle
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
