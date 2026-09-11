#pragma once

class QWindow;

// Hides the system title bar so content extends to the very top
// (unified toolbar). No-op outside macOS.
// NOTE: __APPLE__, not Q_OS_MACOS (Qt macros aren't defined yet here).
#ifdef __APPLE__
void hideSystemTitleBar(QWindow *window);
#else
inline void hideSystemTitleBar(QWindow *) {}
#endif
