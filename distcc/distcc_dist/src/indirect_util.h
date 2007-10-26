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


#define indirection_request_token    "INDR"
#define indirection_request_pull     0
#define indirection_request_push     1
#define indirection_complete         2
#define indirection_protocol_version "1"

#define indirection_path_length_token   "PLEN"
#define indirection_file_stat_token     "FSTI"
#define indirection_file_stat_info_present 1
#define indirection_no_file_stat_info      2

#define indirection_pull_response_token "PULR"
#define indirection_pull_response_file_ok 1
#define indirection_pull_response_file_download 2
#define indirection_pull_response_file_missing 3

#define indirection_pull_file       "FILE"

#define operation_pull_token         "PULL"
#define operation_both_token         "BOTH"
#define operation_push_token         "PUSH"
#define operation_version_token      "VERS"

extern unsigned pullfile_cache_max_age;
extern unsigned pullfile_max_cache_size;
extern unsigned pullfile_min_free_space;
