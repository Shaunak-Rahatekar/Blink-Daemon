# Blink Daemon

Blink Daemon is a native Windows application built in C++ that acts as a lightweight background monitor to enforce the 20-20-20 rule for eye strain. It runs silently in the background as a system tray application and periodically forces a 5-minute eye exercise routine.

## Features

- **System Tray Daemon:** Runs entirely in the background via a hidden, message-only window. Access settings by right-clicking the icon in the system tray.
- **Un-skippable Breaks:** When a break is triggered, the application creates a topmost, borderless fullscreen window to ensure the 5-minute routine is completed.
- **Emergency Terminate:** Includes a strict, 5-step confirmation process if you absolutely must bypass the break during an emergency.
- **Native Win32 API:** Built using pure Win32 API and Windows GDI for maximum performance and an ultra-minimal memory footprint.

## Setup & Compilation

This project is built using native C++ and Windows APIs. It can be compiled using either MSVC or MinGW.

### Using MSVC (Visual Studio)
1. Open the **Developer Command Prompt for Visual Studio**.
2. Navigate to this directory.
3. Run the following command to compile and link against the necessary Windows libraries:
   ```cmd
   cl.exe /EHsc /W4 /DUNICODE /D_UNICODE main.cpp User32.lib Shell32.lib Gdi32.lib /link /SUBSYSTEM:WINDOWS /OUT:BlinkDaemon.exe
   ```

### Using MinGW / GCC
1. Open your MinGW/MSYS2 terminal (or PowerShell, if `g++` is in your PATH).
2. Navigate to this directory.
3. Run the following command:
   ```bash
   g++ main.cpp -o BlinkDaemon.exe -mwindows -municode
   ```
*(The `-mwindows` flag ensures it builds as a GUI application without opening a black console window.)*

## How to Use

1. Double-click the compiled `BlinkDaemon.exe` to launch the program.
2. The Settings Window will appear immediately upon launch. You can configure your break interval (default is 20 minutes) and enable/disable the daemon.
3. Click **Save**. The application will now run silently in the background. You will see its icon in the bottom right corner of your screen (the System Tray).
4. When the timer triggers, a fullscreen break overlay will appear, guiding you through eye exercises.
5. If you need to change settings or exit the program, right-click the icon in the System Tray and select **Settings...** or **Exit Blink Daemon**.

## Contributing & Pushing to GitHub

To push your changes to a GitHub repository, use the following standard git workflow:

1. Check the status of your changed files:
   ```bash
   git status
   ```
2. Stage all modifications (or specify individual files like `main.cpp`):
   ```bash
   git add .
   ```
3. Commit your changes with a descriptive message:
   ```bash
   git commit -m "Update UI and add new features"
   ```
4. Push to your remote repository:
   ```bash
   git push
   ```

## Documentation

For a deep dive into the native Win32 concepts used in this project (Message Loops, GDI, and System Tray APIs), refer to [Blink_Daemon_Docs.md](Blink_Daemon_Docs.md).
