/* APPLE LOCAL Objective-C++ */
/* Definitions for specs for Objective-C++.
   Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001
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
   g++.  */

#ifndef OBJCPLUSPLUS_CPP_SPEC
#define OBJCPLUSPLUS_CPP_SPEC 0
#endif

  {".mm",  "@objective-c++", 0},
  {".M",   "@objective-c++", 0},
  {"@objective-c++",
   /* cc1objplus has an integrated ISO C preprocessor.  We should invoke
      the external preprocessor if -save-temps is given.  */
   /* APPLE LOCAL begin cpp-precomp dpatel */
   /* Add support to invoke cpp-precomp with -precomp 
      or -cpp-precomp, and optionally with -E.  */
    "%{M|MM:cpp0 -lang-objc++ %{!no-gcc:-D__GNUG__=%v1}\
       %{!Wno-deprecated:-D__DEPRECATED}\
       %{!fno-exceptions:-D__EXCEPTIONS}\
       -D__GXX_ABI_VERSION=100\
       %{ansi:-D__STRICT_ANSI__ -trigraphs -$} %(cpp_options)}\
      %{E|S:%{@:%e-E and -S are not allowed with multiple -arch flags}}\
      %{E:\
	  %{cpp-precomp:\
	    %(cpp_precomp) -lang-objc++ %{!no-gcc:-D__GNUG__=%v1}\
		%{!Wno-deprecated:-D__DEPRECATED}\
		%{!fno-exceptions:-D__EXCEPTIONS}\
		-D__OBJC__ -D__cplusplus -D__GXX_ABI_VERSION=100\
		%{ansi:-D__STRICT_ANSI__ -trigraphs -$} %(cpp_precomp_options) %y1}\
	  %{!cpp-precomp:\
	    cpp0 -lang-objc++ %{!no-gcc:-D__GNUG__=%v1}\
		%{!Wno-deprecated:-D__DEPRECATED}\
		%{!fno-exceptions:-D__EXCEPTIONS}\
		-D__GXX_ABI_VERSION=100\
		%{ansi:-D__STRICT_ANSI__ -trigraphs -$} %(cpp_options)}}\
     %{precomp: %(cpp_precomp) -lang-objc++ \
       %{!no-gcc:-D__GNUG__=%v1}\
       %{!Wno-deprecated:-D__DEPRECATED}\
       %{!fno-exceptions:-D__EXCEPTIONS}\
       -D__GXX_ABI_VERSION=100\
       %{ansi:-D__STRICT_ANSI__ -trigraphs -$}\
       -D__OBJC__ -D__cplusplus %(cpp_precomp_options) %y1\
       %{@:-o %f%u.pp}%{!@:%W{o}%W{!o*:-o %b-gcc3.pp}} \n}\
     %{!E:%{!M:%{!MM:%{!precomp:\
       %{!save-temps:%{!no-integrated-cpp:%{!fload=*:%{!fdump=*:%{cpp-precomp:%(cpp_precomp) -lang-objc++ \
		    %{!no-gcc:-D__GNUG__=%v1}\
       		    %{!Wno-deprecated:-D__DEPRECATED}\
		    %{!fno-exceptions:-D__EXCEPTIONS}\
		    -D__GXX_ABI_VERSION=100\
		    %{ansi:-D__STRICT_ANSI__ -trigraphs -$}\
		    -D__OBJC__ -D__cplusplus %(cpp_precomp_options) %y1 %d%g.mii \n}}}}}\
       %{save-temps|no-integrated-cpp:%{!cpp-precomp|fdump=*|fload=*:cpp0 -lang-objc++ \
		    %{!no-gcc:-D__GNUG__=%v1}\
       		    %{!Wno-deprecated:-D__DEPRECATED}\
		    %{!fno-exceptions:-D__EXCEPTIONS}\
		    -D__GXX_ABI_VERSION=100\
		    %{ansi:-D__STRICT_ANSI__ -trigraphs -$}\
		    %(cpp_options) %b.mii \n}}\
       %{save-temps|no-integrated-cpp:%{cpp-precomp:%{!fdump=*:%{!fload=*:%(cpp_precomp) -lang-objc++ \
		    %{!no-gcc:-D__GNUG__=%v1}\
       		    %{!Wno-deprecated:-D__DEPRECATED}\
		    %{!fno-exceptions:-D__EXCEPTIONS}\
		    -D__GXX_ABI_VERSION=100\
		    %{ansi:-D__STRICT_ANSI__ -trigraphs -$}\
		    %(cpp_precomp_options) %y1 %b.mii \n}}}}\
      cc1objplus %{save-temps|no-integrated-cpp:%{!cpp-precomp:-fpreprocessed} %{cpp-precomp:-cpp-precomp} %b.mii}\
              %{cpp-precomp:%{!fload=*:%{!fdump=*:-cpp-precomp %d%b.mii}}}\
              %{!save-temps:%{!no-integrated-cpp:%{!cpp-precomp|fload=*|fdump=*:%(cpp_options)\
			    %{!no-gcc:-D__GNUG__=%v1} \
       			    %{!Wno-deprecated:-D__DEPRECATED}\
			    %{!fno-exceptions:-D__EXCEPTIONS}\
			    -D__GXX_ABI_VERSION=100\
			    %{ansi:-D__STRICT_ANSI__}}}}\
       %{ansi:-trigraphs -$}\
       %(cc1_options) %{gen-decls} %2 %{+e1*}\
       %{!fsyntax-only:%(invoke_as)}}}}}",
     OBJCPLUSPLUS_CPP_SPEC},
   /* APPLE LOCAL end cpp-precomp dpatel */
  {".mii", "@objc++-cpp-output", 0},
  /* APPLE LOCAL cpp-precomp dpatel */
  /* Do not invoke_as with -precomp */
  {"@objc++-cpp-output",
   "%{!M:%{!MM:%{!E:\
    cc1objplus -fpreprocessed %i %(cc1_options) %{gen-decls} %2 %{+e*}\
    %{!fsyntax-only:%{!precomp:%(invoke_as)}}}}}", 0},
