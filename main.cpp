#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shellapi.h>
#include <stdlib.h> // For _wtoi
#include <string.h> // For memcmp
#include <time.h>   // For rand seed

// Define the display state GUID manually to fix MinGW/GCC linker errors
const GUID CUSTOM_GUID_CONSOLE_DISPLAY_STATE = { 0x271A8220, 0xA2BD, 0x4F9D, { 0x83, 0x40, 0x0B, 0xA4, 0x20, 0xF9, 0xB2, 0xDB } };

// Custom message identifier for system tray interactions
#define WM_USER_TRAY_ICON (WM_USER + 1)

// Menu identifiers
#define ID_TRAY_EXIT 1001
#define ID_TRAY_SETTINGS 1002

// Settings Window Control IDs
#define ID_SETTINGS_ENABLE_CHK 2001
#define ID_SETTINGS_INTERVAL_TXT 2002
#define ID_SETTINGS_SAVE_BTN 2003
#define ID_SETTINGS_STARTUP_CHK 2004

// Timer IDs
#define ID_WORK_TIMER 3001
#define ID_EXERCISE_TIMER 3002
#define ID_EVASION_TIMER 3003
#define ID_UI_TIMER 3004

// Overlay Window Control IDs
#define ID_OVERLAY_TERMINATE_BTN 4001

// Math Window Control IDs
#define ID_MATH_OK_BTN 5001
#define ID_MATH_INPUT_TXT 5002

// App Icon Resource ID
#define IDI_APPICON 101

// Global variables
NOTIFYICONDATA nid = {};
bool g_isDaemonEnabled = true;
int g_workIntervalMinutes = 20;
HWND g_hSettingsWindow = NULL;
HWND g_hMainWindow = NULL;
HWND g_hOverlayWindow = NULL;
int g_exerciseStep = 1;
int g_buttonJumps = 0;
DWORD g_stepStartTime = 0;
DWORD g_blackoutStartTime = 0;

// Power State Management
DWORD g_timerStartTime = 0;
int g_remainingTimeMs = 0;
bool g_isTimerPaused = false;

// Math Challenge State
int g_mathNum1 = 0;
int g_mathNum2 = 0;
HWND g_hMathWindow = NULL;

// Daily Stats
int GetCurrentDateInt() {
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    return (tm->tm_year + 1900) * 10000 + (tm->tm_mon + 1) * 100 + tm->tm_mday;
}

void LoadStats(int& completedToday, int& abortedToday) {
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\BlinkDaemon", 0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD date = 0;
        DWORD size = sizeof(DWORD);
        if (RegQueryValueEx(hKey, L"LastDate", NULL, NULL, (LPBYTE)&date, &size) == ERROR_SUCCESS) {
            if (date == GetCurrentDateInt()) {
                size = sizeof(DWORD);
                RegQueryValueEx(hKey, L"CompletedToday", NULL, NULL, (LPBYTE)&completedToday, &size);
                size = sizeof(DWORD);
                RegQueryValueEx(hKey, L"AbortedToday", NULL, NULL, (LPBYTE)&abortedToday, &size);
            } else {
                completedToday = 0;
                abortedToday = 0;
            }
        } else {
            completedToday = 0;
            abortedToday = 0;
        }
        RegCloseKey(hKey);
    }
}

