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


#define checksum_length_token        "SUML"
#define checksum_suffix              ".sum"
#define checksum_suffix_length       4
#define indirection_request_pull     0
#define indirection_request_push     1
#define indirection_request_token    "INDR"
#define indirection_protocol_version "1"
#define operation_both_token         "BOTH"
#define operation_pull_token         "PULL"
#define operation_push_token         "PUSH"
#define operation_version_token      "VERS"
#define result_count_token           "NUMF"
#define result_item_token            "FILE"
#define result_name_token            "NAME"
#define result_type_checksum_only    3
#define result_type_dir              2
#define result_type_file             1
#define result_type_nothing          0
#define result_type_token            "TYPE"
#define token_length                 4


char **dcc_filenames_in_directory(const char *dir_path, int *numFiles);


#endif // DARWIN
