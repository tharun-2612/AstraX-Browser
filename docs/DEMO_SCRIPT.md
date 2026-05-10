# Demo Script

Use this flow when showing AstraX to an interviewer or recording a short portfolio video.

## Two-Minute Walkthrough

1. Open AstraX and show the start dashboard with logo, quick links, clock, workspace status, and live internet location.
2. Search from the dashboard or address bar and show live suggestions plus AstraX Search ranking bookmarks/history before web fallback.
3. Open `Tools -> AstraX Engine Lab`, parse the sample HTML/CSS, select DOM nodes, show matched styles, and point out calculated layout boxes.
4. Open two or three tabs, then move and close one tab.
5. Click the page info chip or press `Ctrl+I` to show page details.
6. Bookmark a page with `Ctrl+D`, then open the Library sidebar with `Ctrl+B`.
7. Visit a few pages and show History inside the Library sidebar.
8. Open Settings, change the homepage or search URL, and toggle tracker blocking.
9. Close and reopen the browser to show session restore.
10. Press `Ctrl+F` and search inside a page.
11. Download a small file and show the Downloads tab in the Library sidebar.
12. Press `F12` to open Developer Tools.
13. Open a private window with `Ctrl+Shift+N` and explain the separate in-memory profile.

## Resume Bullets

- Built a C++20 desktop browser using Qt 6 WebEngine with tabbed browsing, smart URL/search handling, persistent bookmarks, browsing history, session restore, downloads, private mode, and Developer Tools.
- Designed a modern Qt Widgets browser shell with styled tabs, a page information chip, and a Library sidebar for bookmarks, history, and downloads.
- Designed JSON-backed storage for settings, sessions, bookmarks, and history with duplicate compaction and atomic writes using `QSaveFile`.
- Added a profile-level request interceptor to block common tracker/ad requests before network loading.
- Structured the application into focused UI, storage, and URL-resolution modules with CMake build presets and Linux CI.

## Interview Talking Points

- I used Qt WebEngine because browser rendering is a deep systems problem; the engineering value here is product architecture around a real engine.
- Private mode uses a separate off-the-record `QWebEngineProfile`, disables persistent cookies, uses memory cache, and pauses history writes.
- Bookmarks and history are saved as structured JSON and written atomically to avoid corrupting user data.
- New-window requests from web pages are routed into new browser tabs through a small `BrowserView` extension.
- Search and homepage behavior are user-configurable through a settings dialog.
