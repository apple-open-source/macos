/* APPLE LOCAL Objective-C++ */
/* Definitions for specs for C++.
   Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* This is the contribution to the `default_compilers' array in gcc.c for
   objective-c++.  */

#ifndef OBJCPLUSPLUS_CPP_SPEC
#define OBJCPLUSPLUS_CPP_SPEC 0
#endif

  {".mm",  "@objective-c++", 0},
  {".M",   "@objective-c++", 0},
  {"@objective-c++",
    "%{E|M|MM:cc1objplus -E %{!no-gcc:-D__GNUG__=%v1}\
       %(cpp_options) %2 %(cpp_debug_options)}\
    "/* APPLE LOCAL prohibit -arch with -E and -S  */"\
     %{E|S:%{@:%e-E and -S are not allowed with multiple -arch flags}}\
     %{!E:%{!M:%{!MM:\
       %{save-temps:cc1objplus -E %{!no-gcc:-D__GNUG__=%v1}\
 	"/* APPLE LOCAL IMI */"\
		%(cpp_options) %2 -o %b.mii \n}\
      cc1objplus %{save-temps:-fpreprocessed %b.mii}\
	      %{!save-temps:%(cpp_unique_options) %{!no-gcc:-D__GNUG__=%v1}}\
	%(cc1_options) %{gen-decls} %2 %{+e1*}\
       %{!fsyntax-only:%(invoke_as)}}}}",
     OBJCPLUSPLUS_CPP_SPEC},
  {".mii", "@objc++-cpp-output", 0},
  {"@objc++-cpp-output",
   "%{!M:%{!MM:%{!E:\
    cc1objplus -fpreprocessed %i %(cc1_options) %{gen-decls} %2 %{+e*}\
    %{!fsyntax-only:%(invoke_as)}}}}", 0},
  {"@objective-c++-header",
    "%{E|M|MM:cc1objplus -E %{!no-gcc:-D__GNUG__=%v1}\
       %(cpp_options) %2 %(cpp_debug_options)}\
     %{!E:%{!M:%{!MM:\
       %{save-temps:cc1objplus -E %{!no-gcc:-D__GNUG__=%v1}\
		%(cpp_options) %2 %b.mii \n}\
      cc1objplus %{save-temps:-fpreprocessed %b.mii}\
	      %{!save-temps:%(cpp_unique_options) %{!no-gcc:-D__GNUG__=%v1}}\
	%(cc1_options) %{gen-decls} %2 %{+e1*}\
               %(pch) %(dbg_ss)}}}", 0},
