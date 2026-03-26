/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2026 Michel Lachaine
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
 */
/* beacon.h — NCDXF/IARU beacon network realtime tracking and visualization.
 *
 * The 18 NCDXF/IARU beacons transmit in a coordinated 3-minute (180 s) cycle.
 * In each 10-second time slot, 5 beacons transmit simultaneously — one per
 * band (14.100, 18.110, 21.150, 24.930, 28.200 MHz).  The remaining 13
 * beacons are silent.
 */

#ifndef BEACON_H
#define BEACON_H

#include <time.h>

#define NCDXF_BEACON_COUNT  18
#define BEACON_SLOT_SECONDS 10   /* 10 seconds per time slot */
#define NCDXF_BAND_COUNT    5    /* 5 bands */
#define FULL_CYCLE_SECONDS  (NCDXF_BEACON_COUNT * BEACON_SLOT_SECONDS) /* 180s */

typedef struct {
    char callsign[16];
    char locator[8];
    double lat;
    double lon;
    int active;             /* Currently transmitting */
    int band_index;         /* Which band (0-4), or -1 if silent */
    char qth[32];
} Beacon;

/* NCDXF frequencies (MHz) — indexed by band_index 0-4 */
extern const float ncdxf_frequencies[NCDXF_BAND_COUNT];

/* Band colors (RGBA float) for map rendering and legend */
extern const float ncdxf_band_colors[NCDXF_BAND_COUNT][4];

/* Band label strings ("20m", "17m", …) */
extern const char *ncdxf_band_labels[NCDXF_BAND_COUNT];

typedef struct {
    Beacon beacons[NCDXF_BEACON_COUNT];
    int count;
    time_t last_update;
    int enabled;
    int show_inactive;       /* Show silent beacons as grey */
} BeaconSystem;

void beacon_system_init(BeaconSystem *sys);
void beacon_update_states(BeaconSystem *sys, time_t now);

/* Get beacon transmitting on a given band (NULL if none). */
const Beacon* beacon_get_on_band(const BeaconSystem *sys, int band_index);

/* Get beacon by index. */
Beacon* beacon_get_by_index(BeaconSystem *sys, int index);

/* Seconds remaining in the current 10-second slot. */
int beacon_get_seconds_until_next(const BeaconSystem *sys);

/* Current time slot index (0-17). */
int beacon_get_slot_index(void);

#endif
