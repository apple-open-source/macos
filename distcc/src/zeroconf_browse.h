/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2003 by Apple Computer, Inc.
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


void dcc_browse_for_zeroconfig(char *txtRecord);
void dcc_zc_add_to_select_set(int anFD);
int dcc_zc_process_browse_messages(fd_set *availableFDs, int fdCount);
int dcc_zc_process_resolve_messages(fd_set *availableFDs, int fdCount);
void dcc_zc_remove_from_select_set(int anFD);
char *dcc_zc_resolved_services_list(void);
void dcc_zc_select_set(fd_set *setCopy);
int dcc_zc_select_count(void);
int dcc_zc_should_process_client_requests(void);


#endif // DARWIN
