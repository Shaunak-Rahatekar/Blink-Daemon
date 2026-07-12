# Blink Daemon Documentation

## Blink Daemon Docs: The System Tray Foundation & Event Architecture

To understand how native Windows applications operate, you must first understand the beating heart of the operating system: **The Message Loop**. 

Unlike console applications that run linearly from top to bottom (via `main`), a Windows GUI application operates asynchronously through an event-driven architecture. The OS communicates with your application by dispatching "Messages" (represented by the `MSG` struct and integers starting with `WM_`).

### 1. The Message Loop (`GetMessage` / `DispatchMessage`)

When you create a thread that owns a window (or in our case, calls user interface APIs like `CreateWindowEx`), Windows automatically provisions a **Message Queue** specifically for that thread.

- `GetMessage()`: This function polls the queue. Crucially, if the queue is empty, `GetMessage()` puts the thread to sleep, yielding CPU time back to the OS. It wakes up and returns `> 0` only when a new message arrives (e.g., mouse movement, keyboard press, system shutdown alert). This is what makes native applications highly efficient—they consume 0% CPU when idle.
- `TranslateMessage()`: Translates raw virtual-key strokes into character messages (e.g., converting a raw "Shift + A" event into a `WM_CHAR` message representing a capital 'A').
- `DispatchMessage()`: This function tells the OS to invoke the specific `WindowProc` (Window Procedure) associated with the `HWND` (Window Handle) the message was targeted at.

The loop only breaks when `GetMessage()` receives a `WM_QUIT` message (which causes it to return `0`).

### 2. Message-Only Windows (`HWND_MESSAGE`)

Normally, creating a window results in a physical rendering context on the desktop. However, for background services like Blink Daemon, drawing a window is completely unnecessary and wastes resources.

By passing `HWND_MESSAGE` as the parent window during `CreateWindowEx`, you instruct the Windows Object Manager to create a **Message-Only Window**. 
- It has no Z-order.
- It cannot be enumerated or made visible.
- It does not receive system-wide broadcast messages.
- It **only** exists to own a Window Procedure (`WindowProc`) to send and receive targeted messages.

This is the perfect lightweight anchor for a background daemon. We need an `HWND` to handle the system tray callbacks, and a message-only window provides exactly that with zero visual overhead.

### 3. System Tray Communication (`Shell_NotifyIcon`)

The "System Tray" (officially known as the Taskbar Notification Area) is managed by `explorer.exe` (The Windows Shell). To place an icon there, we don't draw it ourselves; we ask `explorer.exe` to host it for us using `Shell_NotifyIcon()`.

We pass an `NOTIFYICONDATA` struct to the OS. Let's break down the critical flags:
- `NIF_ICON`: We are providing an `hIcon` to display.
- `NIF_TIP`: We are providing a tooltip string (displayed when hovering).
- `NIF_MESSAGE`: **The most important flag**. This sets up a two-way communication channel. We tell the Shell, "Whenever a user interacts with this icon (clicks, hovers), send a message to my `hWnd` using this `uCallbackMessage` identifier."

We defined our callback message as `WM_USER_TRAY_ICON` (`WM_USER + 1`). `WM_USER` is the starting point reserved by Windows for application-specific messages.

When the user right-clicks our icon, `explorer.exe` intercepts the click and posts `WM_USER_TRAY_ICON` to our message-only window. Inside `WindowProc`, we check the `lParam` (which `explorer.exe` populates with the mouse event, like `WM_RBUTTONUP`), generate our context menu using `CreatePopupMenu`, and use `TrackPopupMenu` to block and wait for a selection.

> **Crucial Quirk (The Foreground Window Hack):** Notice the call to `SetForegroundWindow(hwnd)` right before `TrackPopupMenu`. If you don't do this, a known bug/feature in Windows causes the popup menu to remain stuck on the screen forever, even if the user clicks away. By briefly setting our hidden window as the foreground focus, the OS knows to accurately dismiss the menu when focus is lost.

## Module 2: The Settings Dialog & Win32 UI Elements

Now that we have our invisible message-only window and system tray icon, we need a way for the user to configure the daemon. We will programmatically create a standard window to act as our settings dialog. 

### 1. Creating Controls (Child Windows)

In Win32, UI elements like buttons, checkboxes, and text boxes are actually just **windows** themselves! They are predefined window classes provided by the OS (such as `BUTTON`, `STATIC`, and `EDIT`). 

To create these elements, we use the same `CreateWindow` or `CreateWindowEx` function that we use for a main window, but we provide specific styles:

- `WS_CHILD`: Indicates this window is a child element and must be contained within a parent window.
- `WS_VISIBLE`: Ensures the control is visible as soon as it's created.
- **Specific Control Styles**: 
  - `BS_AUTOCHECKBOX`: A button style that creates a checkbox which toggles its state automatically when clicked.
  - `BS_DEFPUSHBUTTON`: Creates a standard push button with a heavy border, indicating it's the default action.
  - `ES_NUMBER`: An edit control style that only accepts numerical input.

