// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#include "days_utils.h"

void set_day_mask(uint8_t *days, uint8_t mask)
{
    *days = mask;
}

void add_day(uint8_t *days, day_of_week_t day)
{
    *days |= (1 << (day));
}

void remove_day(uint8_t *days, day_of_week_t day)
{
    *days &= ~(1 << (day));
}

bool has_day(uint8_t days, day_of_week_t day)
{
    return (days & (1 << (day))) != 0;
}

// Calculate next run time for a program
time_t calculate_next_run(uint8_t days, uint8_t start_hour, uint8_t start_minute)
{
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);

    // Create time structure for today's scheduled time
    struct tm tm_target = *tm_now;
    tm_target.tm_hour = start_hour;
    tm_target.tm_min = start_minute;
    tm_target.tm_sec = 0;

    time_t target_today = mktime(&tm_target);

    // Check if program should run today and hasn't run yet
    int today_bit = 1 << tm_now->tm_wday;
    if (days & today_bit && target_today > now)
    {
        return target_today;
    }

    // Find next day this program should run
    for (int days_ahead = 1; days_ahead <= 7; days_ahead++)
    {
        time_t future_time = now + (days_ahead * 24 * 60 * 60);
        struct tm *tm_future = localtime(&future_time);

        int future_day_bit = 1 << tm_future->tm_wday;
        if (days & future_day_bit)
        {
            struct tm tm_future_target = *tm_future;
            tm_future_target.tm_hour = start_hour;
            tm_future_target.tm_min = start_minute;
            tm_future_target.tm_sec = 0;

            return mktime(&tm_future_target);
        }
    }

    return 0; // Should never reach here if program has valid schedule
}