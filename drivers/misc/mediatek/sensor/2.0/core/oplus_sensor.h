/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*
 * Copyright(C)2014 MediaTek Inc.
 * Modification based on code covered by the below mentioned copyright
 * and/or permission notice(S).
 */

#ifndef __OPLUSSENSOR_H__
#define __OPLUSSENSOR_H__
#include <linux/types.h>

typedef struct {
    uint32_t value;
    uint16_t report_count;
} camera_protect_event_t;

typedef struct {
    uint32_t free_fall_time;
    uint32_t angle;
    uint16_t report_count;
} free_fall_event_t;

typedef struct {
    uint32_t value;
    uint16_t report_count;
} pickup_detect_event_t;

typedef struct {
    uint32_t value;
    uint16_t report_count;
} fp_display_event_t;

typedef struct {
    int32_t state;
} sar_modem_event_t;

typedef struct {
    int16_t state;
    uint16_t report_count;
} lux_aod_event_t;

typedef struct {
    int16_t state;
    uint16_t report_count;
} sensor_monitor_event_t;

typedef struct {
    uint32_t step_count;
    uint16_t report_count;
    uint32_t move_status;
    uint16_t time_gap;
    uint32_t step_run_count;
    uint32_t step_walk_count;
} pedo_minute_event_t;

typedef struct {
    uint16_t motion_count;
    uint16_t motion_ith;
    uint16_t incar_state;
    uint16_t activity_mode;
    uint32_t delta_time;
} oplus_activity_recognition_event_t;

typedef struct {
    uint16_t value;
    uint16_t report_count;
} elevator_detect_event_t;

typedef struct {
    uint16_t value;
    uint16_t report_count;
} rotation_detect_event_t;

typedef struct {
    uint16_t value;
    uint16_t report_count;
} back_tap_detect_event_t;

typedef struct {
    uint16_t value;
    uint16_t report_count;
} lay_detect_event_t;

typedef struct {
    uint16_t value;
    uint16_t state;
    uint16_t report_count;
} palm_detect_event_t;

typedef struct {
    uint16_t value;
    uint16_t report_count;
} smart_prox_event_t;

union oplus_data_unit_t {
    camera_protect_event_t camera_protect_data_t;
    free_fall_event_t free_fall_data_t;
    pickup_detect_event_t pickup_detect_data_t;
    fp_display_event_t fp_display_data_t;
    sar_modem_event_t sar_modem_event;
    lux_aod_event_t lux_aod_event;
    sensor_monitor_event_t sensor_monitor_event;
    pedo_minute_event_t pedo_minute_event;
    oplus_activity_recognition_event_t oplus_activity_recognition_event;
    elevator_detect_event_t elevator_detect_event;
    rotation_detect_event_t rotation_detect_event;
    back_tap_detect_event_t back_tap_detect_event;
    lay_detect_event_t lay_detect_event;
    palm_detect_event_t palm_detect_event;
    smart_prox_event_t phone_prox_event;
    smart_prox_event_t gesture_prox_event;
    smart_prox_event_t ambiente_prox_event;
};

#endif /*__OPLUSSENSOR_H*/
