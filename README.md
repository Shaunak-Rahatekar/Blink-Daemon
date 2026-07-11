# Blink Daemon

Blink Daemon is a native Windows application built in C++ that acts as a lightweight break reminder to enforce the 20-20-20 rule for eye strain. It runs silently in the background as a system tray daemon and periodically forces a 5-minute eye exercise routine.

## Features (In Progress)

- **System Tray Daemon:** Runs entirely in the background via a hidden, message-only window with zero UI overhead.
- **Un-skippable Breaks:** When a break is triggered, the application creates a topmost, borderless fullscreen window to ensure the 5-minute routine cannot be bypassed or minimized.
- **Native Win32 API:** Built using pure Win32 API and Windows GDI for maximum performance and a minimal memory footprint.

## Technical Stack

- **Language:** C++17+
- **Framework:** Pure Win32 API
- **Graphics:** Windows GDI

## Documentation

As this project is being developed, theoretical documentation on the underlying OS-level concepts (such as the Windows Message Loop, Window Procedures, System Tray Communication, and GDI) are compiled into the [Blink_Daemon_Docs.md](Blink_Daemon_Docs.md) file. This serves as a deep-dive learning resource for native Windows development.

## How to Build

*(Compilation instructions will be added as the build system is finalized.)*
