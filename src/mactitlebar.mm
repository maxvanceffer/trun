#import <Cocoa/Cocoa.h>

#include "mactitlebar.h"

#include <QWindow>
#include <QDebug>

bool hideSystemTitleBar(QWindow *window)
{
    if (!window)
        return false;
    // Native handle may not exist before the first show
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
    if (!view) {
        qWarning() << "[titlebar] no native view yet";
        return false;
    }
    NSWindow *nswindow = [view window];
    if (!nswindow) {
        qWarning() << "[titlebar] no NSWindow yet";
        return false;
    }
    // Untitled window: no titlebar area at all, so AppKit cannot reserve
    // 28pt for it and contentLayoutRect spans the full window.
    // (FullSizeContentView + transparent titlebar alone did not release it.)
    // Native traffic lights go away with the title — trun draws its own.
    nswindow.styleMask &= ~NSWindowStyleMaskTitled;
    nswindow.styleMask |= NSWindowStyleMaskFullSizeContentView;
    nswindow.titlebarAppearsTransparent = YES;
    nswindow.titleVisibility = NSWindowTitleHidden;
    nswindow.movableByWindowBackground = NO;
    nswindow.movable = YES;
    const NSRect layoutRect = [nswindow contentLayoutRect];
    const NSRect viewBounds = [view bounds];
    const NSRect windowFrame = [nswindow frame];
    qWarning() << "[titlebar] applied, fullSize:"
               << ((nswindow.styleMask & NSWindowStyleMaskFullSizeContentView) != 0)
               << "transparent:" << nswindow.titlebarAppearsTransparent
               << "mask:" << (unsigned long)nswindow.styleMask
               << "layoutY:" << layoutRect.origin.y << "layoutH:" << layoutRect.size.height
               << "viewH:" << viewBounds.size.height << "frameH:" << windowFrame.size.height;
    return true;
}
