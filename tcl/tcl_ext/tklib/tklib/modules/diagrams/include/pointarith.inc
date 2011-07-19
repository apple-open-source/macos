[call [arg number] [arg number]]

[para][image figure-50-point-cons-absolute][para]

This command takes the x and y coordinates of a location and returns
the [term absolute] point for it.


[call [cmd by] [arg distance] [arg direction]]

[para][image figure-51-point-cons-relative][para]

This command takes a [arg distance] and [arg direction] (angle in
degress, or registered direction name) and returns the [term relative]
point for it, i.e. the [term delta] or [term translation] it
represents.

[para]
Note also the (dis)similarities to the directional specifications for
the attribute [cmd then] of [cmd line] and [cmd move] elements.

Where we say here
[example {by 50 east}]
for the attribute we say
[example {... then east 50 ...}]
or just
[example {... then east ...}]


[call [arg point1] [cmd +] [arg point2]]

[para][image figure-48-point-vectoradd][para]

This command interprets two points as vectors and adds them together.

If at least one of the points is [term absolute] the result is
absolute as well.

The result is a [term relative] point if and only if both points are
[term relative].


[call [arg point1] [cmd -] [arg point2]]

[para][image figure-49-point-vectorsub][para]

This command interprets two points as vectors and subtracts the second
from the first.

If at least one of the points is [term absolute] the result is
absolute as well.

The result is a [term relative] point if and only if both points are
[term relative].


[call [arg point] [cmd by] [arg distance] [arg direction]]

This command is a more convenient, or at least shorter, form of

[example {
    [$point + [by $distance $direction]]
}]


[call [arg point1] [cmd |] [arg point2]]

[para][image figure-31-point-projection][para]

This command calculates the [term projection] of two points, i.e. the
result is the point having the x-coordinate of [arg point1] and the
y-coordinate of [arg point2].


[call [arg n] [cmd between] [arg poin1] [arg point2]]

[para][image figure-29-point-interpolation-1][para]

This command computes the point which is [arg n]*100 percent of the
way between [arg point1] and [arg point2], and returns it as its
result.

This means that for
[list_begin definitions]
[def "[arg n] == 0"] The result is [arg point1].
[def "[arg n] == 1"] The result is [arg point2].
[def "[arg n] == 0.5"] The result is half way between the two points.
[list_end]
etc.

[emph Note] that it is allowed to use values < 0 and > 1 for [arg n]


[call [cmd intersect] [arg elem1] [arg elem2]]

[para][image figure-32-point-intersection][para]

This command takes two [term open] elements, computes the lines going
through their "start"- and "end"-corners, and returns the point where
these two lines intersect.

The command throws an error if the lines do not intersect, or are
coincident.
