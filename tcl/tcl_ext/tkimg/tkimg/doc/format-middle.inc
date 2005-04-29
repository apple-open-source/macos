[comment {-*- tcl -*- doctools = format-middle.inc}]
[para]

All of the above means that in a call like

[list_begin definitions]
[call [cmd image] [method {create photo}] [opt [arg name]] [opt [arg options]]]
[list_end]

[list_begin enum]
[enum]
Image data in [vset FORMATNAME] format (options [option -data] and [option -file]) is
detected automatically.

[enum]
The format name [const [vset FORMATNAME]] is recognized by the option [option -format].
