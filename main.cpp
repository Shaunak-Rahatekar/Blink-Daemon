#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shellapi.h>
#include <stdlib.h> // For _wtoi

// Custom message identifier for system tray interactions
#define WM_USER_TRAY_ICON (WM_USER + 1)

// Menu identifiers
#define ID_TRAY_EXIT 1001
#define ID_TRAY_SETTINGS 1002

// Settings Window Control IDs
#define ID_SETTINGS_ENABLE_CHK 2001
#define ID_SETTINGS_INTERVAL_TXT 2002
#define ID_SETTINGS_SAVE_BTN 2003

// Global variables
NOTIFYICONDATA nid = {};
bool g_isDaemonEnabled = true;
int g_workIntervalMinutes = 20;
HWND g_hSettingsWindow = NULL;

// Window Procedure forward declaration
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Entry point for Windows GUI applications
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // 1. Register the Window Class
    const wchar_t CLASS_NAME[] = L"BlinkDaemonMessageClass";
    const wchar_t SETTINGS_CLASS_NAME[] = L"BlinkDaemonSettingsClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClass(&wc)) {
        return 0;
    }

    // Register Settings Window Class
    WNDCLASS settingsWc = {};
    settingsWc.lpfnWndProc = SettingsWindowProc;
    settingsWc.hInstance = hInstance;
    settingsWc.lpszClassName = SETTINGS_CLASS_NAME;
    settingsWc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    settingsWc.hCursor = LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClass(&settingsWc)) {
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
                AppendMenu(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"Settings...");
                AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit Blink Daemon");

                // Critical: Window must be set to foreground before displaying the menu, 
                // otherwise the menu won't disappear when clicking outside of it.
                SetForegroundWindow(hwnd);

                // Display the menu and block until the user makes a selection or dismisses it
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
                
                // Process the menu selection
                if (cmd == ID_TRAY_EXIT) {
                    PostMessage(hwnd, WM_CLOSE, 0, 0);
                } else if (cmd == ID_TRAY_SETTINGS) {
                    if (g_hSettingsWindow == NULL) {
                        g_hSettingsWindow = CreateWindowEx(
                            WS_EX_CLIENTEDGE,
                            L"BlinkDaemonSettingsClass",
                            L"Blink Daemon Settings",
                            WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                            CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
                            NULL, NULL, GetModuleHandle(NULL), NULL
                        );
                        ShowWindow(g_hSettingsWindow, SW_SHOW);
                    } else {
                        SetForegroundWindow(g_hSettingsWindow);
                    }
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

LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // Enable Checkbox
            HWND hChk = CreateWindow(L"BUTTON", L"Enable Blink Daemon",
                WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                20, 20, 200, 20,
                hwnd, (HMENU)ID_SETTINGS_ENABLE_CHK, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            SendMessage(hChk, BM_SETCHECK, g_isDaemonEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

            // Interval Label
            CreateWindow(L"STATIC", L"Work Interval (minutes):",
                WS_VISIBLE | WS_CHILD,
                20, 60, 150, 20,
                hwnd, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

            // Interval TextBox
            HWND hTxt = CreateWindow(L"EDIT", L"",
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
                180, 60, 50, 20,
                hwnd, (HMENU)ID_SETTINGS_INTERVAL_TXT, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            
            wchar_t buffer[10];
            wsprintf(buffer, L"%d", g_workIntervalMinutes);
            SetWindowText(hTxt, buffer);

            // Medical Text Block
            CreateWindow(L"STATIC", L"Medical Information:\nExtended periods of screen time can cause digital eye strain. It is recommended to follow the 20-20-20 rule. Every 20 minutes, take a 20-second break and focus your eyes on something at least 20 feet away.",
                WS_VISIBLE | WS_CHILD,
                20, 100, 340, 80,
                hwnd, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

            // Save Button
            CreateWindow(L"BUTTON", L"Save",
                WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                150, 200, 80, 30,
                hwnd, (HMENU)ID_SETTINGS_SAVE_BTN, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

            return 0;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_SETTINGS_SAVE_BTN) {
                // Read Checkbox state
                HWND hChk = GetDlgItem(hwnd, ID_SETTINGS_ENABLE_CHK);
                g_isDaemonEnabled = (SendMessage(hChk, BM_GETCHECK, 0, 0) == BST_CHECKED);

                // Read TextBox state
                HWND hTxt = GetDlgItem(hwnd, ID_SETTINGS_INTERVAL_TXT);
                wchar_t buffer[10];
                GetWindowText(hTxt, buffer, 10);
                g_workIntervalMinutes = _wtoi(buffer);
                if (g_workIntervalMinutes <= 0) g_workIntervalMinutes = 1; // Basic validation

                DestroyWindow(hwnd);
            }
            return 0;
        }
        case WM_DESTROY: {
            g_hSettingsWindow = NULL; // Clear handle when window is destroyed
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
