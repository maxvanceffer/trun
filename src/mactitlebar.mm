#import <Cocoa/Cocoa.h>

#include "mactitlebar.h"

#include <QWindow>

void hideSystemTitleBar(QWindow *window)
{
    if (!window)
        return;
    // Native handle may not exist before the first show
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
    if (!view)
        return;
    NSWindow *nswindow = [view window];
    if (!nswindow)
        return;
    nswindow.styleMask |= NSWindowStyleMaskFullSizeContentView;
    nswindow.titlebarAppearsTransparent = YES;
    nswindow.titleVisibility = NSWindowTitleHidden;
    nswindow.movableByWindowBackground = NO;
    nswindow.movable = YES;
}
