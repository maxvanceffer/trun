#import <Cocoa/Cocoa.h>

#include "macappmenu.h"
#include "updater.h"

#include <QMetaObject>

// Forwards the menu action into Qt (the QML root's openUpdateDialog()).
@interface TrunMenuTarget : NSObject {
    QObject *_openTarget;
}
- (instancetype)initWithOpenTarget:(QObject *)openTarget;
- (void)onCheckUpdates:(id)sender;
@end

@implementation TrunMenuTarget
- (instancetype)initWithOpenTarget:(QObject *)openTarget
{
    self = [super init];
    if (self)
        _openTarget = openTarget;
    return self;
}
- (void)onCheckUpdates:(id)sender
{
    (void)sender;
    QMetaObject::invokeMethod(_openTarget, "openUpdateDialog");
}
@end

static NSString *titleForUpdater(Updater *updater)
{
    if (updater->updateAvailable() && !updater->latestVersion().isEmpty()) {
        const QString text =
            QStringLiteral("Update to v%1…").arg(updater->latestVersion());
        return [NSString stringWithUTF8String:text.toUtf8().constData()];
    }
    return @"Check for Updates…";
}

void installMacAppMenu(QObject *openTarget, Updater *updater)
{
    NSMenu *appMenu = [[[NSApplication sharedApplication] mainMenu] itemAtIndex:0].submenu;
    if (!appMenu)
        return;

    TrunMenuTarget *target =
        [[TrunMenuTarget alloc] initWithOpenTarget:openTarget];
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:titleForUpdater(updater)
                                                 action:@selector(onCheckUpdates:)
                                          keyEquivalent:@""];
    [item setTarget:target];

    // Place directly above Quit (below its separator when there is one).
    NSInteger at = [appMenu indexOfItemWithTarget:nil andAction:@selector(terminate:)];
    if (at < 0)
        at = [appMenu numberOfItems];
    [appMenu insertItem:item atIndex:at];

    // Availability indicator: the menu item itself names the newer version.
    // Both live for the app lifetime; updater is a safe signal context.
    QObject::connect(updater, &Updater::stateChanged, updater, [item, updater]() {
        [item setTitle:titleForUpdater(updater)];
    });
}
