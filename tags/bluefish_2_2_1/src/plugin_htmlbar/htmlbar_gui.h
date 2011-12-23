/* Bluefish HTML Editor
 * htmlbar_gui.h
 *
 * Copyright (C) 2005 Olivier Sessink
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
 
extern const guint8 pixmap_css_small[];
extern const guint8 pixmap_php3[];
extern const guint8 pixmap_edit_tag[];

GtkWidget *htmlbar_pixmap(const guint8 *data);
void htmlbar_toolbar(Thtmlbarwin *hbw);

