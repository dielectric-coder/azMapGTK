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
/* beacon.c — NCDXF/IARU beacon network implementation.
 *
 * In each 10-second time slot, 5 of the 18 beacons transmit simultaneously
 * (one per band).  Beacon i transmits on band b when:
 *   slot = (UTC_seconds / 10) % 18
 *   b    = (slot - i + 18) % 18   (only active if b < 5)
 *
 * Reference: https://www.ncdxf.org/beacon/beaconschedule.html
 */

#include "beacon.h"
#include <string.h>

const float ncdxf_frequencies[NCDXF_BAND_COUNT] = {
    14.100f, 18.110f, 21.150f, 24.930f, 28.200f
};

const char *ncdxf_band_labels[NCDXF_BAND_COUNT] = {
    "20m", "17m", "15m", "12m", "10m"
};

/* Band colors: blue, teal, green, orange, red */
const float ncdxf_band_colors[NCDXF_BAND_COUNT][4] = {
    { 0.25f, 0.50f, 1.00f, 0.95f },  /* 14.100 — blue   */
    { 0.10f, 0.80f, 0.80f, 0.95f },  /* 18.110 — teal   */
    { 0.20f, 0.90f, 0.20f, 0.95f },  /* 21.150 — green  */
    { 1.00f, 0.70f, 0.15f, 0.95f },  /* 24.930 — orange */
    { 1.00f, 0.20f, 0.20f, 0.95f },  /* 28.200 — red    */
};

/* Official NCDXF/IARU beacon database — 18 beacons in transmission order. */
static const Beacon ncdxf_beacon_database[NCDXF_BEACON_COUNT] = {
    /*  0 */ {"4U1UN",   "FN30as",  40.750,   -73.967,  0, -1, "New York"},
    /*  1 */ {"VE8AT",   "DO33we",  62.450,  -114.383,  0, -1, "Yellowknife"},
    /*  2 */ {"W6WX",    "CM87xq",  37.339,  -122.037,  0, -1, "California"},
    /*  3 */ {"KH6RS",   "BL10lh",  21.307,  -157.858,  0, -1, "Hawaii"},
    /*  4 */ {"ZL6B",    "RE78ir", -41.283,   174.767,  0, -1, "New Zealand"},
    /*  5 */ {"VK6RBP",  "OF78wd", -31.950,   115.850,  0, -1, "W. Australia"},
    /*  6 */ {"JA2IGY",  "PM84jl",  34.617,   136.538,  0, -1, "Japan"},
    /*  7 */ {"RR9O",    "NO14mx",  55.756,    73.373,  0, -1, "Novosibirsk"},
    /*  8 */ {"VR2B",    "OL72bi",  22.396,   114.110,  0, -1, "Hong Kong"},
    /*  9 */ {"4S7B",    "MJ96wv",   7.288,    80.636,  0, -1, "Sri Lanka"},
    /* 10 */ {"ZS6DN",   "KG33wc", -26.204,    28.047,  0, -1, "South Africa"},
    /* 11 */ {"5Z4B",    "KI88hr",  -1.283,    36.817,  0, -1, "Kenya"},
    /* 12 */ {"4X6TU",   "KM72jv",  32.085,    34.782,  0, -1, "Israel"},
    /* 13 */ {"OH2B",    "KP20if",  60.170,    24.938,  0, -1, "Finland"},
    /* 14 */ {"CS3B",    "IM12or",  32.637,   -16.900,  0, -1, "Madeira"},
    /* 15 */ {"LU4AA",   "GF05rd", -34.604,   -58.382,  0, -1, "Argentina"},
    /* 16 */ {"OA4B",    "FH17ga", -12.046,   -77.043,  0, -1, "Peru"},
    /* 17 */ {"YV5B",    "FK60qc",  10.481,   -66.904,  0, -1, "Venezuela"},
};

void beacon_system_init(BeaconSystem *sys)
{
    memset(sys, 0, sizeof(BeaconSystem));
    sys->enabled = 1;
    sys->show_inactive = 1;
    sys->count = NCDXF_BEACON_COUNT;
    memcpy(sys->beacons, ncdxf_beacon_database, sizeof(ncdxf_beacon_database));

    time_t now = time(NULL);
    beacon_update_states(sys, now);
}

void beacon_update_states(BeaconSystem *sys, time_t now)
{
    if (!sys->enabled || sys->count == 0)
        return;

    int slot = beacon_get_slot_index();

    for (int i = 0; i < sys->count; i++) {
        int b = (slot - i + NCDXF_BEACON_COUNT) % NCDXF_BEACON_COUNT;
        if (b < NCDXF_BAND_COUNT) {
            sys->beacons[i].active = 1;
            sys->beacons[i].band_index = b;
        } else {
            sys->beacons[i].active = 0;
            sys->beacons[i].band_index = -1;
        }
    }

    sys->last_update = now;
}

const Beacon* beacon_get_on_band(const BeaconSystem *sys, int band_index)
{
    if (!sys->enabled || band_index < 0 || band_index >= NCDXF_BAND_COUNT)
        return NULL;
    for (int i = 0; i < sys->count; i++)
        if (sys->beacons[i].active && sys->beacons[i].band_index == band_index)
            return &sys->beacons[i];
    return NULL;
}

Beacon* beacon_get_by_index(BeaconSystem *sys, int index)
{
    if (index < 0 || index >= sys->count)
        return NULL;
    return &sys->beacons[index];
}

int beacon_get_seconds_until_next(const BeaconSystem *sys)
{
    if (!sys->enabled)
        return -1;
    time_t now = time(NULL);
    return BEACON_SLOT_SECONDS - (int)(now % BEACON_SLOT_SECONDS);
}

int beacon_get_slot_index(void)
{
    time_t now = time(NULL);
    return (int)((now / BEACON_SLOT_SECONDS) % NCDXF_BEACON_COUNT);
}
