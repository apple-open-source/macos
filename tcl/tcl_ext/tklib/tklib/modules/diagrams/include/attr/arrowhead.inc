[def "[cmd arrowhead] [arg spec]"]

[image figure-19-style-arrowheads]
Specifies where to draw arrowheads on the [cmd line] element, at the
beginning or end, at both ends, or none.

If not specified the system falls back to the value taken from the
language variable [var arrowhead], which itself defaults to
[const none].

[include ../values/arrowhead.inc]

Note that the values "start", "end", "-", "->", "<-", and "<->" are
all accepted as shorthands for the [cmd arrowhead] command using them
as argument.
