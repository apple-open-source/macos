[call [cmd move] [arg attr]]

[include cat_open.inc]

A [cmd move] element is in essence an invisible [cmd line].

While the main effect we are interested in is the change it makes to
the layout system, it is an actual element, i.e. it has the same
corners as an ordinary line, and shows up in the history as well,
allowing future references to all the places it touched.

It handles the same attibutes as [cmd line] elements.
