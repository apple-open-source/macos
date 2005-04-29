/*
 * "$Id: gimp-print-intl.h,v 1.1.1.2 2004/07/23 06:26:27 jlovell Exp $"
 *
 *   I18N header file for Gimp-Print.
 *
 *   Copyright 1997-2000 Michael Sweet (mike@easysw.com),
 *	Robert Krawitz (rlk@alum.mit.edu) and Michael Natterer (mitch@gimp.org)
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License as published by the Free
 *   Software Foundation; either version 2 of the License, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *   for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/**
 * @file gimp-print-intl.h
 * @brief Internationalisation functions.
 */

#ifndef GIMP_PRINT_INTL_H
#define GIMP_PRINT_INTL_H

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * Internationalisation functions are used to localise Gimp-Print by
   * translating strings into the user's native language.
   *
   * The macros defined in this header are convenience wrappers around
   * the gettext functions provided by libintl library (or directly by
   * libc on GNU systems).
   *
   * @defgroup intl intl
   * @{
   */

#ifdef INCLUDE_LOCALE_H
INCLUDE_LOCALE_H
#else
#include <locale.h>
#endif

#ifdef ENABLE_NLS

#include <libintl.h>
#ifndef _
/** Translate String. */
#define _(String) gettext (String)
#endif
#ifndef gettext_noop
/** Mark String for translation, but don't translate it right now. */
#define gettext_noop(String) (String)
#endif
#ifdef gettext_noop
/** Mark String for translation, but don't translate it right now. */
# define N_(String) gettext_noop (String)
#else
/** Mark String for translation, but don't translate it right now. */
# define N_(String) (String)
#endif

#else /* ifndef ENABLE_NLS */
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)

#endif

  /** @} */

#ifdef __cplusplus
  }
#endif

#endif /* GIMP_PRINT_INTL_H */
