[def [cmd counterclockwise]]
[def [cmd ccw]]

Specifies the direction of the [cmd arc] element, here
counter-clockwise.

The complementary attribute is [cmd clockwise].

If not specified the system falls back to the value taken from the
language variable [var clockwise], which itself defaults to 
[const false], for counter-clockwise direction.
