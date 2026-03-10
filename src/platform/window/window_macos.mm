#include "platform/macos/internal.h"

void pwCreateWindow(String title, int width, int height) {
    @autoreleasepool {
        if (mac_state.initialized) {
            pwDestroyWindow();
        }

        mac_state.application = [NSApplication sharedApplication];
        [mac_state.application
            setActivationPolicy:NSApplicationActivationPolicyRegular];
        [mac_state.application finishLaunching];

        mac_state.resizable = true;
        NSRect rect = NSMakeRect(0, 0, width, height);
        mac_state.window = [[NSWindow alloc]
            initWithContentRect:rect
                      styleMask:pwGetWindowStyleMacOS()
                        backing:NSBackingStoreBuffered
                          defer:NO];
        if (mac_state.window == nil) {
            pFail("Failed to create macOS window.");
        }

        mac_state.delegate = [[CppGamingWindowDelegate alloc] init];
        [mac_state.window setDelegate:mac_state.delegate];
        [mac_state.window setTitle:pwToNSStringMacOS(
            title.size > 0 ? title : String::lit("cpp-gaming")
        )];

        mac_state.width = width;
        mac_state.height = height;
        mac_state.initialized = true;
        mac_state.should_close = false;
        mac_state.visible = false;
    }
}

void pwDestroyWindow(void) {
    @autoreleasepool {
        if (!mac_state.initialized) {
            return;
        }

        if (mac_state.window != nil) {
            [mac_state.window setDelegate:nil];
            [mac_state.window close];
            mac_state.window = nil;
        }
        if (mac_state.delegate != nil) {
            [mac_state.delegate release];
            mac_state.delegate = nil;
        }

        mac_state = {};
    }
}

bool pwShouldWindowClose(void) {
    return mac_state.should_close;
}

void pwPollEvents(void) {
    @autoreleasepool {
        if (!mac_state.initialized || mac_state.application == nil) {
            return;
        }

        for (;;) {
            NSEvent* event = [mac_state.application
                nextEventMatchingMask:NSEventMaskAny
                            untilDate:[NSDate distantPast]
                               inMode:NSDefaultRunLoopMode
                              dequeue:YES];
            if (event == nil) {
                break;
            }

            [mac_state.application sendEvent:event];
        }

        [mac_state.application updateWindows];
    }
}

void pwSetWindowTitle(String title) {
    @autoreleasepool {
        if (mac_state.window != nil) {
            [mac_state.window setTitle:pwToNSStringMacOS(title)];
        }
    }
}

void pwGetWindowSize(int* width, int* height) {
    if (width) {
        *width = mac_state.width;
    }
    if (height) {
        *height = mac_state.height;
    }
}

void pwSetWindowSize(int width, int height) {
    @autoreleasepool {
        if (mac_state.window == nil) {
            return;
        }

        NSRect frame = [mac_state.window frameRectForContentRect:
            NSMakeRect(0, 0, width, height)];
        [mac_state.window setFrame:frame display:YES];
        mac_state.width = width;
        mac_state.height = height;
    }
}

void pwSetWindowResizable(bool resizable) {
    @autoreleasepool {
        mac_state.resizable = resizable;
        if (mac_state.window == nil) {
            return;
        }

        NSUInteger style = [mac_state.window styleMask];
        if (resizable) {
            style |= NSWindowStyleMaskResizable;
        } else {
            style &= (u32)~NSWindowStyleMaskResizable;
        }
        [mac_state.window setStyleMask:style];
    }
}

void pwPresentWindow(void) {
    @autoreleasepool {
        if (mac_state.window != nil) {
            [mac_state.window displayIfNeeded];
        }
    }
}

void pwShowWindow(void) {
    @autoreleasepool {
        if (!mac_state.initialized || mac_state.visible ||
            mac_state.window == nil) {
            return;
        }

        [mac_state.window makeKeyAndOrderFront:nil];
        [mac_state.application activateIgnoringOtherApps:YES];
        mac_state.visible = true;
    }
}
