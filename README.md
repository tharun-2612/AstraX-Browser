# AstraX Browser

AstraX is a portfolio-grade desktop web browser written in modern C++ with Qt 6 and Qt WebEngine. It uses Chromium through Qt WebEngine for standards-compliant rendering, then focuses the C++ work on application architecture: tabs, persistent data, downloads, keyboard-driven navigation, private mode, and clean separation between UI, URL handling, and storage.

This is the kind of project you can show in interviews because it demonstrates pragmatic engineering judgment: do not hand-roll a browser engine, but do build a capable product around a real engine.

## Features

- Multi-tab browsing with movable and closable tabs.
- Modern browser chrome with styled tabs, address bar, and page information chip.
- Advanced AstraX start dashboard with logo, search, quick links, live clock, workspace status, and live internet location.
- AstraX Search page with local bookmark/history ranking, live suggestions, and configurable web fallback.
- AstraX Engine Lab with custom HTML/CSS parsers, DOM tree viewer, style inspector, and block layout engine.
- Library sidebar for bookmarks, history, and downloads.
- Smart address bar that accepts URLs or search terms.
- Persistent bookmarks stored as JSON.
- Persistent browsing history with duplicate compaction.
- Session restore for previously open tabs.
- Settings dialog for homepage, search URL, session restore, and tracker blocking.
- Lightweight request blocker for common ad/tracker hosts.
- Private mode with in-memory cache/cookies and paused history writes.
- Downloads panel with progress and completion state.
- Page information dialog with URL, connection, profile, tracker-blocking, and zoom details.
- Find-in-page toolbar.
- Built-in Developer Tools window.
- Keyboard shortcuts for common browser workflows.
- Atomic file writes for bookmarks/history using `QSaveFile`.
- Educational browser-internals module for parser, DOM, style-matching, and layout experiments.
- CMake project structure suitable for Windows, Linux, and macOS.

## Tech Stack

- C++20
- Qt 6.5 or newer
- Qt Network
- Qt WebEngine Widgets
- CMake 3.24 or newer
- Ninja, MSVC, Clang, or GCC

## Build

Install Qt 6 with the **WebEngine** component first. On Windows, the easiest route is the Qt Online Installer plus the MSVC toolchain from Visual Studio Build Tools.

```powershell
cmake --preset debug -DCMAKE_PREFIX_PATH="C:\Qt\6.7.0\msvc2019_64"
cmake --build --preset debug
.\build\debug\astrax-browser.exe
```

On this Windows setup, you can use the helper script that loads the Visual Studio compiler environment and points CMake at the installed Qt:

```powershell
.\scripts\build-vs2022.bat
.\build\debug\astrax-browser.exe
```

If Windows reports missing DLLs after a release build, deploy the Qt runtime files:

```powershell
.\scripts\deploy-release.bat
.\scripts\run-release.bat
```

On Linux with distro Qt packages:

```bash
cmake --preset debug
cmake --build --preset debug
./build/debug/astrax-browser
```

Private mode:

```bash
./build/debug/astrax-browser --private
```

Run tests:

```bash
ctest --test-dir build/debug --output-on-failure
```

## Keyboard Shortcuts

- `Ctrl+L`: focus the address bar
- `Ctrl+T`: open a new tab
- `Ctrl+W`: close the current tab
- `Ctrl+D`: bookmark the current page
- `Ctrl+F`: find in page
- `Ctrl+J`: show downloads
- `Ctrl+B`: show the Library sidebar
- `Ctrl+I`: show page information
- `Ctrl+Shift+N`: open a private window
- `Alt+Left` / `Alt+Right`: back and forward
- `F12`: open Developer Tools

## Why This Project Works For A Job Portfolio

AstraX shows that you can work with a large third-party framework, design a maintainable desktop app, persist user state safely, and build real user workflows. The code is intentionally split into focused modules so an interviewer can review how you think:

- `src/core`: input parsing and URL/search resolution.
- `src/storage`: JSON-backed persistence for bookmarks, history, settings, and sessions.
- `src/ui`: Qt widgets, browser windows, tabs, downloads, and shortcuts.
- `tests`: Qt Test coverage for URL resolution and persistence behavior.

Good interview talking points:

- Why Qt WebEngine is the right abstraction instead of building a rendering engine.
- How private mode changes profile/cache/history behavior.
- How data is persisted atomically.
- How tab creation works for links that request a new window.
- How the Library sidebar keeps bookmarks, history, and downloads discoverable without separate windows.
- How the page information dialog reflects profile/security state.
- How `QWebEngineUrlRequestInterceptor` blocks known tracker requests before Chromium loads them.
- How session restore reconstructs tabs from a sanitized JSON session file.
- What you would add next: filter-list imports, certificate details, bookmark management, and packaged installers.

## Next Milestones

See [docs/ROADMAP.md](docs/ROADMAP.md) for feature ideas that would make this even stronger as a job-facing project.

For presenting the project, see [docs/DEMO_SCRIPT.md](docs/DEMO_SCRIPT.md).
