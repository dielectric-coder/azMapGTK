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
/* qrz.h — QRZ.com amateur radio callsign lookup via XML API.
 *
 * Authenticates with QRZ.com credentials, then looks up callsigns to retrieve
 * name, location, Maidenhead grid square, and lat/lon coordinates.  Uses
 * libcurl for HTTP; sessions auto-renew on timeout. */

#ifndef QRZ_H
#define QRZ_H

typedef struct {
    double lat, lon;
    char call[32];       /* normalized callsign */
    char name[128];      /* "fname name" */
    char location[128];  /* "addr2, country" */
    char grid[16];       /* Maidenhead grid */
    int  valid;          /* 1 if lat/lon were found */
} QRZResult;

/* Initialize with credentials. Returns 0 on success. */
int qrz_init(const char *username, const char *password);

/* Lookup a callsign. Blocks during HTTP request.
 * Returns 0 on success, -1 on error. err_buf filled on error. */
int qrz_lookup(const char *callsign, QRZResult *result, char *err_buf, int err_sz);

/* Free any resources (curl cleanup). */
void qrz_cleanup(void);

#endif
