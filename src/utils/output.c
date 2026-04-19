#include "output.h"

#define PGS_LOG_STRIP_PREFIX
#include "third_party/pgs_log.h"

bool is_fancy_output = false;

bool is_fancy() {
    return is_fancy_output;
};

bool setup_in_and_output() {
    bool ret = true;
    if (!enable_raw_mode()) {
        LOG_ERROR("Failed to enable raw mode");
        ret = false;
    }
    if (!enable_ansi()) {
        LOG_ERROR("Failed to enable ANSI mode");
        ret = false;
    }
    if (!enable_utf8()) {
        LOG_ERROR("Failed to enable UTF-8");
        ret = false;
    }
    if (ret)
        is_fancy_output = true;
    return ret;
}

#ifdef __WIN32

#include <windows.h>

static bool ansi_mode_enabled = false;
static HANDLE hOut = INVALID_HANDLE_VALUE;
static DWORD original_output_mode = 0;

static UINT g_oldOutputCP = 0;
static UINT g_oldInputCP = 0;

bool raw_mode_enabled = false;
static DWORD original_input_mode = 0;
static HANDLE hIn = INVALID_HANDLE_VALUE;

HANDLE get_input_handle() {
    return hIn;
}

bool enable_raw_mode() {
    if (raw_mode_enabled) {
        return true;
    }

    SetConsoleCtrlHandler(NULL, TRUE);

    hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (!GetConsoleMode(hIn, &original_input_mode)) {
        return false;
    }
    DWORD mode = original_input_mode;

    mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    if (!SetConsoleMode(hIn, mode)) {
        return false;
    }

    atexit(disable_raw_mode);
    raw_mode_enabled = true;
    return true;
}

bool enable_ansi() {
    if (ansi_mode_enabled) {
        return true;
    }

    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        return false;
    }

    if (!GetConsoleMode(hOut, &original_output_mode)) {
        return false;
    }
    DWORD dwMode = original_output_mode;

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN | ENABLE_PROCESSED_OUTPUT;
    if (!SetConsoleMode(hOut, dwMode)) {
        return false;
    }

    ansi_mode_enabled = true;
    atexit(disable_ansi);
    return true;
}

bool enable_utf8() {
    g_oldOutputCP = GetConsoleOutputCP();
    g_oldInputCP = GetConsoleCP();

    if (!SetConsoleOutputCP(65001)) return false;
    if (!SetConsoleCP(65001)) return false;
    atexit(disable_utf8);
    return true;
}

void disable_raw_mode() {
    if (!raw_mode_enabled || original_input_mode == 0 || hIn == INVALID_HANDLE_VALUE) {
        return;
    }

    FlushConsoleInputBuffer(hIn);
    SetConsoleMode(hIn, original_input_mode);
    raw_mode_enabled = false;
}

void disable_ansi() {
    if (!ansi_mode_enabled || original_output_mode == 0 || hOut == INVALID_HANDLE_VALUE) {
        return;
    }

    SetConsoleMode(hOut, original_output_mode);
    ansi_mode_enabled = false;
}
void disable_utf8() {
    if (g_oldOutputCP != 0) {
        SetConsoleOutputCP(g_oldOutputCP);
    }
    if (g_oldInputCP != 0) {
        SetConsoleCP(g_oldInputCP);
    }
}

#elif defined(__unix__)
#define PGS_LOG_STRIP_PREFIX
#include "third_party/pgs_log.h"

#include <stdbool.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
static struct termios orig_termios;

// https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html
bool enable_raw_mode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        LOG_ERROR("Failed to enable raw mode");
        return false;
    }
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        LOG_ERROR("FAiled to enable raw mode");
        return false;
    }
    return true;
}

bool enable_ansi() {
    return true;
}

bool enable_utf8() {
    return true;
}

void disable_raw_mode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
        LOG_ERROR("Failed to disable raw mode");
    }
}

void disable_ansi() { }

void disable_utf8() { }
#else
#error "Unsupported Platform"
#endif
