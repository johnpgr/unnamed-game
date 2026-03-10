#pragma once

void pwCreateWindow(String title, int width, int height);

inline void pwCreateWindow(const char* title, int width, int height) {
    pwCreateWindow(String::fromCStr(title), width, height);
}

void pwDestroyWindow(void);
bool pwShouldWindowClose(void);
void pwPollEvents(void);
void pwSetWindowTitle(String title);

template <u64 N> inline void pwSetWindowTitle(const char (&title)[N]) {
    pwSetWindowTitle({(const u8*)title, N - 1});
}

inline void pwSetWindowTitle(const char* title) {
    pwSetWindowTitle(String::fromCStr(title));
}

void pwGetWindowSize(int* width, int* height);
void pwSetWindowSize(int width, int height);
void pwSetWindowResizable(bool resizable);
void pwPresentWindow(void);
void pwShowWindow(void);
