#include "fog_timer.h"
#include "utils/file_helper.h"
#include <stdint.h>

#ifdef __linux__
#   include <sys/time.h>
#endif


int64_t get_current_ns() {
#ifdef __linux__
    struct timeval tp;

    gettimeofday(&tp, NULL);
    return tp.tv_sec * 1000000LL + tp.tv_usec;
#endif
}

void timer_start(FogTimer *timer) {
    timer->start = get_current_ns();
}
void timer_end(FogTimer *timer) {
    timer->end = get_current_ns();
}

uint64_t timer_ms(FogTimer timer) {
    return (timer.end - timer.start) / 1000;
}

