#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include "mactitlebar.h"

#include <QWindow>
#include <QDebug>

// Click-through vibrancy view: shows the blur but never steals mouse events
@interface TrunVibrancyView : NSVisualEffectView
@end

@implementation TrunVibrancyView
- (NSView *)hitTest:(NSPoint)point
{
    return nil;
}
@end

bool hideSystemTitleBar(QWindow *window)
{
    if (!window)
        return false;
    // Native handle may not exist before the first show
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
    if (!view)
        return false;
    NSWindow *nswindow = [view window];
    if (!nswindow)
        return false;

    // Unified toolbar: content spans the full window. The TitleHidden
    // styleMask bit (not just the titleVisibility property) is what
    // actually releases the 28pt titlebar area.
    nswindow.styleMask |= NSWindowStyleMaskFullSizeContentView | NSWindowTitleHidden;
    nswindow.titlebarAppearsTransparent = YES;
    nswindow.titleVisibility = NSWindowTitleHidden;
    nswindow.movableByWindowBackground = NO;
    nswindow.movable = YES;

    // Sidebar vibrancy: glass behind Qt's scene, click-through.
    // Installed once; Qt content paints over it everywhere except
    // transparent regions (the sidebar strip).
    static char kVibrancyKey;
    if (!objc_getAssociatedObject(nswindow, &kVibrancyKey)) {
        NSView *container = [view superview];
        if (container) {
            [nswindow setOpaque:NO];
            [nswindow setBackgroundColor:NSColor.clearColor];
            TrunVibrancyView *effectView =
                [[TrunVibrancyView alloc] initWithFrame:view.frame];
            [effectView setMaterial:NSVisualEffectMaterialSidebar];
            [effectView setBlendingMode:NSVisualEffectBlendingModeBehindWindow];
            [effectView setState:NSVisualEffectStateFollowsWindowActiveState];
            [effectView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
            [container addSubview:effectView positioned:NSWindowBelow relativeTo:view];
            objc_setAssociatedObject(nswindow, &kVibrancyKey, effectView,
                                     OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            [effectView release];
        }
    }

    const NSRect layoutRect = [nswindow contentLayoutRect];
    qWarning() << "[titlebar] applied, layoutY:" << layoutRect.origin.y
               << "layoutH:" << layoutRect.size.height;
    return true;
}
