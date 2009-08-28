#
# tkphoto.tcl - access photo images via ffidl
#
package provide Tkphoto 0.1
package require Ffidl
package require Ffidlrt
package require Tk

# define a type alias for Tk_PhotoHandle
ffidl::typedef Tk_PhotoHandle pointer

# define a structure for Tk_PhotoImageBlock
ffidl::typedef Tk_PhotoImageBlock pointer int int int int int int int int

# bind to tk
ffidl::callout ffidl-find-photo {pointer pointer-utf8} Tk_PhotoHandle \
    [ffidl::stubsymbol tk stubs 64]; #Tk_FindPhoto
ffidl::callout ffidl-photo-put-block {Tk_PhotoHandle pointer-byte int int int int} void \
    [ffidl::stubsymbol tk stubs 246]; #Tk_PhotoPutBlock
ffidl::callout ffidl-photo-put-zoomed-block {Tk_PhotoHandle pointer-byte int int int int int int int int} void \
    [ffidl::stubsymbol tk stubs 247]; #Tk_PhotoPutZoomedBlock
ffidl::callout ffidl-photo-get-image {Tk_PhotoHandle pointer-var} int \
    [ffidl::stubsymbol tk stubs 146]; #Tk_PhotoGetImage
ffidl::callout ffidl-photo-blank {Tk_PhotoHandle} void \
    [ffidl::stubsymbol tk stubs 147]; #Tk_PhotoBlank
ffidl::callout ffidl-photo-expand {Tk_PhotoHandle int int} void \
    [ffidl::stubsymbol tk stubs 148]; #Tk_PhotoExpand
ffidl::callout ffidl-photo-get-size {Tk_PhotoHandle pointer-var pointer-var} void \
    [ffidl::stubsymbol tk stubs 149]; #Tk_PhotoGetSize
ffidl::callout ffidl-photo-set-size {Tk_PhotoHandle int int} void \
    [ffidl::stubsymbol tk stubs 150]; #Tk_PhotoSetSize

# use the ffidl::info format for Tk_PhotoImageBlock to get the fields
proc ffidl-photo-block-fields {pib} {
    binary scan $pib [ffidl::info format Tk_PhotoImageBlock] \
	pixelPtr width height pitch pixelSize red green blue reserved
    list $pixelPtr $width $height $pitch $pixelSize $red $green $blue $reserved
}
# define accessors for the fields
foreach {name offset} {
    pixelPtr 0 width 1 height 2 pitch 3 pixelSize 4 red 5 green 6 blue 7 reserved 8
} {
    proc ffidl-photo-block.$name {pib} "lindex \[ffidl-photo-block-fields \$pib] $offset"
}

proc ffidl-photo-get-block-bytes {block} {
    set nbytes [expr {[ffidl-photo-block.height $block]*[ffidl-photo-block.pitch $block]}]
    set bytes [binary format x$nbytes]
    ffidl::memcpy bytes [ffidl-photo-block.pixelPtr $block] $nbytes
    set bytes
}

		
