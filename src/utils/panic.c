#include "panic.h"

#include "panic_audio.h"
#include "utils/output.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MSG_LEN 1 << 16

#define STRINGIFY(x) #x

#define CSI "\033["
#define SGR(code) CSI STRINGIFY(code)"m"

#define FG_RGB(r,g,b) CSI "38;2;" STRINGIFY(r) ";" STRINGIFY(g) ";" STRINGIFY(b) "m"
#define BG_RGB(r,g,b) CSI "48;2;" STRINGIFY(r) ";" STRINGIFY(g) ";" STRINGIFY(b) "m"

#define BOLD SGR(1)
#define UNDERLINE SGR(4)
#define FAINT SGR(2)
#define RESET SGR(0)

static const char *PANIC_ASCII[] = {
" ███████████    █████████   ██████   █████ █████   █████████  ███",
"░░███░░░░░███  ███░░░░░███ ░░██████ ░░███ ░░███   ███░░░░░███░███",
" ░███    ░███ ░███    ░███  ░███░███ ░███  ░███  ███     ░░░ ░███",
" ░██████████  ░███████████  ░███░░███░███  ░███ ░███         ░███",
" ░███░░░░░░   ░███░░░░░███  ░███ ░░██████  ░███ ░███         ░███",
" ░███         ░███    ░███  ░███  ░░█████  ░███ ░░███     ███░░░ ",
" █████        █████   █████ █████  ░░█████ █████ ░░█████████  ███",
"░░░░░        ░░░░░   ░░░░░ ░░░░░    ░░░░░ ░░░░░   ░░░░░░░░░  ░░░ ",
    NULL
};

static const int RB[][3] = {
    {255,   0,   0}, {255,  80,   0}, {255, 160,   0},
    {255, 220,   0}, {180, 255,   0}, {  0, 255,  80},
    {  0, 255, 200}, {  0, 200, 255}, {  0,  80, 255},
    { 80,   0, 255}, {160,   0, 255}, {255,   0, 200},
    {255,   0, 100},
};
#define NRB ((int)(sizeof(RB)/sizeof(RB[0])))

static void set_random_rgb_fg() {
    int r = rand() % NRB;
    fprintf(stderr, CSI "38;2;%d;%d;%dm", RB[r][0], RB[r][1], RB[r][2]);
}

static void set_random_rgb_bg() {
    int r = rand() % NRB;
    fprintf(stderr, CSI "48;2;%d;%d;%dm", RB[r][0], RB[r][1], RB[r][2]);
}

static void show_panic_ascii() {
    int i = 0;
    for (;;) {
        const char *line = PANIC_ASCII[i++];
        if (!line) break;

        for (size_t j = 0; j < strlen(line); ++j) {
            const char c = line[j];
            fprintf(stderr, RESET);
            if (c == ' ') {
                fprintf(stderr, " ");
                continue;
            }

            set_random_rgb_fg();

            if (rand() % 5 > 3) {
                set_random_rgb_bg();
            }
            fprintf(stderr, "%c", c);
        }
            fprintf(stderr, RESET);
        printf("\n");
    }
}

static void play_panic_audio_async() {
    FILE *f = fopen("/tmp/panic_audio.mp3", "wb");
    fwrite(freesound_community_panic_squeaks_103142_mp3, 1, freesound_community_panic_squeaks_103142_mp3_len, f);
    fclose(f);

    system("ffplay -nodisp -autoexit /tmp/panic_audio.mp3 > /dev/null 2>&1 &");
}

void print_information(const char *msg, const char *file, int line) {
    fprintf(stderr, "\n");

    fprintf(stderr, FG_RGB(180, 180, 180) FAINT "  Reason:\n" RESET);

    char *msg_dup = strdup(msg);
    char *tok = strtok(msg_dup, " ");

    while(tok != NULL) {
        set_random_rgb_fg();
        fprintf(stderr, "%s " RESET, tok);
        tok = strtok(NULL, " ");

    // BOLD FG_RGB(255, 255, 255) "%s" RESET "\n\n", msg);
    }

    fprintf(stderr, FG_RGB(180, 180, 180) FAINT "\n at    " RESET);
    set_random_rgb_fg();
    fprintf(stderr, UNDERLINE "%s" RESET, file);
    set_random_rgb_fg();
    fprintf(stderr, ":%d\n" RESET, line);

}

static const char *FUNNY_FNS[] = {
    "definitely_not_undefined_behavior",
    "trust_me_its_fine",
    "this_totally_works",
    "what_could_go_wrong",
    "TODO_fix_before_release",
    "FIXME_URGENT_DO_NOT_SHIP",
    "copied_from_stackoverflow",
    "my_first_malloc",
    "probably_safe",
    "i_have_no_idea_what_im_doing",
    "technically_works_on_my_machine",
    "its_not_a_bug_its_a_feature",
    "legacy_code_do_not_touch",
    "NobodyKnowsWhatThisDoes",
    "segfault_unless_lucky",
};
#define NFUNNY ((int)(sizeof(FUNNY_FNS)/sizeof(FUNNY_FNS[0])))

static void print_funny_quote() {
    int r = rand() % NFUNNY;
    fprintf(stderr, FAINT FG_RGB(150, 150, 150) "\n\n       %s\n", FUNNY_FNS[r]);
}

void panic(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char msg[MSG_LEN];
    vsnprintf(msg, MSG_LEN, fmt, ap);
    va_end(ap);

    if (is_fancy()) {
        srand(time(NULL));
        play_panic_audio_async();
        show_panic_ascii();
        print_information(msg, file, line);
        print_funny_quote();
    } else {
        fprintf(stderr, "[PANIC] %s - %d\n    %s\n", file, line, msg);
    }

    exit(1);
}
