#!/bin/sh

##
# Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# The contents of this file constitute Original Code as defined in and
# are subject to the Apple Public Source License Version 1.1 (the
# "License").  You may not use this file except in compliance with the
# License.  Please obtain a copy of the License at
# http://www.apple.com/publicsource and read it before using this file.
# 
# This Original Code and all software distributed under the License are
# distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License.
# 
# @APPLE_LICENSE_HEADER_END@
##
#
# ConvertMakefilesToNewDirs
#
# Convert all references to the old Rhapsody DR1 / OpenStep system
# directory layout to references to the new system directory layout,
# in all PB.project, Makefile, Makefile.preamble, and Makefile.postamble
# files in a project's directory hierarchy.
#
# Copyright (c) 1998, Apple Computer, Inc.
#  All rights reserved.

#
program="$0"

usage="\
Usage: $program [OPTIONS] file ...
Fixes old-style makefiles and PB.project files to reflect the modern
directory layout.
Options:
-r, --recursive  Find and fix all makefiles and PB.project files recursively.
-n, --just-print Don't actually run any commands; just print them.
--help           Print this usage message.
"

recursive=no
just_print=no

for option in "$@"; do
  case "$option" in
    -n | --just-print)
      just_print=yes
      ;;
    -r | --recursive)
      recursive=yes
      ;;
    --help)
      echo "$usage"
      exit 0
      ;;
    -*)
      echo "$program: Unknown option: $option"
      echo "$usage"
      exit 1
      ;;
    *)
      shift; set dummy "$@" "$option"
      ;;
  esac
  shift
done

if [ $# -eq 0 ]; then
  if [ "$recursive" = "yes" ]; then
    echo -n "Convert current directory `pwd`?[n] "
    read convert
    case "$convert" in
	y|yes|Y|YES)
	    set . ;;	# set current directory as argument
	*)
	    exit 1 ;;
    esac
  else
    echo "$program: Too few parameters."
    echo "$usage"
    exit 1
  fi
fi

# Makefile substitutions
#
# /NextApps			=> SYSTEM_APPS_DIR
#				   /System/Applications
# /NextAdmin			=> SYSTEM_ADMIN_APPS_DIR
#				   /System/Administration
# /NextDeveloper/Demos		=> SYSTEM_DEMOS_DIR
#				   /System/Demos
# /NextDeveloper		=> SYSTEM_DEVELOPER_DIR
#				   /System/Developer
# /NextDeveloper/Apps		=> SYSTEM_DEVELOPER_APPS_DIR
#				   /System/Developer/Applications
# /NextLibrary			=> SYSTEM_LIBRARY_DIR
#				   /System/Library
# /usr/lib/NextStep		=> SYSTEM_CORE_SERVICES_DIR
#				   /System/Library/CoreServices
# /LocalDeveloper		=> LOCAL_DEVELOPER_DIR
#				   /Local/Developer
# ~/Apps			=> USER_APPS_DIR
#				   $(HOME)/Applications
# ~/Library			=> USER_LIBRARY_DIR
#				   $(HOME)/Library
# /NextLibrary/Executables	=> SYSTEM_LIBRARY_EXECUTABLES_DIR
#				   $(NEXT_ROOT)/Library/Executables
# /NextDeveloper/Executables	=> SYSTEM_DEVELOPER_EXECUTABLES_DIR
#				   $(NEXT_ROOT)/Developer/Executables
# /LocalDeveloper/Executables	=> LOCAL_DEVELOPER_EXECUTABLES_DIR
#				   $(NEXT_ROOT)/Local/Developer/Executables
# /NextDemos			=> SYSTEM_DEMOS_DIR
#				   /System/Demos
#
# /usr/lib/NextPrinter		=> /System/Library/Printers
# /NextDeveloper/Headers	=> OBSOLETE
# /LocalApps			=> /Network/Applications
# /LocalLibrary			=> /Network/Library
# /LocalAdmin			=> /Network/Administration
# /NextDeveloper/Makefiles	=> /System/Developer/Makefiles

