#ifndef WOLODRIVE_FRAME_TIMING_H
#define WOLODRIVE_FRAME_TIMING_H

#include <SDL2/SDL.h>

/* Keep a 60 Hz ceiling even with VSync on a higher-refresh display;
   after an overrun, resume from completion instead of producing catch-up bursts. */
static inline Uint64 frame_next_deadline(Uint64 previous, Uint64 finished, Uint64 frequency) {
    Uint64 deadline = previous + frequency / 60;
    return deadline > finished ? deadline : finished;
}

static inline void frame_wait_until(Uint64 deadline, Uint64 frequency) {
    Uint64 now;
    while ((now = SDL_GetPerformanceCounter()) < deadline) {
        Uint32 remaining_ms = (Uint32)((deadline - now) * 1000 / frequency);
        /* Sleep for most of the interval, yielding near the deadline to avoid
           adding a whole timer tick to every frame on Windows or Linux. */
        SDL_Delay(remaining_ms > 1 ? remaining_ms - 1 : 0);
    }
}

#endif
