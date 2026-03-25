#pragma once
#include "freertos/FreeRTOS.h"

inline bool IsElapsed(TickType_t now, TickType_t last, TickType_t interval)
{
    return (now - last) >= interval;
}

inline TickType_t GetSleepTime(TickType_t now, TickType_t last, TickType_t interval)
{
    TickType_t diff = now - last;
    if (diff >= interval)
        return 0;
    return interval - diff;
}
