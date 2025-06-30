// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Example usage:
/*
uint8_t days = 0;

// Set Monday, Wednesday, Friday
add_day(&days, DAY_MONDAY);
add_day(&days, DAY_WEDNESDAY);
add_day(&days, DAY_FRIDAY);
remove_day(&days, DAY_MONDAY);

// Or set multiple days at once using masks
set_day_mask(&days, DAYS_MONDAY | DAYS_WEDNESDAY | DAYS_FRIDAY);

// Check if a day is set
if (has_day(&days, DAY_MONDAY)) {
    // Monday is scheduled
}
*/

// Days of the week enum for bit positions
typedef enum
{
    DAY_SUNDAY = 0,
    DAY_MONDAY = 1,
    DAY_TUESDAY = 2,
    DAY_WEDNESDAY = 3,
    DAY_THURSDAY = 4,
    DAY_FRIDAY = 5,
    DAY_SATURDAY = 6
} day_of_week_t;

// Days of the week bit masks
typedef enum
{
    DAYS_SUNDAY = (1 << DAY_SUNDAY),       // 0x01
    DAYS_MONDAY = (1 << DAY_MONDAY),       // 0x02
    DAYS_TUESDAY = (1 << DAY_TUESDAY),     // 0x04
    DAYS_WEDNESDAY = (1 << DAY_WEDNESDAY), // 0x08
    DAYS_THURSDAY = (1 << DAY_THURSDAY),   // 0x10
    DAYS_FRIDAY = (1 << DAY_FRIDAY),       // 0x20
    DAYS_SATURDAY = (1 << DAY_SATURDAY)    // 0x40
} days_mask_t;

void set_day_mask(uint8_t *days, uint8_t mask);
void add_day(uint8_t *days, day_of_week_t day);
void remove_day(uint8_t *days, day_of_week_t day);
bool has_day(uint8_t days, day_of_week_t day);

time_t calculate_next_run(uint8_t days, uint8_t start_hour, uint8_t start_minute);
