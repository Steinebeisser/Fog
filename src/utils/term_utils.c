#include "term_utils.h"

#include <sys/ioctl.h>

Vec2 get_term_size() {
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);

    return (Vec2){
        .x = w.ws_col,
        .y = w.ws_row
    };
}
