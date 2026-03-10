#include "platform/linux/internal.h"

void pwCreateWindow(String title, int width, int height) {
    if (linux_state.initialized) {
        pwDestroyWindow();
    }

    linux_state.connection = xcb_connect(nullptr, nullptr);
    if (!linux_state.connection ||
        xcb_connection_has_error(linux_state.connection) != 0) {
        LOG_FATAL("Failed to connect to the X server.");
        abort();
    }

    linux_state.screen = pwGetDefaultScreenLinux(linux_state.connection);
    if (!linux_state.screen) {
        xcb_disconnect(linux_state.connection);
        linux_state = {};
        LOG_FATAL("Failed to acquire the default X screen.");
        abort();
    }

    linux_state.window = xcb_generate_id(linux_state.connection);
    linux_state.width = width;
    linux_state.height = height;
    linux_state.resizable = true;

    u32 event_mask = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    u32 value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    u32 value_list[] = {linux_state.screen->black_pixel, event_mask};

    xcb_void_cookie_t create_cookie = xcb_create_window_checked(
        linux_state.connection,
        XCB_COPY_FROM_PARENT,
        linux_state.window,
        linux_state.screen->root,
        0,
        0,
        (u16)width,
        (u16)height,
        0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        linux_state.screen->root_visual,
        value_mask,
        value_list
    );

    xcb_generic_error_t* create_error =
        xcb_request_check(linux_state.connection, create_cookie);
    if (create_error) {
        free(create_error);
        xcb_disconnect(linux_state.connection);
        linux_state = {};
        LOG_FATAL("Failed to create the XCB window.");
        abort();
    }

    linux_state.wm_protocols = pwInternAtomLinux("WM_PROTOCOLS");
    linux_state.wm_delete_window = pwInternAtomLinux("WM_DELETE_WINDOW");
    linux_state.net_wm_name = pwInternAtomLinux("_NET_WM_NAME");
    linux_state.utf8_string = pwInternAtomLinux("UTF8_STRING");

    if (linux_state.wm_protocols != XCB_ATOM_NONE &&
        linux_state.wm_delete_window != XCB_ATOM_NONE) {
        xcb_change_property(
            linux_state.connection,
            XCB_PROP_MODE_REPLACE,
            linux_state.window,
            linux_state.wm_protocols,
            XCB_ATOM_ATOM,
            32,
            1,
            &linux_state.wm_delete_window
        );
    }

    pwApplyWindowTitleLinux(title);
    xcb_flush(linux_state.connection);

    linux_state.should_close = false;
    linux_state.visible = false;
    linux_state.initialized = true;
}

void pwDestroyWindow(void) {
    if (!linux_state.initialized) {
        return;
    }

    pwDestroyNativeWindowLinux();
    if (linux_state.connection != nullptr) {
        xcb_disconnect(linux_state.connection);
    }
    linux_state = {};
}

bool pwShouldCloseWindow(void) {
    return linux_state.should_close;
}

void pwPollEvents(void) {
    if (!linux_state.initialized || !linux_state.connection) {
        return;
    }

    xcb_generic_event_t* event = nullptr;
    while ((event = xcb_poll_for_event(linux_state.connection)) != nullptr) {
        u8 response_type = (u8)(event->response_type & 0x7f);
        switch (response_type) {
            case XCB_CLIENT_MESSAGE: {
                xcb_client_message_event_t* client_message =
                    (xcb_client_message_event_t*)event;
                if (client_message->type == linux_state.wm_protocols &&
                    client_message->data.data32[0] ==
                        linux_state.wm_delete_window) {
                    linux_state.should_close = true;
                }
            } break;
            case XCB_CONFIGURE_NOTIFY: {
                xcb_configure_notify_event_t* configure =
                    (xcb_configure_notify_event_t*)event;
                linux_state.width = configure->width;
                linux_state.height = configure->height;
            } break;
            case XCB_DESTROY_NOTIFY:
                linux_state.should_close = true;
                break;
            default:
                break;
        }

        free(event);
    }
}

void pwSetWindowTitle(String title) {
    pwApplyWindowTitleLinux(title);
    if (linux_state.connection != nullptr) {
        xcb_flush(linux_state.connection);
    }
}

void pwGetWindowSize(int* width, int* height) {
    if (width) {
        *width = linux_state.width;
    }
    if (height) {
        *height = linux_state.height;
    }
}

void pwSetWindowSize(int width, int height) {
    if (!linux_state.initialized || linux_state.window == XCB_WINDOW_NONE) {
        return;
    }

    u32 values[] = {(u32)width, (u32)height};
    xcb_configure_window(
        linux_state.connection,
        linux_state.window,
        XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
        values
    );
    xcb_flush(linux_state.connection);

    linux_state.width = width;
    linux_state.height = height;
}

void pwSetWindowResizable(bool resizable) {
    linux_state.resizable = resizable;
}

void pwPresentWindow(void) {
    if (linux_state.connection != nullptr) {
        xcb_flush(linux_state.connection);
    }
}

void pwShowWindow(void) {
    if (!linux_state.initialized || linux_state.visible ||
        linux_state.window == XCB_WINDOW_NONE) {
        return;
    }

    xcb_map_window(linux_state.connection, linux_state.window);
    xcb_flush(linux_state.connection);
    linux_state.visible = true;
}
