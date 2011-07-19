
[subsection Elements]

This section lists the commands for the predefined drawing elements,
aka shapes. These commands are all defined in the language's context.

All commands of this section return the handle of the newly created
element as their result. This handle also exists as a command which
can be used to query the element for its corners (names, values).

See section [sectref {Miscellaneous Commands}].

[image figure-02-basic-shapes]

[list_begin definitions]
[include elem/arc.inc]
[include elem/arrow.inc]
[include elem/block.inc]
[include elem/box.inc]
[include elem/circle.inc]
[include elem/diamond.inc]
[include elem/drum.inc]
[include elem/ellipse.inc]
[include elem/line.inc]
[include elem/move.inc]
[include elem/spline.inc]
[include elem/text.inc]
[list_end]


[subsection Attributes]

The set of all attributes supported by all the element commands is
shown below.

While we speak of them as commands, and provide a syntax, they are not
truly available as actual commands, but only as part of the arguments
for an element command.

[para]

Note that some of the attribute names are overloaded, i.e. have
multiple, different, definitions. During processing of attributes for
an element the actual definition used is chosen based on the type of
the element the processing is for.

[para]

Further, as a catch-all clause, any attribute which could not be
processed according to the definitions below will be treated as the
argument of an implicit [cmd text] attribute.

[list_begin definitions]
[include attr/anchor.inc]
[include attr/arrowhead.inc]
[include attr/diam_aspect.inc]
[include attr/drum_aspect.inc]
[include attr/at.inc]
[include attr/line_at.inc]
[include attr/chop.inc]
[include attr/clockwise.inc]
[include attr/color.inc]
[include attr/counterclockwise.inc]
[include attr/diameter.inc]
[include attr/fillcolor.inc]
[include attr/from.inc]
[include attr/arc_from.inc]
[include attr/height.inc]
[include attr/diam_height.inc]
[include attr/text_height.inc]
[include attr/justify.inc]
[include attr/noturn.inc]
[include attr/circle_radius.inc]
[include attr/arc_radius.inc]
[include attr/slant.inc]
[include attr/smooth.inc]
[include attr/stroke.inc]
[include attr/style.inc]
[include attr/text.inc]
[include attr/textcolor.inc]
[include attr/textfont.inc]
[include attr/then.inc]
[include attr/to.inc]
[include attr/arc_to.inc]
[include attr/width.inc]
[include attr/diam_width.inc]
[include attr/text_width.inc]
[include attr/with.inc]
[include attr/line_with.inc]
[list_end]


[subsection Corners]
[include corners.inc]

[subsection {Named directions}]
[include directions.inc]

[subsection {Miscellaneous Commands}]

[list_begin definitions]
[include distances.inc]
[include pointarith.inc]
[include elementq.inc]
[include history.inc]
[list_end]

[subsection {Variables}]

The language context contains a number of predefined variables which
hold the default values for various attributes. These variables, their
uses, and values are:

[list_begin definitions]
[include var/anchor.inc]
[include var/arcradius.inc]
[include var/arrowhead.inc]
[include var/boxheight.inc]
[include var/boxwidth.inc]
[include var/clockwise.inc]
[include var/circleradius.inc]
[include var/drumaspect.inc]
[include var/fillcolor.inc]
[include var/justify.inc]
[include var/linecolor.inc]
[include var/linestyle.inc]
[include var/linewidth.inc]
[include var/movelength.inc]
[include var/slant.inc]
[include var/textcolor.inc]
[include var/textfont.inc]
[list_end]