SYSTEM_APPS_DIR='$(SYSTEM_APPS_DIR)'
SYSTEM_ADMIN_APPS_DIR='$(SYSTEM_ADMIN_APPS_DIR)'
SYSTEM_DEMOS_DIR='$(SYSTEM_DEMOS_DIR)'
SYSTEM_DEVELOPER_DIR='$(SYSTEM_DEVELOPER_DIR)'
SYSTEM_DEVELOPER_APPS_DIR='$(SYSTEM_DEVELOPER_APPS_DIR)'
SYSTEM_DOCUMENTATION_DIR='$(SYSTEM_DOCUMENTATION_DIR)'
SYSTEM_LIBRARY_DIR='$(SYSTEM_LIBRARY_DIR)'
SYSTEM_CORE_SERVICES_DIR='$(SYSTEM_CORE_SERVICES_DIR)'
SYSTEM_LIBRARY_EXECUTABLES_DIR='$(SYSTEM_LIBRARY_EXECUTABLES_DIR)'
SYSTEM_DEVELOPER_EXECUTABLES_DIR='$(SYSTEM_DEVELOPER_EXECUTABLES_DIR)'
LOCAL_DEVELOPER_DIR='$(LOCAL_DEVELOPER_DIR)'
LOCAL_DEVELOPER_EXECUTABLES_DIR='$(LOCAL_DEVELOPER_EXECUTABLES_DIR)'
MAKEFILE_DIR='$(MAKEFILEPATH)'
WINDOWS_MAKE_PATH='/Developer/Executables/make'
PDO_MAKE_PATH='/Developer/bin/make'

post_process ()
{
    cmp -s "$1" "$2"
    if [ $? = 0 ]; then
      echo "No need to change $1"
      rm "$2"
    else
      if [ "$just_print" = "yes" ]; then
        echo "Need to fix $1"
        rm "$2"
      else
        echo "Fixing $1"
        mv "$1" "$1~"
        mv "$2" "$1"
      fi
    fi
}

process_makefile ()
{
    sed -e "
      /WINDOWS_BUILDTOOL/ {
        s:[\\/]NextDeveloper[\\/]Executables[\\/]make:$WINDOWS_MAKE_PATH:
        b
      }
      /.*_BUILDTOOL/ {
        b
      }
      s:[\\/]NextApps:$SYSTEM_APPS_DIR:g
      s:[\\/]NextAdmin:$SYSTEM_ADMIN_APPS_DIR:g
      s:[\\/]NextDemos:$SYSTEM_DEMOS_DIR:g
      s:[\\/]NextDeveloper[\\/]Apps:$SYSTEM_DEVELOPER_APPS_DIR:g
      s:[\\/]NextDeveloper[\\/]Demos:$SYSTEM_DEMOS_DIR:g
      s:[\\/]NextDeveloper[\\/]Executables:$SYSTEM_DEVELOPER_EXECUTABLES_DIR:g
      s:\$(NEXT_ROOT)[\\/]NextDeveloper[\\/]Makefiles:$MAKEFILE_DIR:g
      s:[\\/]NextDeveloper:$SYSTEM_DEVELOPER_DIR:g
      s:[\\/]NextLibrary[\\/]Documentation:$SYSTEM_DOCUMENTATION_DIR:g
      s:[\\/]NextLibrary[\\/]Executables:$SYSTEM_LIBRARY_EXECUTABLES_DIR:g
      s:[\\/]NextLibrary:$SYSTEM_LIBRARY_DIR:g
      s:/usr/lib/NextStep:$SYSTEM_CORE_SERVICES_DIR:g
      s:[\\/]LocalDeveloper[\\/]Executables:$LOCAL_DEVELOPER_EXECUTABLES_DIR:g
      s:[\\/]LocalDeveloper:$LOCAL_DEVELOPER_DIR:g
    " < "$1" > "$1.~+~"

    post_process "$1" "$1.~+~"
}

