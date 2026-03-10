#include "platform/win32/internal.h"

void pwCreateWindow(String title, int width, int height) {
    if (win32_state.initialized) {
        pwDestroyWindow();
    }

    win32_state.resizable = false;
    win32_state.width = width;
    win32_state.height = height;
    win32_state.should_close = false;

    if (!pwRegisterWindowClassWin32()) {
        DWORD error = GetLastError();
        LOG_FATAL(
            "RegisterClassExW failed with error %lu",
            (unsigned long)error
        );
        abort();
    }

    Arena* scratch = pGetScratchWin32();
    u64 scratch_mark = scratch->mark();
    wchar_t* wide_title = pToWideStringWin32(
        scratch,
        title.size > 0 ? title : String::lit("cpp-gaming")
    );
    if (!wide_title) {
        scratch->restore(scratch_mark);
        pFail("Window title conversion failed.");
    }

    RECT rect = {0, 0, width, height};
    DWORD style = pwGetWindowStyleWin32();
    AdjustWindowRectEx(&rect, style, FALSE, 0);

    win32_state.window = CreateWindowExW(
        0,
        L"cpp_gaming_window_class",
        wide_title,
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        win32_state.instance,
        nullptr
    );
    scratch->restore(scratch_mark);

    if (!win32_state.window) {
        DWORD error = GetLastError();
        LOG_FATAL(
            "CreateWindowExW failed with error %lu",
            (unsigned long)error
        );
        abort();
    }

    win32_state.initialized = true;
    win32_state.visible = false;
}

void pwDestroyWindow(void) {
    if (!win32_state.initialized) {
        return;
    }

    if (win32_state.window) {
        DestroyWindow(win32_state.window);
        win32_state.window = nullptr;
    }

    if (win32_state.class_registered) {
        UnregisterClassW(L"cpp_gaming_window_class", win32_state.instance);
    }

    win32_state = {};
}

bool pwShouldCloseWindow(void) {
    return win32_state.should_close;
}

void pwPollEvents(void) {
    if (!win32_state.initialized) {
        return;
    }

    MSG message = {};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
        if (message.message == WM_QUIT) {
            win32_state.should_close = true;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

void pwSetWindowTitle(String title) {
    if (!win32_state.window) {
        return;
    }

    Arena* scratch = pGetScratchWin32();
    u64 scratch_mark = scratch->mark();
    wchar_t* wide_title = pToWideStringWin32(scratch, title);
    if (!wide_title) {
        scratch->restore(scratch_mark);
        return;
    }

    SetWindowTextW(win32_state.window, wide_title);
    scratch->restore(scratch_mark);
}

void pwGetWindowSize(int* width, int* height) {
    if (width) {
        *width = win32_state.width;
    }
    if (height) {
        *height = win32_state.height;
    }
}

void pwSetWindowSize(int width, int height) {
    if (!win32_state.window) {
        return;
    }

    RECT rect = {0, 0, width, height};
    DWORD style = (DWORD)GetWindowLongPtrW(win32_state.window, GWL_STYLE);
    DWORD ex_style = (DWORD)GetWindowLongPtrW(win32_state.window, GWL_EXSTYLE);
    AdjustWindowRectEx(&rect, style, FALSE, ex_style);

    SetWindowPos(
        win32_state.window,
        nullptr,
        0,
        0,
        rect.right - rect.left,
        rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE
    );

    win32_state.width = width;
    win32_state.height = height;
}

void pwSetWindowResizable(bool resizable) {
    win32_state.resizable = resizable;
    if (!win32_state.window) {
        return;
    }

    LONG_PTR style = GetWindowLongPtrW(win32_state.window, GWL_STYLE);
    style &= ~(WS_MAXIMIZEBOX | WS_THICKFRAME);
    if (resizable) {
        style |= WS_MAXIMIZEBOX | WS_THICKFRAME;
    }

    SetWindowLongPtrW(win32_state.window, GWL_STYLE, style);
    SetWindowPos(
        win32_state.window,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
            SWP_FRAMECHANGED
    );
}

void pwPresentWindow(void) {
    if (win32_state.window) {
        UpdateWindow(win32_state.window);
    }
}

void pwShowWindow(void) {
    if (!win32_state.initialized || win32_state.visible ||
        !win32_state.window) {
        return;
    }

    ShowWindow(win32_state.window, SW_SHOW);
    UpdateWindow(win32_state.window);
    win32_state.visible = true;
}
