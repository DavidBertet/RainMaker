// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "storage.h"

#define MAX_ZONES 8
#define MAX_PROGRAMS 4
#define MAX_ZONE_NAME_LEN 32
#define MAX_PROGRAM_NAME_LEN 32
#define MAX_ZONES_PER_PROGRAM 8
#define MAX_DAYS_LEN 16

typedef enum
{
    ZONE_STATUS_DISABLED,
    ZONE_STATUS_IDLE,
    ZONE_STATUS_RUNNING,
    ZONE_STATUS_TESTING
} zone_status_t;

typedef enum
{
    PROGRAM_STATUS_DISABLED,
    PROGRAM_STATUS_SCHEDULED,
    PROGRAM_STATUS_RUNNING,
    PROGRAM_STATUS_COMPLETED
} program_status_t;

typedef struct
{
    uint8_t id;
    char name[MAX_ZONE_NAME_LEN];
    uint8_t output;
    bool enabled;
    time_t last_run;
} zone_t;

typedef struct
{
    uint8_t zone_id;
    uint16_t duration; // in minutes
    uint8_t order;
} program_zone_t;

typedef struct
{
    uint8_t days; // Bitfield: bit 0=Sun, bit 1=Mon, ..., bit 6=Sat
    uint8_t start_hour;
    uint8_t start_minute;
} schedule_t;

typedef struct
{
    uint8_t id;
    char name[MAX_PROGRAM_NAME_LEN];
    bool enabled;
    schedule_t schedule;
    program_zone_t zones[MAX_ZONES_PER_PROGRAM];
    uint8_t zone_count;
    time_t last_run;
    time_t next_run;
} program_t;

typedef struct
{
    zone_t zones[MAX_ZONES];
    uint8_t zone_count;
    program_t programs[MAX_PROGRAMS];
    uint8_t program_count;
} sprinkler_data_t;

// Function prototypes
esp_err_t sprinkler_load_all_data(sprinkler_data_t *data);
void sprinkler_delete_all_data(void);
esp_err_t sprinkler_save_zone(const zone_t *zone);
esp_err_t sprinkler_load_zone(uint8_t zone_id, zone_t *zone);
esp_err_t sprinkler_delete_zone(uint8_t zone_id);
esp_err_t sprinkler_save_program(const program_t *program);
esp_err_t sprinkler_load_program(uint8_t program_id, program_t *program);
esp_err_t sprinkler_delete_program(uint8_t program_id);
esp_err_t sprinkler_clear_all_data(void);