process_project_file ()
{
    sed -e "
      /WINDOWS_BUILDTOOL/ {
        s:[\\/]NextDeveloper[\\/]Executables[\\/]make:$WINDOWS_MAKE_PATH:
        b
      }
      /PDO_UNIX_BUILDTOOL/ {
        s:[\\/]NextDeveloper[\\/]bin[\\/]make:$PDO_MAKE_PATH:
        b
      }
      /.*_BUILDTOOL/ {
        b
      }
      s:\"\([^\"]*\)[\\/]NextApps\([^\"]*\)\":\"\1$SYSTEM_APPS_DIR\2\":g
      s:\([^ (]*\)[\\/]NextApps\([^ ,;)]*\):\"\1$SYSTEM_APPS_DIR\2\":g
      s:\"\([^\"]*\)[\\/]NextAdmin\([^\"]*\)\":\"\1$SYSTEM_ADMIN_APPS_DIR\2\":g
      s:\([^ (]*\)[\\/]NextAdmin\([^ ,;)]*\):\"\1$SYSTEM_ADMIN_APPS_DIR\2\":g
      s:\"\([^\"]*\)[\\/]NextDemos\([^\"]*\)\":\"\1$SYSTEM_DEMOS_DIR\2\":g
      s:\([^ (]*\)[\\/]NextDemos\([^ ,;)]*\):\"\1$SYSTEM_DEMOS_DIR\2\":g
      s:\"\([^\"]*\)[\\/]NextDeveloper[\\/]Apps\([^\"]*\)\":\"\1$SYSTEM_DEVELOPER_APPS_DIR\2\":g
      s:\([^ (]*\)[\\/]NextDeveloper[\\/]Apps\([^ ,;)]*\):\"\1$SYSTEM_DEVELOPER_APPS_DIR\2\":g
      s:\"\([^\"]*\)[\\/]NextDeveloper[\\/]Demos\([^\"]*\)\":\"\1$SYSTEM_DEMOS_DIR\2\":g
      s:\([^ (]*\)[\\/]NextDeveloper[\\/]Demos\([^ ,;)]*\):\"\1$SYSTEM_DEMOS_DIR\2\":g
      s:\"\([^\"]*\)[\\/]NextDeveloper[\\/]Executables\([^\"]*\)\":\"\1$SYSTEM_DEVELOPER_EXECUTABLES_DIR\2\":g
      s:\([^ (]*\)[\\/]NextDeveloper[\\/]Executables\([^ ,;)]*\):\"\1$SYSTEM_DEVELOPER_EXECUTABLES_DIR\2\":g
      s:\$(NEXT_ROOT)[\\/]NextDeveloper[\\/]Makefiles:$MAKEFILE_DIR:g
      s:\"\([^\"]*\)[\\/]NextDeveloper\([^\"]*\)\":\"\1$SYSTEM_DEVELOPER_DIR\2\":g
      s:\([^ (]*\)[\\/]NextDeveloper\([^ ,;)]*\):\"\1$SYSTEM_DEVELOPER_DIR\2\":g
      s:\"\([^\"]*\)[\\/]NextLibrary[\\/]Documentation\([^\"]*\)\":\"\1$SYSTEM_DOCUMENTATION_DIR\2\":g
      s:\([^ (]*\)[\\/]NextLibrary[\\/]Documentation\([^ ,;)]*\):\"\1$SYSTEM_DOCUMENTATION_DIR\2\":g
      s:\"\([^\"]*\)[\\/]NextLibrary[\\/]Executables\([^\"]*\)\":\"\1$SYSTEM_LIBRARY_EXECUTABLES_DIR\2\":g
      s:\([^ (]*\)[\\/]NextLibrary[\\/]Executables\([^ ,;)]*\):\"\1$SYSTEM_LIBRARY_EXECUTABLES_DIR\2\":g
      s:\"\([^\"]*\)[\\/]NextLibrary\([^\"]*\)\":\"\1$SYSTEM_LIBRARY_DIR\2\":g
      s:\([^ (]*\)[\\/]NextLibrary\([^ ,;)]*\):\"\1$SYSTEM_LIBRARY_DIR\2\":g
      s:\"\([^\"]*\)/usr/lib/NextStep\([^\"]*\)\":\"\1$SYSTEM_CORE_SERVICES\2\":g
      s:\([^ (]*\)/usr/lib/NextStep\([^ ,;)]*\):\"\1$SYSTEM_CORE_SERVICES\2\":g
      s:\"\([^\"]*\)[\\/]LocalDeveloper[\\/]Executables\([^\"]*\)\":\"\1$LOCAL_DEVELOPER_EXECUTABLES_DIR\2\":g
      s:\([^ (]*\)[\\/]LocalDeveloper[\\/]Executables\([^ ,;)]*\):\"\1$LOCAL_DEVELOPER_EXECUTABLES_DIR\2\":g
      s:\"\([^\"]*\)[\\/]LocalDeveloper\([^\"]*\)\":\"\1$LOCAL_DEVELOPER_DIR\2\":g
      s:\([^ (]*\)[\\/]LocalDeveloper\([^ ,;)]*\):\"\1$LOCAL_DEVELOPER_DIR\2\":g
    " < "$1" > "$1.~+~"

    post_process "$1" "$1.~+~"
}

process_directory ()
{
  for file in $(find "$1" -name 'Makefile*' -o -name 'PB.project' -o -name 'Info.table');
  do
    case $(basename "$file") in
      Makefile | Makefile.preamble | Makefile.postamble)
        process_makefile "$file"
        ;;
      PB.project | Info.table)
        process_project_file "$file"
        ;;
    esac
  done
}

for file in "$@"; do
  if [ -f "$file" ]; then
    case $(basename "$file") in
    Makefile | Makefile.preamble | Makefile.postamble)
      process_makefile "$file"
      ;;
    PB.project | Info.table)
      process_project_file "$file"
      ;;
    *)
      echo "Skipping file: $file"
      ;;
    esac
  elif [ -d "$file" ]; then
    if [ "$recursive" = "yes" ]; then
      process_directory "$file"
    else
      echo "Skipping directory: $file"
    fi
  else
    echo "Skipping file: $file"
  fi
done
