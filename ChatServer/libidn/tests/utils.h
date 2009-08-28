/* utils.h --- Prototypes for self test utilities.
 * Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007  Simon Josefsson
 *
 * This file is part of GNU Libidn.
 *
 * GNU Libidn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GNU Libidn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Libidn; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 */

#ifndef UTILS_H
# define UTILS_H

# include <string.h>
# include <stdarg.h>
# include <stringprep.h>

extern int debug;
extern int error_count;
extern int break_on_error;

extern void fail (const char *format, ...);
extern void escapeprint (const char *str, size_t len);
extern void hexprint (const char *str, size_t len);
extern void binprint (const char *str, size_t len);
extern void ucs4print (const uint32_t * str, size_t len);

/* This must be implemented elsewhere. */
extern void doit (void);

#endif /* UTILS_H */
