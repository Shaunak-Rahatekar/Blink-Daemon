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

// Timer IDs
#define ID_WORK_TIMER 3001
#define ID_EXERCISE_TIMER 3002

// Overlay Window Control IDs
#define ID_OVERLAY_TERMINATE_BTN 4001

// Global variables
NOTIFYICONDATA nid = {};
bool g_isDaemonEnabled = true;
int g_workIntervalMinutes = 20;
HWND g_hSettingsWindow = NULL;
HWND g_hMainWindow = NULL;
HWND g_hOverlayWindow = NULL;
int g_exerciseStep = 1;

// Window Procedure forward declaration
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK OverlayWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

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

    // Register Overlay Window Class
    WNDCLASS overlayWc = {};
    overlayWc.lpfnWndProc = OverlayWindowProc;
    overlayWc.hInstance = hInstance;
    overlayWc.lpszClassName = L"BlinkDaemonOverlayClass";
    overlayWc.hCursor = LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClass(&overlayWc)) {
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
    
    g_hMainWindow = hwnd;

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

    // Show the Settings Window on startup so the user knows the application is running
    g_hSettingsWindow = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"BlinkDaemonSettingsClass",
        L"Blink Daemon Settings",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        NULL, NULL, hInstance, NULL
    );
    ShowWindow(g_hSettingsWindow, SW_SHOW);

    // 3.5. Start the background timer if enabled
    if (g_isDaemonEnabled) {
        SetTimer(g_hMainWindow, ID_WORK_TIMER, g_workIntervalMinutes * 60 * 1000, NULL);
    }

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

        case WM_TIMER: {
            if (wParam == ID_WORK_TIMER) {
                // Spawn the lockdown overlay instead of a message box
                if (g_hOverlayWindow == NULL) {
                    g_exerciseStep = 1;
                    g_hOverlayWindow = CreateWindowEx(
                        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                        L"BlinkDaemonOverlayClass",
                        L"Blink Daemon Break",
                        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
                        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
                        NULL, NULL, GetModuleHandle(NULL), NULL
                    );
                }
            }
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

                // Apply Timer Settings
                KillTimer(g_hMainWindow, ID_WORK_TIMER);
                if (g_isDaemonEnabled) {
                    SetTimer(g_hMainWindow, ID_WORK_TIMER, g_workIntervalMinutes * 60 * 1000, NULL);
                }

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

LRESULT CALLBACK OverlayWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // Start the exercise sequence timer (60 seconds per step)
            SetTimer(hwnd, ID_EXERCISE_TIMER, 60 * 1000, NULL);
            
            // Create Emergency Terminate button
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int screenHeight = GetSystemMetrics(SM_CYSCREEN);
            int splitX = screenWidth * 3 / 4;
            
            CreateWindow(L"BUTTON", L"Emergency Terminate",
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                splitX + 30, screenHeight - 80, 200, 40,
                hwnd, (HMENU)ID_OVERLAY_TERMINATE_BTN, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
                
            return 0;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_OVERLAY_TERMINATE_BTN) {
                // Show 5 annoying message boxes
                int result1 = MessageBox(hwnd, L"Are you sure you want to skip your eye exercise? Your eyes need rest!", L"Warning 1/5", MB_YESNO | MB_ICONWARNING | MB_TOPMOST);
                if (result1 == IDYES) {
                    int result2 = MessageBox(hwnd, L"Skipping these breaks can lead to permanent digital eye strain. Are you ABSOLUTELY sure this is an emergency?", L"Warning 2/5", MB_YESNO | MB_ICONWARNING | MB_TOPMOST);
                    if (result2 == IDYES) {
                        int result3 = MessageBox(hwnd, L"This is your third warning. Your long-term eye health is more important than a few minutes of work. Continue skipping?", L"Warning 3/5", MB_YESNO | MB_ICONWARNING | MB_TOPMOST);
                        if (result3 == IDYES) {
                            int result4 = MessageBox(hwnd, L"Please reconsider. The 20-20-20 rule is scientifically proven to prevent fatigue. Are you 100% sure you want to exit?", L"Warning 4/5", MB_YESNO | MB_ICONWARNING | MB_TOPMOST);
                            if (result4 == IDYES) {
                                int result5 = MessageBox(hwnd, L"Final confirmation. If you click Yes, the exercise will be cancelled.", L"Warning 5/5", MB_YESNO | MB_ICONERROR | MB_TOPMOST);
                                if (result5 == IDYES) {
                                    DestroyWindow(hwnd);
                                }
                            }
                        }
                    }
                }
            }
            return 0;
        }
        case WM_TIMER: {
            if (wParam == ID_EXERCISE_TIMER) {
                g_exerciseStep++;
                if (g_exerciseStep > 5) {
                    DestroyWindow(hwnd); // Exercise complete
                } else {
                    InvalidateRect(hwnd, NULL, TRUE); // Force a repaint for the next step
                }
            }
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            
            // Calculate 75% width for the split screen
            int splitX = (rcClient.right - rcClient.left) * 3 / 4;
            
            RECT rcLeft = rcClient;
            rcLeft.right = splitX;
            
            RECT rcRight = rcClient;
            rcRight.left = splitX;
            
            // Fill Left Side (75%) with Black
            HBRUSH hBrushBlack = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hdc, &rcLeft, hBrushBlack);
            DeleteObject(hBrushBlack);
            
            // Fill Right Side (25%) with Dark Gray
            HBRUSH hBrushGray = CreateSolidBrush(RGB(40, 40, 40));
            FillRect(hdc, &rcRight, hBrushGray);
            DeleteObject(hBrushGray);
            
            // Draw instruction text centered in the black pane
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            
            const wchar_t* instruction = L"";
            switch (g_exerciseStep) {
                case 1: instruction = L"Step 1/5: Focus on a distant object for 60 seconds."; break;
                case 2: instruction = L"Step 2/5: Slowly blink your eyes 10 times."; break;
                case 3: instruction = L"Step 3/5: Roll your eyes in a circle, clockwise."; break;
                case 4: instruction = L"Step 4/5: Roll your eyes in a circle, counter-clockwise."; break;
                case 5: instruction = L"Step 5/5: Close your eyes tightly for 60 seconds."; break;
            }
            
            DrawText(hdc, instruction, -1, &rcLeft, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            // Draw Medical Information and Settings in the right pane
            RECT rcRightText = rcRight;
            rcRightText.left += 30;
            rcRightText.right -= 30;
            rcRightText.top += 50;
            rcRightText.bottom -= 50;
            
            const wchar_t* infoText = L"MEDICAL INFORMATION\n\n"
                                      L"Extended periods of screen time can cause digital eye strain. "
                                      L"It is recommended to follow the 20-20-20 rule. Every 20 minutes, "
                                      L"take a 20-second break and focus your eyes on something at least 20 feet away.\n\n\n"
                                      L"SETTINGS\n\n"
                                      L"To change the interval, right-click the Blink Daemon icon in the System Tray (bottom right of your screen) and select 'Settings...'.\n\n"
                                      L"Press Ctrl+Shift+Q to exit this screen if needed.";
                                      
            DrawText(hdc, infoText, -1, &rcRightText, DT_LEFT | DT_TOP | DT_WORDBREAK);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_KEYDOWN: {
            // Secret override to destroy the window during testing (Ctrl+Shift+Q)
            if (wParam == 'Q' && (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000)) {
                DestroyWindow(hwnd);
            }
            return 0;
        }
        case WM_CLOSE: {
            // Intercept WM_CLOSE to block Alt+F4 closures
            // Returning 0 indicates we have handled the message and the window should not be closed.
            return 0;
        }
        case WM_DESTROY: {
            KillTimer(hwnd, ID_EXERCISE_TIMER);
            g_hOverlayWindow = NULL;
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
