[call [arg number] [const cm]]
[call [arg number] [const mm]]
[call [arg number] [const inch]]
[call [arg number] [const pt]]

These commands allow the specification of distances and coordinates in
metric and imperial units, returning the equivalent distance or
coordinate in pixels, which is the unit used internally for all
calculations.

[para]

The conversion factors are based on the result of [cmd {tk scaling}]
and are computed once, at the time the package is sourced, future
changes of the [cmd {tk scaling}] factor have no effect.
