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
/* fetch.h — Threaded non-blocking HTTP GET via libcurl + pthread.
 *
 * Each FetchRequest spawns a joinable thread that performs an HTTP GET.
 * The main loop polls fetch_check() each frame; when done, it takes
 * ownership of the response string.  Used by all overlay data sources
 * (MUF, Sporadic E, Aurora, DRAP, Kp/Bz) for async refresh. */

#ifndef FETCH_H
#define FETCH_H

#include <pthread.h>
#include <stddef.h>

typedef struct {
    char           *url;
    char           *response;      /* malloc'd response body (caller frees) */
    size_t          response_len;
    int             status;        /* 0=pending, 1=done, -1=error */
    pthread_t       thread;
    int             thread_active; /* 1 if thread was created and not yet joined */
    pthread_mutex_t mutex;
} FetchRequest;

/* Start an async HTTP GET in a background thread.
 * Safe to call on a previously-used FetchRequest (joins old thread first). */
void fetch_start(FetchRequest *req, const char *url);

/* Non-blocking check: returns status (0=pending, 1=done, -1=error). */
int  fetch_check(FetchRequest *req);

/* Take ownership of the response string (caller must free). Returns NULL on error. */
char *fetch_take_response(FetchRequest *req);

/* Cleanup request resources (joins thread if still active). */
void fetch_cleanup(FetchRequest *req);

#endif
