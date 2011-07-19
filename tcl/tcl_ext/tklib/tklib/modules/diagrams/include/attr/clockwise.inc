[def [cmd clockwise]]
[def [cmd cw]]

Specifies the direction of the [cmd arc] element, here going
clockwise.

The complementary attribute is [cmd counterclockwise].

If not specified the system falls back to the value taken from the
language variable [var clockwise], which itself defaults to 
[const false], for counter-clockwise direction.
