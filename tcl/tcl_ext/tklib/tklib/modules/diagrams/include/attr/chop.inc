[def "[cmd chop] [opt [arg length]]"]

Specifies the length of the [cmd line] element to remove from the
beginning and/or end.

Defaults to nothing.

If specified once the chopping applies to both beginning and end of
the line.

If specified twice or more the last two specifications are used, and
applied to beginning and end of the line, in this order.

Whenever the attribute is specified without an explicit length, the
system falls back to the value taken from the language variable
[var circleradius], which itself defaults to the pixel equivalent of
[const {1 cm}]
