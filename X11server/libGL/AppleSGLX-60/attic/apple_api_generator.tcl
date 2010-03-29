package require Tcl 8.5

if 0 {
 apple_api_generator.tcl -- This generates:
    include/GL/gl.h (from /System/Library/Frameworks/OpenGL.framework/Headers/gl.h)
    apple_api.h (from /System/Library/Frameworks/OpenGL.framework/Headers/gl.h)
    apple_api.c

 Copyright (c) 2008 Apple Inc.
 
 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation files
 (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge,
 publish, distribute, sublicense, and/or sell copies of the Software,
 and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:
 
 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT.  IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT
 HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE SOFTWARE.
 
 Except as contained in this notice, the name(s) of the above
 copyright holders shall not be used in advertising or otherwise to
 promote the sale, use or other dealings in this Software without
 prior written authorization.
}

#Translate from a gliDispatch.h function pointer name to a gl*() type name.
proc gli-to-gl name {
    set r gl
    set toupper 1
    foreach c [split $name ""] {
	if {$toupper} {
	    append r [string toupper $c]
	    set toupper 0
	} else {
	    if {"_" eq $c} {
		set toupper 1
	    } else {
		append r $c
	    }
	}
    }
    return $r
}

set gl_license {
/*
** License Applicability. Except to the extent portions of this file are
** made subject to an alternative license as permitted in the SGI Free
** Software License B, Version 1.1 (the "License"), the contents of this
** file are subject only to the provisions of the License. You may not use
** this file except in compliance with the License. You may obtain a copy
** of the License at Silicon Graphics, Inc., attn: Legal Services, 1600
** Amphitheatre Parkway, Mountain View, CA 94043-1351, or at:
** 
** http://oss.sgi.com/projects/FreeB
** 
** Note that, as provided in the License, the Software is distributed on an
** "AS IS" basis, with ALL EXPRESS AND IMPLIED WARRANTIES AND CONDITIONS
** DISCLAIMED, INCLUDING, WITHOUT LIMITATION, ANY IMPLIED WARRANTIES AND
** CONDITIONS OF MERCHANTABILITY, SATISFACTORY QUALITY, FITNESS FOR A
** PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
** 
** Original Code. The Original Code is: OpenGL Sample Implementation,
** Version 1.2.1, released January 26, 2000, developed by Silicon Graphics,
** Inc. The Original Code is Copyright (c) 1991-2000 Silicon Graphics, Inc.
** Copyright in any portions created by third parties is as indicated
** elsewhere herein. All Rights Reserved.
** 
** Additional Notice Provisions: This software was created using the
** OpenGL(R) version 1.2.1 Sample Implementation published by SGI, but has
** not been independently verified as being compliant with the OpenGL(R)
** version 1.2.1 Specification.
*/
}

set apple_license {
/*
 Copyright (c) 2008 Apple Inc.
 
 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation files
 (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge,
 publish, distribute, sublicense, and/or sell copies of the Software,
 and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:
 
 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT.  IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT
 HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE SOFTWARE.
 
 Except as contained in this notice, the name(s) of the above
 copyright holders shall not be used in advertising or otherwise to
 promote the sale, use or other dealings in this Software without
 prior written authorization.
*/
}


proc parse-decl-split decl {
    set r [list]
    set token ""

    foreach c [split $decl ""] {
	if {"*" eq $c || "(" eq $c || ")" eq $c || "," eq $c || ";" eq $c} {
	    if {[string length $token]} {
		lappend r $token
	    }
	    lappend r $c
	    set token ""
	} elseif {[string is space -strict $c]} {
	    if {[string length $token]} {
		lappend r $token
	    }
	    set token ""
	} else {
	    append token $c
	}
    }

    if {[string length $token]} {
	lappend r $token
    }

    return $r
}

#Return an empty [list] when it's not a declaration.
#Return a list with a format of: rettype name arglist; otherwise.
proc parse-decl decl {
    #puts DECL:$decl

    set dlist [parse-decl-split $decl]
    if {![llength $dlist]} {
	return [list]
    }

    #puts DLIST:$dlist

    set end [lsearch -exact $dlist ";"]
    set argstart [lsearch -exact $dlist "("]
    set argend $end

    set namestart [expr {$argstart - 1}]
    set name [lindex $dlist $namestart]
    set rettype [lrange $dlist 0 [expr {$namestart - 1}]]

    set arglist [list]
    set arg [list]
    
    foreach i [lrange $dlist [expr {$argstart + 1}] $argend] {
	if {"," eq $i || ")" eq $i} {
	    if {[llength $arg] >= 2} {
		set var [lindex $arg end]
		set type [lrange $arg 0 end-1]
		lappend arglist [list $type $var]
	    } else {
		#This is probably a decl like: void foo(void);
		lappend arglist [list $arg ""]
	    }
	    
	    set arg [list]
	    continue
	}
	lappend arg $i
    }

    if {![llength $name]} {
	return [list]
    }

    if {[lindex $rettype 0] eq "extern"} {
	set rettype [lrange $rettype 1 end]
    }

    #puts ARGLIST:$arglist

    return [list $rettype $name $arglist]
}

proc arglist-to-c arglist {
    set c ""
    set varchar [scan a %c]
    
    foreach arg $arglist {
	lassign $arg type _
	if {"void" eq $type || "GLvoid" eq $type} {
	    continue
	}

	append c "[join $type " "] [format %c $varchar], "
	incr varchar
    }

    return [string trim $c ", "]
}

proc arglist-to-c-call arglist {
    set call ""
    set varchar [scan a %c]

    foreach arg $arglist {
	lassign $arg type var

	if {"void" eq $type || "GLvoid" eq $type} {
	    continue
	}

	append call "[format %c $varchar], "
	incr varchar
    }

    return [string trimright $call ", "]
}

proc main {argv} {
    global gl_license apple_license

    set fd [open /System/Library/Frameworks/OpenGL.framework/Headers/gl.h r]
    set data [read $fd]
    close $fd
    
    set gldefs [list]


    foreach {allmatch def body} [regexp -inline -all {(#[^\s]+)\s+([^\n]+)} $data] {
	#puts "DEF:$def BODY:$body"
	set symbol [lindex [split $body] 0]

	if {"#define" eq $def && 
		("__gl_h_" eq $symbol || "GL_GLEXT_FUNCTION_POINTERS" eq $symbol 
		 || "GL_TYPEDEFS_2_0" eq $symbol)} {
	    #skip
	    continue
	}

	if {"#define" eq $def} {
	    #puts "ADDING:$body"
	    lappend gldefs "$def $body"
	}
    }


    set fd [open "/System/Library/Frameworks/OpenGL.framework/Headers/gl.h" r]
    set data [read $fd]
    close $fd

    set glfunclist [list]
    foreach line [split $data \n] {
	if {[string match extern* $line]} {
	    set glfunc [parse-decl $line]
	    if {[llength $glfunc]} {
		lappend glfunclist $glfunc
	    }
	}
    }

    set fd [open "|gcc -E /System/Library/Frameworks/OpenGL.framework/Headers/gl.h" r]
    set data [read $fd]
    close $fd

    set gltypedefs [list]

    #Find the typedefs after preprocessing.
    foreach {allmatch def} [regexp -inline -all {(typedef.*?\;)} $data] {
	lappend gltypedefs $def
    }

    set gldecls [list]
    set glstructdecls [list]
  
    foreach glist $glfunclist {
	lassign $glist rettype name arglist 

	set argstr ""
	foreach arg $arglist {
	    lassign $arg type var
	    if {"" eq $type} {
		#probably void
		continue
	    }
	    append argstr "$type $var, "
	}
	set argstr [string trimright $argstr ", "]

	lappend gldecls "$rettype [set name]($argstr);"
	set structname [string range $name 2 end]
       	lappend glstructdecls "$rettype (*[set structname])([arglist-to-c $arglist]);"	
    }

    #Now generate include/GL/gl.h using the information gathered from the system.
    set fd [open include/GL/gl.h w]

    puts $fd "/* This file was automatically generated with apple_api_generator.tcl. */"
    puts $fd $gl_license\n\n

    puts $fd "#ifndef __gl_h_"
    puts $fd "#define __gl_h_"
    
    foreach def $gldefs {
	puts $fd $def
    }

    puts $fd \n\n

    foreach tdef $gltypedefs {
	puts $fd $tdef
    }

    puts $fd \n\n

    #puts $fd "#include \"/System/Library/Frameworks/OpenGL.framework/Headers/glext.h\""
    
    puts $fd \n\n
    
    foreach decl $gldecls {
	puts $fd $decl
    }

    puts $fd "#endif /*__gl_h_*/"

    close $fd

    #Now we can generate the apple_api.h (the giant dispatch struct).
    set fd [open apple_api.h w]
   
    puts $fd "/* This file was automatically generated with apple_api_generator.tcl. */"
    puts $fd $apple_license

    puts $fd "#ifndef APPLE_API_H"
    puts $fd "#define APPLE_API_H"
    
    puts $fd "#ifdef __cplusplus"
    puts $fd "extern \"C\" \{"
    puts $fd "#endif"

    puts $fd "struct apple_api \{"

    foreach decl $glstructdecls {
	puts $fd \t$decl
    }
    
    puts $fd "\};"

    puts $fd "extern struct apple_api __gl_api;"

    puts $fd "extern void apple_api_init_direct(void);"

    puts $fd "#ifdef __cplusplus"
    puts $fd "\}"
    puts $fd "#endif"

    puts $fd "#endif /*APPLE_API_H*/"

    close $fd

    #Generate the code that provides the initialization for the direct calls.
    set fd [open apple_api.c w]
    
    puts $fd "/* This file was automatically generated with apple_api_generator.tcl. */"
    puts $fd $apple_license

    puts $fd "#include <dlfcn.h>"
    puts $fd "#include \"glxclient.h\""
    puts $fd "#include \"apple_api.h\""
    puts $fd "#include \"apple_context.h\""
    puts $fd "struct apple_api __gl_api;"

    #Generate the API functions.
    foreach fdef $glfunclist {
	lassign $fdef rettype glname arglist
	
	puts $fd "$rettype [set glname]([arglist-to-c $arglist]) \{"
	if 0 {
	    puts $fd {
		GLXContext gc;
		GLIContext ctx;
		gc = __glXGetCurrentContext(); 
		ctx = apple_context_get_gli_context(gc->apple);
	    }
	}

	set op "\t"
	if {"void" ne $rettype && "GLvoid" ne $rettype} {
	    set op "\treturn "
	}

	puts -nonewline $fd "$op __gl_api.[string range $glname 2 end]([arglist-to-c-call $arglist]);"
	puts $fd "\n\}"
    }

    puts $fd \n\n
    #Generate the API initalization function.
    puts $fd "void apple_api_init_direct(void) \{"
    puts $fd {
	void *h;
	
#ifndef LIBGLNAME
#define LIBGLNAME "/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib"
#endif LIBGLNAME

	/*warning: dlerror is known to not be thread-safe in POSIX. */
	(void)dlerror(); /*drain dlerror();*/

	h = dlopen(LIBGLNAME, RTLD_LAZY);
	if(NULL == h) {
	    fprintf(stderr, "error: unable to dlopen " LIBGLNAME " : " "%s\n",
		    dlerror());
	    abort();
	}
    }

    foreach fdef $glfunclist {
	lassign $fdef rettype glname arglist
	
	set structname [string range $glname 2 end]

	puts $fd "\t__gl_api.[set structname] = dlsym(h, \"$glname\");"
	puts $fd "\tif(NULL == __gl_api.[set structname]) \{"
	puts $fd "\t\tfprintf(stderr, \"symbol not found: %s.  Error: %s\\n\", \"$glname\", dlerror());"
	puts $fd "\t\tabort();"
	puts $fd "\t\}"
    }

    puts $fd "\}\n"

    close $fd

    set fd [open apple_exports.list w]
    foreach fdef $glfunclist {
	lassign $fdef rettype glname arglist
	puts $fd _$glname
    }

    #Start with 1.0
    set glxlist [list \
		     glXChooseVisual glXCreateContext glXDestroyContext \
		     glXMakeCurrent glXCopyContext glXSwapBuffers \
		     glXCreateGLXPixmap glXDestroyGLXPixmap \
		     glXQueryExtension glXQueryVersion \
		     glXIsDirect glXGetConfig \
		     glXGetCurrentContext glXGetCurrentDrawable \
		     glXWaitGL glXWaitX glXUseXFont]

    #GLX 1.1 and later
    lappend glxlist glXQueryExtensionsString glXQueryServerString \
		     glXGetClientString

    #GLX 1.2 and later
    lappend glxlist glXGetCurrentDisplay

    #GLX 1.3 and later
    lappend glxlist glXChooseFBConfig glXGetFBConfigAttrib \
	glXGetFBConfigs glXGetVisualFromFBConfig \
	glXCreateWindow glXDestroyWindow \
	glXCreatePixmap glXDestroyPixmap \
	glXCreatePbuffer glXDestroyPbuffer \
	glXQueryDrawable glXCreateNewContext \
	glXMakeContextCurrent glXGetCurrentReadDrawable \
	glXQueryContext glXSelectEvent glXGetSelectedEvent

    #GLX 1.4 and later
    lappend glxlist glXGetProcAddress
	
    foreach sym $glxlist {
	if {![string match glX* $sym]} {
	    return -code error "invalid symbol: $sym"
	}
	puts $fd _$sym
    }
		     
    close $fd

    exit 0
}
main $::argv
