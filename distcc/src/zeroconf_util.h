/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2003, 2005 by Apple Computer, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#if defined(DARWIN)


#define DISTCC_DEFAULT_SCHEDULER_PORT    3633
#define DISTCC_DEFAULT_ZC_LIST_LEN_TOKEN "ZCLL"

#define ZC_ALL_INTERFACES      0
#define ZC_DOMAIN              ""
#define ZC_REG_TYPE            "_distcc._tcp"


char *dcc_generate_txt_record(void);
int dcc_simple_spawn(const char *path, char *const argv[]);
char *dcc_zc_full_name(const char *aName, const char *aRegType,
                       const char *aDomain);


#endif // DARWIN