void IncrementStat(bool completed) {
    int completedToday = 0;
    int abortedToday = 0;
    LoadStats(completedToday, abortedToday);
    
    if (completed) completedToday++;
    else abortedToday++;

    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\BlinkDaemon", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD date = GetCurrentDateInt();
        RegSetValueEx(hKey, L"LastDate", 0, REG_DWORD, (const BYTE*)&date, sizeof(DWORD));
        RegSetValueEx(hKey, L"CompletedToday", 0, REG_DWORD, (const BYTE*)&completedToday, sizeof(DWORD));
        RegSetValueEx(hKey, L"AbortedToday", 0, REG_DWORD, (const BYTE*)&abortedToday, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

void StartWorkTimer(int remainingMs) {
    if (remainingMs <= 0) remainingMs = g_workIntervalMinutes * 60 * 1000;
    g_remainingTimeMs = remainingMs;
    g_timerStartTime = GetTickCount();
    SetTimer(g_hMainWindow, ID_WORK_TIMER, g_remainingTimeMs, NULL);
    g_isTimerPaused = false;
}

void PauseWorkTimer() {
    if (!g_isTimerPaused) {
        KillTimer(g_hMainWindow, ID_WORK_TIMER);
        DWORD elapsed = GetTickCount() - g_timerStartTime;
        g_remainingTimeMs -= elapsed;
        if (g_remainingTimeMs < 0) g_remainingTimeMs = 0;
        g_isTimerPaused = true;
    }
}


// Window Procedure forward declaration
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK OverlayWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MathWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

bool GetRunOnStartup() {
    HKEY hKey;
    const wchar_t* keyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    bool enabled = false;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, keyPath, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx(hKey, L"BlinkDaemon", NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            enabled = true;
        }
        RegCloseKey(hKey);
    }
    return enabled;
}

void SetRunOnStartup(bool enable) {
    HKEY hKey;
    const wchar_t* keyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    if (RegOpenKeyEx(HKEY_CURRENT_USER, keyPath, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t exePath[MAX_PATH];
            GetModuleFileName(NULL, exePath, MAX_PATH);
            RegSetValueEx(hKey, L"BlinkDaemon", 0, REG_SZ, (const BYTE*)exePath, (DWORD)((wcslen(exePath) + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValue(hKey, L"BlinkDaemon");
        }
        RegCloseKey(hKey);
    }
}

// Entry point for Windows GUI applications
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    srand((unsigned int)time(NULL));

    int result = MessageBox(NULL, L"Do you want to start Blink Daemon?", L"Startup Confirmation", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
    if (result == IDNO) {
        return 0;
    }

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
    settingsWc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    if (!RegisterClass(&settingsWc)) {
        return 0;
    }

    // Register Overlay Window Class
    WNDCLASS overlayWc = {};
    overlayWc.lpfnWndProc = OverlayWindowProc;
    overlayWc.hInstance = hInstance;
    overlayWc.lpszClassName = L"BlinkDaemonOverlayClass";
    overlayWc.hCursor = LoadCursor(NULL, IDC_ARROW);
    overlayWc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    if (!RegisterClass(&overlayWc)) {
        return 0;
    }

    // Register Math Challenge Window Class
    WNDCLASS mathWc = {};
    mathWc.lpfnWndProc = MathWindowProc;
    mathWc.hInstance = hInstance;
    mathWc.lpszClassName = L"BlinkDaemonMathChallengeClass";
    mathWc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    mathWc.hCursor = LoadCursor(NULL, IDC_ARROW);
    mathWc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    if (!RegisterClass(&mathWc)) {
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

    // Register for display state power notifications
    RegisterPowerSettingNotification(hwnd, &CUSTOM_GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_WINDOW_HANDLE);

    // 3. Initialize and Add the System Tray Icon
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1; // Unique ID for our icon
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER_TRAY_ICON;
    // Load the custom logo from the embedded resource
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    // Fallback if the resource is missing
    if (nid.hIcon == NULL) {
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    
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
        StartWorkTimer(0);
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
                // Kill repeating timer to avoid triggering early next time
                KillTimer(hwnd, ID_WORK_TIMER);
                
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
                
                // Restart for the next full interval
                StartWorkTimer(g_workIntervalMinutes * 60 * 1000);
            }
            return 0;
        }

        case WM_POWERBROADCAST: {
            if (wParam == PBT_POWERSETTINGCHANGE) {
                POWERBROADCAST_SETTING* pbs = (POWERBROADCAST_SETTING*)lParam;
                if (memcmp(&pbs->PowerSetting, &CUSTOM_GUID_CONSOLE_DISPLAY_STATE, sizeof(GUID)) == 0) {
                    if (pbs->Data[0] == 0) { // Display off
                        PauseWorkTimer();
                    } else if (pbs->Data[0] == 1) { // Display on
                        if (g_isDaemonEnabled && g_isTimerPaused) {
                            StartWorkTimer(g_remainingTimeMs);
                        }
                    }
                }
            } else if (wParam == PBT_APMSUSPEND) {
                PauseWorkTimer();
            } else if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
                if (g_isDaemonEnabled && g_isTimerPaused) {
                    StartWorkTimer(g_remainingTimeMs);
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

            // Run on Startup Checkbox
            HWND hStartupChk = CreateWindow(L"BUTTON", L"Run on Windows Startup",
                WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                20, 45, 200, 20,
                hwnd, (HMENU)ID_SETTINGS_STARTUP_CHK, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            SendMessage(hStartupChk, BM_SETCHECK, GetRunOnStartup() ? BST_CHECKED : BST_UNCHECKED, 0);

            // Interval Label
            CreateWindow(L"STATIC", L"Work Interval (minutes):",
                WS_VISIBLE | WS_CHILD,
                20, 75, 150, 20,
                hwnd, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

            // Interval TextBox
            HWND hTxt = CreateWindow(L"EDIT", L"",
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
                180, 75, 50, 20,
                hwnd, (HMENU)ID_SETTINGS_INTERVAL_TXT, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            
            wchar_t buffer[10];
            wsprintf(buffer, L"%d", g_workIntervalMinutes);
            SetWindowText(hTxt, buffer);

            // Medical Text Block
            CreateWindow(L"STATIC", L"Medical Information:\nExtended periods of screen time can cause digital eye strain. It is recommended to follow the 20-20-20 rule. Every 20 minutes, take a 20-second break and focus your eyes on something at least 20 feet away.",
                WS_VISIBLE | WS_CHILD,
                20, 115, 340, 80,
                hwnd, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

            // Save Button
            CreateWindow(L"BUTTON", L"Save",
                WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                150, 215, 80, 30,
                hwnd, (HMENU)ID_SETTINGS_SAVE_BTN, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

            return 0;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_SETTINGS_SAVE_BTN) {
                // Read Checkbox state
                HWND hChk = GetDlgItem(hwnd, ID_SETTINGS_ENABLE_CHK);
                g_isDaemonEnabled = (SendMessage(hChk, BM_GETCHECK, 0, 0) == BST_CHECKED);

                // Read Startup Checkbox state
                HWND hStartupChk = GetDlgItem(hwnd, ID_SETTINGS_STARTUP_CHK);
                bool runOnStartup = (SendMessage(hStartupChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SetRunOnStartup(runOnStartup);

                // Read TextBox state
                HWND hTxt = GetDlgItem(hwnd, ID_SETTINGS_INTERVAL_TXT);
                wchar_t buffer[10];
                GetWindowText(hTxt, buffer, 10);
                g_workIntervalMinutes = _wtoi(buffer);
                if (g_workIntervalMinutes <= 0) g_workIntervalMinutes = 1; // Basic validation

                // Apply Timer Settings
                PauseWorkTimer();
                if (g_isDaemonEnabled) {
                    StartWorkTimer(g_workIntervalMinutes * 60 * 1000);
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
            // Start evasion timer for the terminate button (50ms)
            SetTimer(hwnd, ID_EVASION_TIMER, 50, NULL);
            // Start UI update timer for the countdown (1 second)
            SetTimer(hwnd, ID_UI_TIMER, 1000, NULL);
            g_buttonJumps = 0;
            g_stepStartTime = GetTickCount();
            g_blackoutStartTime = GetTickCount();
            
            // Play a sound to notify the user that a break has started
            MessageBeep(MB_OK);
            
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
                                    g_mathNum1 = rand() % 10;
                                    g_mathNum2 = rand() % 10;
                                    if (g_hMathWindow == NULL) {
                                        g_hMathWindow = CreateWindowEx(
                                            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                                            L"BlinkDaemonMathChallengeClass",
                                            L"Math Challenge",
                                            WS_POPUP | WS_VISIBLE | WS_BORDER,
                                            GetSystemMetrics(SM_CXSCREEN) / 2 - 150,
                                            GetSystemMetrics(SM_CYSCREEN) / 2 - 100,
                                            300, 200,
                                            hwnd, NULL, GetModuleHandle(NULL), NULL
                                        );
                                        EnableWindow(hwnd, FALSE); // Make it modal
                                    }
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
                g_stepStartTime = GetTickCount();
                if (g_exerciseStep > 5) {
                    IncrementStat(true);
                    MessageBeep(MB_ICONASTERISK); // Celebratory / completion sound
                    DestroyWindow(hwnd); // Exercise complete
                } else {
                    MessageBeep(MB_OK); // Step change sound
                    InvalidateRect(hwnd, NULL, TRUE); // Force a repaint for the next step
                }
            } else if (wParam == ID_UI_TIMER) {
                InvalidateRect(hwnd, NULL, TRUE); // Update countdown timer
            } else if (wParam == ID_EVASION_TIMER) {
                if (g_buttonJumps < 10) {
                    HWND hBtn = GetDlgItem(hwnd, ID_OVERLAY_TERMINATE_BTN);
                    if (hBtn) {
                        POINT pt;
                        GetCursorPos(&pt); // Screen coordinates

                        RECT btnRect;
                        GetWindowRect(hBtn, &btnRect); // Screen coordinates

                        // Inflate the rectangle by 50 pixels to create a proximity zone
                        InflateRect(&btnRect, 50, 50);

                        if (PtInRect(&btnRect, pt)) {
                            // Teleport!
                            RECT overlayRect;
                            GetClientRect(hwnd, &overlayRect);
                            
                            int btnW = 200;
                            int btnH = 40;
                            int maxX = overlayRect.right - overlayRect.left - btnW;
                            int maxY = overlayRect.bottom - overlayRect.top - btnH;
                            
                            int newX = (maxX > 0) ? (rand() % maxX) : 0;
                            int newY = (maxY > 0) ? (rand() % maxY) : 0;
                            
                            // SetWindowPos uses parent client coordinates for child windows
                            SetWindowPos(hBtn, NULL, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                            g_buttonJumps++;
                        }
                    }
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
            
            // Calculate remaining time
            DWORD elapsed = GetTickCount() - g_stepStartTime;
            int remaining = 60 - (elapsed / 1000);
            if (remaining < 0) remaining = 0;

            wchar_t timerText[16];
            wsprintf(timerText, L"00:%02d", remaining);

            // Draw the big timer
            HFONT hFontTimer = CreateFont(120, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Arial");
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFontTimer);

            RECT rcTimer = rcLeft;
            rcTimer.bottom = rcLeft.bottom / 2 - 20; // Place it just above the center
            DrawText(hdc, timerText, -1, &rcTimer, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);

            SelectObject(hdc, hOldFont);
            DeleteObject(hFontTimer);

            // Draw the instruction line centered
            DrawText(hdc, instruction, -1, &rcLeft, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            // Calculate total remaining blackout time (5 steps * 60 seconds = 300 seconds)
            DWORD totalElapsed = GetTickCount() - g_blackoutStartTime;
            int totalRemaining = 300 - (totalElapsed / 1000);
            if (totalRemaining < 0) totalRemaining = 0;

            int min = totalRemaining / 60;
            int sec = totalRemaining % 60;
            
            wchar_t totalTimerText[64];
            wsprintf(totalTimerText, L"Total Remaining Time: %02d:%02d", min, sec);
            
            // Draw the total timer below the instruction line
            HFONT hFontTotalTimer = CreateFont(36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Arial");
            HFONT hOldFont2 = (HFONT)SelectObject(hdc, hFontTotalTimer);

            RECT rcTotalTimer = rcLeft;
            rcTotalTimer.top = rcLeft.bottom / 2 + 40; // Place it below the center
            DrawText(hdc, totalTimerText, -1, &rcTotalTimer, DT_CENTER | DT_TOP | DT_SINGLELINE);

            SelectObject(hdc, hOldFont2);
            DeleteObject(hFontTotalTimer);

            // Draw encouraging message
            wchar_t encouragingMsg[128];
            wsprintf(encouragingMsg, L"Only %d min %d sec remaining. It's for your own good!", min, sec);
            
            HFONT hFontEncourage = CreateFont(24, 0, 0, 0, FW_NORMAL, TRUE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Arial");
            HFONT hOldFont3 = (HFONT)SelectObject(hdc, hFontEncourage);

            RECT rcEncourage = rcLeft;
            rcEncourage.top = rcLeft.bottom / 2 + 90; // Place it below the total timer
            DrawText(hdc, encouragingMsg, -1, &rcEncourage, DT_CENTER | DT_TOP | DT_SINGLELINE);

            SelectObject(hdc, hOldFont3);
            DeleteObject(hFontEncourage);
            
            // Draw Medical Information and Settings in the right pane
            RECT rcRightText = rcRight;
            rcRightText.left += 30;
            rcRightText.right -= 30;
            rcRightText.top += 50;
            rcRightText.bottom -= 50;
            
            int completedToday = 0;
            int abortedToday = 0;
            LoadStats(completedToday, abortedToday);

            wchar_t infoText[1024];
            wsprintf(infoText, L"DAILY STATS\n\n"
                               L"Breaks Completed Today: %d\n"
                               L"Breaks Skipped Today: %d\n\n\n"
                               L"MEDICAL INFORMATION\n\n"
                               L"Extended periods of screen time can cause digital eye strain. "
                               L"It is recommended to follow the 20-20-20 rule. Every 20 minutes, "
                               L"take a 20-second break and focus your eyes on something at least 20 feet away.\n\n\n"
                               L"SETTINGS\n\n"
                               L"To change the interval, right-click the Blink Daemon icon in the System Tray (bottom right of your screen) and select 'Settings...'.\n\n"
                               L"Press Ctrl+Shift+Q to exit this screen if needed.",
                               completedToday, abortedToday);
                                      
            DrawText(hdc, infoText, -1, &rcRightText, DT_LEFT | DT_TOP | DT_WORDBREAK);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_KEYDOWN: {
            // Secret override to destroy the window during testing (Ctrl+Shift+Q)
            if (wParam == 'Q' && (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000)) {
                IncrementStat(false);
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
            KillTimer(hwnd, ID_EVASION_TIMER);
            KillTimer(hwnd, ID_UI_TIMER);
            g_hOverlayWindow = NULL;
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK MathWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            wchar_t question[50];
            wsprintf(question, L"What is %d + %d?", g_mathNum1, g_mathNum2);

            CreateWindow(L"STATIC", question,
                WS_VISIBLE | WS_CHILD | SS_CENTER,
                50, 40, 200, 20,
                hwnd, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

            CreateWindow(L"EDIT", L"",
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER,
                100, 80, 100, 25,
                hwnd, (HMENU)ID_MATH_INPUT_TXT, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

            CreateWindow(L"BUTTON", L"OK",
                WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                100, 130, 100, 30,
                hwnd, (HMENU)ID_MATH_OK_BTN, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            return 0;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_MATH_OK_BTN) {
                HWND hTxt = GetDlgItem(hwnd, ID_MATH_INPUT_TXT);
                wchar_t buffer[10];
                GetWindowText(hTxt, buffer, 10);
                int answer = _wtoi(buffer);

                if (answer == (g_mathNum1 + g_mathNum2)) {
                    IncrementStat(false);
                    EnableWindow(g_hOverlayWindow, TRUE);
                    DestroyWindow(g_hOverlayWindow);
                    DestroyWindow(hwnd);
                } else {
                    MessageBox(hwnd, L"Incorrect answer! Termination cancelled.", L"Error", MB_OK | MB_ICONERROR | MB_TOPMOST);
                    EnableWindow(g_hOverlayWindow, TRUE);
                    DestroyWindow(hwnd);
                }
            }
            return 0;
        }
        case WM_CLOSE: {
            EnableWindow(g_hOverlayWindow, TRUE);
            DestroyWindow(hwnd);
            return 0;
        }
        case WM_DESTROY: {
            g_hMathWindow = NULL;
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

