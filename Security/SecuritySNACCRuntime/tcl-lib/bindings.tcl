# file: bindings.tcl

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
proc bit_string_entry_bindings {entry} \
{
  bind $entry <Any-Key> { }
#  bind $entry <Key-Return> {puts return}
  bind $entry <Key-0> {%W insert insert %A}
  bind $entry <Key-1> {%W insert insert %A}
  bind $entry <Control-u> [bind Entry <Control-u>]
  bind $entry <Control-v> [bind Entry <Control-v>]
  bind $entry <Control-d> [bind Entry <Control-d>]
  bind $entry <Delete> [bind Entry <Delete>]
  bind $entry <Backspace> [bind Entry <Backspace>]
  bind $entry <Left> {%W icursor [expr [%W index insert] -1]}
  bind $entry <Right> {%W icursor [expr [%W index insert] +1]}
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
proc int_entry_bindings {entry} \
{
  bit_string_entry_bindings $entry
  for {set i 2} {$i < 10} {incr i} \
  {
    bind $entry <Key-$i> {%W insert insert %A}
    bind $entry <Key-KP_$i> {%W insert insert %A}
  }
  bind $entry <Key-minus> {%W insert insert %A}
  bind $entry <Key-KP_Subtract> {%W insert insert %A}
}