When creating these child windows, the `hWndParent` parameter is set to our settings window handle.

### 2. Control IDs and `HMENU`

How do we identify which button was clicked or which text box to read from? When calling `CreateWindow` for a child window, the `hMenu` parameter is cleverly repurposed. Instead of a menu handle, we pass a unique integer identifier—the **Control ID**.

```cpp
#define ID_SETTINGS_SAVE_BTN 2003
// ...
CreateWindow(L"BUTTON", L"Save", WS_VISIBLE | WS_CHILD, x, y, w, h, hwnd, (HMENU)ID_SETTINGS_SAVE_BTN, hInstance, NULL);
```

We can later use this ID to interact with the control, such as retrieving it with `GetDlgItem(hwnd, ControlID)`.

### 3. Routing `WM_COMMAND` Messages

When a user interacts with a UI control (e.g., clicking a button or typing in a text box), the control generates an event. Because the control is a child window, it doesn't process this event entirely by itself; it notifies its parent window by sending a `WM_COMMAND` message.

In our settings window procedure (`SettingsWindowProc`), we intercept the `WM_COMMAND` message to handle user actions:

- **`LOWORD(wParam)`**: Contains the Control ID of the element that fired the event (e.g., `ID_SETTINGS_SAVE_BTN`).
- **`HIWORD(wParam)`**: Contains the specific notification code (e.g., `BN_CLICKED` for a button click, or `EN_CHANGE` for text edits).
- **`lParam`**: Contains the window handle (`HWND`) of the control itself.

When the save button is clicked, our switch statement in `WM_COMMAND` catches `ID_SETTINGS_SAVE_BTN`. We then use `SendMessage` to query the checkbox's state (`BM_GETCHECK`) and `GetWindowText` to read the interval text box, updating our global state variables before dismissing the dialog with `DestroyWindow`.

## Module 3: The Background Timer Logic

To enforce the 20-20-20 rule, Blink Daemon must be able to wake up periodically and interrupt the user. In the Win32 API, the standard mechanism for this is the `SetTimer` function.

### 1. The `SetTimer` Function

```cpp
#define ID_WORK_TIMER 3001
SetTimer(g_hMainWindow, ID_WORK_TIMER, g_workIntervalMinutes * 60 * 1000, NULL);
```

The `SetTimer` function tells the OS to generate an event for a specific window at a specified interval.
- **Window Handle (`hwnd`)**: We pass `g_hMainWindow` (our message-only window) so that the timer events are routed to our `WindowProc`.
- **Timer ID (`nIDEvent`)**: An arbitrary integer used to identify the timer. If an application uses multiple timers, the ID lets you distinguish between them. It is also used when calling `KillTimer(hwnd, ID_WORK_TIMER)` to pause or reset the countdown.
- **Elapse Time (`uElapse`)**: The timer interval in milliseconds. We multiply the user's minute setting by 60,000.
- **TimerProc (`lpTimerFunc`)**: Instead of supplying a callback function, we pass `NULL`, which tells the OS to post a `WM_TIMER` message to the associated window queue instead.

### 2. Handling `WM_TIMER`

When the interval elapses, a `WM_TIMER` message is dispatched to our `WindowProc`. We intercept it like this:

```cpp
case WM_TIMER: {
    if (wParam == ID_WORK_TIMER) {
        MessageBox(NULL, L"Time to take a 20-second break! Look at something 20 feet away.", 
                   L"Blink Daemon - 20-20-20 Rule", 
                   MB_OK | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);
    }
    return 0;
}
```

The `wParam` parameter holds the Timer ID, allowing us to verify that this is indeed our `ID_WORK_TIMER` firing before displaying the MessageBox alert.

### 3. `WM_TIMER` is a Low-Priority Message

It is important to understand a quirk of Windows timers: **They are not real-time accurate.**

In the Windows Message Queue, `WM_TIMER` is considered a low-priority message. `GetMessage` will only retrieve a `WM_TIMER` message if there are **no other messages** in the thread's queue. If the application is busy processing mouse movements or keyboard input, the timer message will be delayed. Furthermore, if a timer expires multiple times while the application is busy, the OS consolidates them into a single `WM_TIMER` message to prevent flooding the queue.

For an application like Blink Daemon, this slight inaccuracy is perfectly acceptable. However, for a high-performance game or audio processing engine, Win32 timers (`SetTimer`) are entirely insufficient, and developers must use higher-resolution multimedia timers instead.

## Module 4: The Lockdown Overlay

A simple message box isn't enough to force a user to take a break. In this module, we replace it with a fullscreen, borderless overlay that seizes control of the screen, locks out immediate bypass attempts (like Alt+F4), and requires the user to wait out the 20 seconds. 

### 1. Window Styles vs. Extended Styles

When calling `CreateWindowEx`, you configure the window's visual and behavioral traits using bitwise flags. There are two primary categories of flags:

