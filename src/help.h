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
/* help.h — keyboard/mouse shortcut reference window.
 *
 * A single non-modal toplevel listing the map controls, toggled with 'h'.
 * GtkShortcutsWindow would be the stock widget for this, but it is
 * deprecated since GTK 4.18 and carries theme colours we do not want. */

#ifndef HELP_H
#define HELP_H

#include <gtk/gtk.h>

/* Toggle the shortcut window: shows it transient for @parent, or closes it
 * if it is already on screen. Safe to call repeatedly — only ever one
 * window exists, and it is destroyed with its parent. */
void help_toggle(GtkWindow *parent);

#endif
