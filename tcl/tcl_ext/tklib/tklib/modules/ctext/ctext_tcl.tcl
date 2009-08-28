# RCS: @(#) $Id: ctext_tcl.tcl,v 1.2 2005/03/31 03:15:48 andreas_kupries Exp $ 

package provide ctext_tcl 0.8

proc ctext::setHighlightTcl {w} {
    set color(widgets) red3
    set color(flags) orange3
    set color(stackControl) red
    set color(vars) magenta4
    set color(variable_funcs) red4
    set color(brackets) DeepPink
    set color(comments) blue4
    set color(strings) green4

    ctext::addHighlightClass $w widgets $color(widgets) \
	[list obutton button label text frame toplevel cscrollbar \
	     scrollbar checkbutton canvas listbox menu menubar menubutton \
	     radiobutton scale entry message tk_chooseDir tk_getSaveFile \
	     tk_getOpenFile tk_chooseColor tk_optionMenu]

    ctext::addHighlightClass $w flags $color(flags) \
	[list -text -command -yscrollcommand \
	     -xscrollcommand -background -foreground -fg -bg \
	     -highlightbackground -y -x -highlightcolor -relief -width \
	     -height -wrap -font -fill -side -outline -style -insertwidth \
	     -textvariable -activebackground -activeforeground \
	     -insertbackground -anchor -orient -troughcolor -nonewline \
	     -expand -type -message -title -offset -in -after -yscroll \
	     -xscroll -forward -regexp -count -exact -padx -ipadx \
	     -filetypes -all -from -to -label -value -variable \
	     -regexp -backwards -forwards -bd -pady -ipady -state -row \
	     -column -cursor -highlightcolors -linemap -menu -tearoff \
	     -displayof -cursor -underline -tags -tag -length]
    
    ctext::addHighlightClass $w stackControl $color(stackControl) \
	[list proc uplevel namespace while for foreach if else]
    ctext::addHighlightClassWithOnlyCharStart $w vars $color(vars) "\$"
    ctext::addHighlightClass $w variable_funcs $color(variable_funcs) \
	[list set global variable unset]
    ctext::addHighlightClassForSpecialChars $w brackets $color(brackets) {[]{}}
    ctext::addHighlightClassForRegexp $w comments $color(comments) {\#[^\n\r]*}
    ctext::addHighlightClassForRegexp $w strings $color(strings) {"(\\"|[^"])*"}
}