- **Window Styles (`dwStyle`)**: Defines fundamental architectural properties of the window. 
  - We use `WS_POPUP` instead of `WS_OVERLAPPEDWINDOW`. A popup window has no title bar, no system menu, and no borders—perfect for a fullscreen canvas.
  - `WS_VISIBLE` ensures the window is painted immediately upon creation without needing a separate `ShowWindow` call.
- **Extended Window Styles (`dwExStyle`)**: Defines deeper, OS-level integration behaviors.
  - `WS_EX_TOOLWINDOW`: This flag prevents the window from showing up in the Alt+Tab menu or creating a separate button on the Taskbar. It acts as an auxiliary surface, maintaining the daemon's stealthy profile.

### 2. Windows Z-Order and the Topmost Flag

The "Z-Order" is the three-dimensional stack of windows on the desktop (X and Y being the screen coordinates, Z being depth). By default, the window with current focus is brought to the top of the standard Z-order stack.

However, we need our break overlay to display above *everything*—even if the user clicks a different application on a second monitor. 

We achieve this with the `WS_EX_TOPMOST` extended style. This places our window in a completely separate, superior Z-order band reserved for "Topmost" windows. A topmost window will always render above non-topmost windows, regardless of which window holds the keyboard focus.

### 3. Intercepting `WM_CLOSE` to Block Alt+F4

When a user attempts to close a window (by clicking the 'X', right-clicking the taskbar, or pressing Alt+F4), the OS posts a `WM_CLOSE` message to the window's queue.

If your Window Procedure passes `WM_CLOSE` down to `DefWindowProc` (the default behavior), the OS automatically calls `DestroyWindow` and tears down the UI.

To create a "lockdown" effect, we intercept the `WM_CLOSE` message inside `OverlayWindowProc`.

```cpp
case WM_CLOSE: {
    // Returning 0 indicates we have processed the message manually.
    // The default destruction sequence is aborted.
    return 0; 
}
```

By intercepting the message and returning `0`, we effectively nullify the user's attempt to close the window. The only way the window can be destroyed now is if our application code explicitly calls `DestroyWindow(hwnd)`.

For debugging purposes, we included a backdoor in the `WM_KEYDOWN` handler. By checking `wParam == 'Q'` and utilizing `GetKeyState` for the Control and Shift modifiers, we allow developers to forcibly destroy the window using Ctrl+Shift+Q.

## Module 5: GDI Rendering and the State Machine

To guide the user through the eye exercises, we divide our overlay window into a split-screen design and dynamically update the instructions using a 5-step state machine. We draw the interface using the Windows Graphics Device Interface (GDI).

### 1. The Graphics Device Interface (GDI)

GDI is the core, legacy graphics rendering system in Windows. It provides functions to draw lines, curves, closed figures, and text onto a display or printer.

To draw anything in Win32, you must obtain a **Device Context (HDC)**. A device context is an abstraction layer that links your C++ code to the physical drawing surface (in this case, the overlay window).

### 2. The `WM_PAINT` Lifecycle

Windows applications do not constantly redraw themselves in a while-loop like video games do. Instead, the OS sends a `WM_PAINT` message whenever a portion of the window becomes "invalid" (e.g., when the window is first created, resized, or brought to the foreground).

Inside `WM_PAINT`, you must follow a strict ritual:

1. **`BeginPaint`**: This function prepares the window for painting. It gives you the `HDC` and a `PAINTSTRUCT` containing information about which specific area of the window needs to be redrawn (the `rcPaint` rectangle).
2. **Drawing Calls**: We use `FillRect` with our custom `HBRUSH` objects to color the screen, and `DrawText` to render our current instruction based on `g_exerciseStep`.
3. **`EndPaint`**: This is critical. It releases the device context and tells Windows that the invalid area has been resolved. Failing to call `EndPaint` will cause the OS to continuously spam `WM_PAINT` messages, freezing your application.

### 3. Memory Management and `DeleteObject`

A common source of memory leaks in Win32 applications is mishandling GDI objects (like brushes, pens, and fonts). 

When we call `CreateSolidBrush()`, Windows allocates memory in the kernel space for that brush. When we are finished painting with it, we *must* call `DeleteObject()` to free that memory.

```cpp
HBRUSH hBrushBlack = CreateSolidBrush(RGB(0, 0, 0));
FillRect(hdc, &rcLeft, hBrushBlack);
DeleteObject(hBrushBlack); // CRITICAL: Free the GDI object
```

### 4. The Exercise State Machine

We initialized a secondary timer (`ID_EXERCISE_TIMER`) running at 60-second intervals. Every time it ticks, `g_exerciseStep` advances.

If the step is 5 or below, we call `InvalidateRect(hwnd, NULL, TRUE)`. This function is how you manually force a window to redraw. It marks the entire window client area as invalid, prompting the OS to send a fresh `WM_PAINT` message. During the resulting `WM_PAINT`, the `switch` statement reads the newly advanced `g_exerciseStep` and draws the updated instruction.

Once the exercise sequence hits step 6, the timer's logic branches, automatically calling `DestroyWindow` to tear down the lockdown overlay and return the user to their work.
