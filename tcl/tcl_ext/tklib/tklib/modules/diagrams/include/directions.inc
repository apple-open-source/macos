
The named directions are commands which tell the layout system in
which direction to go when placing the next element without an
explicit position specification.

They can also be used as arguments to the attribute [cmd then], and
the command [cmd by] for relative points, see there for the relevant
syntax.

[para]

The diagram core defines the directions of the compass rose, plus a
number of aliases. See below for the full list.

[para][image figure-27-corners-closed][para]

This overlaps with the pre-defined corners for closed elements. This
is used by the layout system, when are going in direction X the name
of the opposite direction is the name of the corner at which the new
element will be attached to the current position, and if this corner
does not exist the nearest actual corner by angle is used.

[para]

[list_begin definitions]
[call [cmd west]]
[call [cmd w]]
[call [cmd left]]
[call [cmd l]]

[call [cmd south]]
[call [cmd s]]
[call [cmd down]]
[call [cmd bottom]]
[call [cmd bot]]
[call [cmd b]]

[call [cmd east]]
[call [cmd e]]
[call [cmd right]]
[call [cmd r]]

[call [cmd north]]
[call [cmd n]]
[call [cmd up]]
[call [cmd top]]
[call [cmd t]]

[call [cmd northwest]]
[call [cmd nw]]
[call [cmd up-left]]
[call [cmd upleft]]
[call [cmd leftup]]

[call [cmd northeast]]
[call [cmd ne]]
[call [cmd up-right]]
[call [cmd upright]]
[call [cmd rightup]]

[call [cmd southwest]]
[call [cmd sw]]
[call [cmd down-left]]
[call [cmd downleft]]
[call [cmd leftdown]]

[call [cmd southeast]]
[call [cmd se]]
[call [cmd down-right]]
[call [cmd downright]]
[call [cmd rightdown]]
[list_end]
