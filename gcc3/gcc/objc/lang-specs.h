/* Definitions for specs for Objective-C.
   Copyright (C) 1998, 1999, 2002 Free Software Foundation, Inc.

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
   objc.  */

  {".m", "@objective-c", 0},
  {"@objective-c",
   /* cc1obj has an integrated ISO C preprocessor.  We should invoke the
      external preprocessor if -save-temps or -traditional is given.  */
   /* APPLE LOCAL begin cpp-precomp dpatel */
   /* Add support to invoke cpp-precomp with -precomp or -cpp-precomp or -E.
      Do not invoke cpp-precomp when -no-cpp-precomp is specified */
     "%{M|MM:%(trad_capable_cpp) -lang-objc -D__OBJC__ %{ansi:-std=c89} %(cpp_options)}\
      %{E|S:%{@:%e-E and -S are not allowed with multiple -arch flags}}\
      %{E:\
	  %{traditional-cpp|no-cpp-precomp:\
	    %(trad_capable_cpp) -lang-objc -D__OBJC__ %{ansi:-std=c89} %(cpp_options)}\
	  %{!traditional-cpp:%{!no-cpp-precomp:\
	    %(cpp_precomp) -lang-objc -D__OBJC__ %{ansi:-std=c89} %(cpp_precomp_options) %y0}}}\
      %{!E:%{!M:%{!MM:\
	  %{save-temps|no-integrated-cpp:\
             %{no-cpp-precomp|traditional-cpp|fload=*|fdump=*:\
                %(trad_capable_cpp) -lang-objc -D__OBJC__ %{ansi:-std=c89} %(cpp_options) %{save-temps:%b.mi} %{!save-temps:%g.mi} \n\
		 cc1obj -fpreprocessed %{save-temps:%b.mi} %{!save-temps:%g.mi} %(cc1_options) %{gen-decls}}\
             %{cpp-precomp|!no-cpp-precomp:%{!traditional-cpp:%{!fdump=*:%{!fload=*:%{!precomp:\
                %(cpp_precomp) -lang-objc -D__OBJC__ %{ansi:-std=c89} %(cpp_precomp_options) %y0 %{save-temps:%b.mi} %{!save-temps:%g.mi} \n\
		 cc1obj -cpp-precomp %{save-temps:%b.mi} %{!save-temps:%g.mi} %(cc1_options) %{gen-decls}}}}}}}\
	    %{precomp:\
		%(cpp_precomp) -lang-objc -D__OBJC__ %{ansi:-std=c89}\
		  %(cpp_precomp_options) %y0\
                  %{precomp:%{@:-o %f%u.p}%{!@:%W{o}%W{!o*:-o %b-gcc3.p}}} }\
	  %{!save-temps:%{!no-integrated-cpp:\
	    %{traditional|ftraditional|traditional-cpp:%{!cpp-precomp:\
		tradcpp0 -lang-objc -D__OBJC__ %{ansi:-std=c89} %(cpp_options) %{!pipe:%g.mi} |\n\
		    cc1obj -fpreprocessed %{!pipe:%g.mi} %(cc1_options) %{gen-decls}}}\
	    %{!fdump=*:%{!fload=*:%{!no-cpp-precomp|cpp-precomp:%{!precomp:%{!traditional-cpp:\
		%(cpp_precomp) -lang-objc -D__OBJC__ %{ansi:-std=c89}\
		  %(cpp_precomp_options) %y0 %{!pipe:%g.mi} |\n\
		    cc1obj -cpp-precomp %{!pipe:%g.mi} %(cc1_options) %{gen-decls}}}}}}\
	    %{precomp:\
		%(cpp_precomp) -lang-objc -D__OBJC__ %{ansi:-std=c89}\
		  %(cpp_precomp_options) %y0\
                  %{precomp:%{@:-o %f%u.p}%{!@:%W{o}%W{!o*:-o %b-gcc3.p}}} }\
	    %{!traditional:%{!ftraditional:%{!traditional-cpp:\
		%{fload=*|fdump=*|no-cpp-precomp:%{!precomp:\
		    cc1obj -lang-objc -D__OBJC__ %{ansi:-std=c89} %(cpp_unique_options) %(cc1_options) %{gen-decls}}}}}}}}\
        %{!fsyntax-only:%{!precomp:%(invoke_as)}}}}}", 0},
   /* APPLE LOCAL end cpp-precomp dpatel */
  {".mi", "@objc-cpp-output", 0},
  {"@objc-cpp-output",
  /* APPLE LOCAL cpp-precomp dpatel */
     "%{!M:%{!MM:%{!E:cc1obj -fpreprocessed %i %(cc1_options) %{gen-decls}\
			     %{!fsyntax-only:%{!precomp:%(invoke_as)}}}}}", 0},
