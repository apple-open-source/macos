if 0 {
 api_to_code.tcl -- This generates the API frontend functions for libGL.

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

set ::template {
    _GLXcontext *gc;

    gc = __glXGetCurrentContext();

    if(gc->isDirect) {
	OPTIONAL_RETURN gc->apple.contextobj->disp.MEMBER(gc->apple.contextobj, ARGUMENTS);
    } else {
	struct _glapi_table *disp = _glapi_dispatch;
	if(__builtin_expect (disp == NULL, 0)) {
	    /* TODO verify that _glapi_get_dispatch aborts if it fails.
	     * otherwise we get a segfault here after this.
	     */
            disp = _glapi_get_dispatch (); 
	}
	
	OPTIONAL_RETURN disp.INDIRECT_MEMBER(ARGUMENTS);
    }
}

proc main {argc argv} {
    global template

    set fd [open [lindex $argv 0] r]
    set data [read $fd] 
    close $fd

    set outfd [open [lindex $argv 1] w]
      
    foreach flist [split $data \n] {
	foreach {rettype name argpat member indirect_member} $flist {
	    puts $outfd "$rettype gl[set name]([join $argpat ", "]) \{"

	    set optreturn ""
	    if {$rettype ne "void"} {
		set optreturn return
	    }

	    set arguments ""
	    foreach typevar $argpat {
		set var [lindex $typevar 1]
	       	append arguments "$var, "
	    }
	    set arguments [string trimright $arguments ", "]

	    puts $outfd [string map \
			     [list OPTIONAL_RETURN $optreturn INDIRECT_MEMBER $indirect_member \
				  MEMBER $member ARGUMENTS $arguments] $template]

	    puts $outfd "\}"
   	}
    }
    close $outfd
    exit 0
}
main $::argc $::argv
