[call [arg element] [cmd names] [opt [arg pattern]]]

This command returns a list containing the names of all corners for
the [arg element]. If a pattern is specified then only the names
matching it (via [cmd {string match}] are returned. Otherwise all
names are returned (equivalent to a default pattern of [const *]).


[call [arg element] [arg corner]]

This command returns the value for the [arg corner] of the
[arg element].

This can be anything, including points and elements.


[call [arg element] [arg corner1] [arg corner2]...]

This is a convenience shorthand for

[example {
[[[$elem $corner1] $corner2] ...]
}]

assuming that the value for [example { [$elem $corner1] }], etc. is
again an element.

[call [arg element] [opt "[arg corner1]... [opt "[method names] [opt [arg pattern]]"]]"]]

This is a convenience shorthand for

[example {
[[[$elem $corner1] ...] names ?pattern?]
}]

assuming that the value for [example { [$elem $corner1] }], etc. is
again an element.
