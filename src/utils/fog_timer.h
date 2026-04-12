#ifndef FOG_TIMER_H
#define FOG_TIMER_H

#include <stdint.h>

typedef struct {
    int64_t start;
    int64_t end;
} FogTimer;

void timer_start(FogTimer *timer);
void timer_end(FogTimer *timer);

uint64_t timer_ms(FogTimer timer);

#endif // FOG_TIMER_H
