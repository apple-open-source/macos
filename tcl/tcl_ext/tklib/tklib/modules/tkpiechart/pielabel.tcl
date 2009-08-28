# copyright (C) 1995-2004 Jean-Luc Fontaine (mailto:jfontain@free.fr)

package require Tk 8.3
package require stooop


::stooop::class pieLabeler {

    set (default,font) {Helvetica -12}

    proc pieLabeler {this canvas args} {
        ::set ($this,canvas) $canvas
    }

    proc ~pieLabeler {this} {}

    ::stooop::virtual proc new {this slice args}    ;# must return a canvasLabel

    ::stooop::virtual proc delete {this label}

    ::stooop::virtual proc set {this label value}

    ::stooop::virtual proc label {this args} ;# set or get label if no arguments

    # set or get label background if no arguments
    ::stooop::virtual proc labelBackground {this args}

    # set or get text label background if no arguments
    ::stooop::virtual proc labelTextBackground {this args}

    ::stooop::virtual proc selectState {this label {state {}}}

    # must be invoked only by pie, which knows when it is necessary to update
    # (new or deleted label, resizing, ...):
    ::stooop::virtual proc update {this left top right bottom}
    # for the labelers that need to know when slices are updated:
    ::stooop::virtual proc updateSlices {this left top right bottom} {}

    ::stooop::virtual proc room {this arrayName}

}
