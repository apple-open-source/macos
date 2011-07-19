[def "[cmd with] [arg corner]"]

[cmd Line] elements are normally positioned absolutely, using the
locations specified through the attributes [cmd from], [cmd then], and
[cmd to].

If [cmd at] is specified however then these positions are translated a
last time, moving the line's corner named by the attribute [cmd with]
to the location given by this attribute.

This means that [arg with] is effective if and only if the attribute
[cmd at] was specified as well for the line.
