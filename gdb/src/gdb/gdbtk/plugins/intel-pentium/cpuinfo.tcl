# Display CPU information.
# Copyright 1999, 2000, 2001 Red Hat, Inc.
#
# Written by Fernando Nasser  <fnasser@redhat.com>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License (GPL) as published by
# the Free Software Foundation; either version 2 of the License, or (at
# your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# ------------------------------------------------------------------
#  NAME:         proc display_cpu_info
#  DESCRIPTION:  display what we know about the target CPU
#                if the information is available.
#
#  ARGUMENTS:    None
#  RETURNS:      Nothing
#
#  NOTES:
# ------------------------------------------------------------------
proc display_cpu_info {} {
  global gdb_cpuid_info
  if {[catch {gdb_cmd "info cpu"} result]} {
    tk_messageBox -message "CPU information not available"
  } else {
    tk_messageBox -message "$result"
  }
}
