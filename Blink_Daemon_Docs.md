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
