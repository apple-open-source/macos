[def "[cmd style] [arg spec]"]

[image figure-18-style-dash]
Specifies the style used to draw the lines of the element.

If not specified the system falls back to the value taken from the
language variable [var linestyle], which itself defaults to
[const solid] lines.

[include ../values/style.inc]

Note that the values "solid", "dot(ted)", "dash(ed)", "dash-dot", and
"dash-dot-dot" are all accepted as shorthands for the [cmd style]
command using them as argument.
