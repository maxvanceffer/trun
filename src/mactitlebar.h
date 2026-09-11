#pragma once

class QWindow;

// Hides the system title bar so content extends to the very top
// (unified toolbar). Returns false when the native handle is not
// ready yet (caller should retry). No-op outside macOS.
// NOTE: __APPLE__, not Q_OS_MACOS (Qt macros aren't defined yet here).
#ifdef __APPLE__
bool hideSystemTitleBar(QWindow *window);
#else
inline bool hideSystemTitleBar(QWindow *) { return true; }
#endif
