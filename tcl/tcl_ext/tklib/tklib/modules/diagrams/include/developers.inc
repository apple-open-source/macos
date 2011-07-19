The intended audience of this section are developers wishing to work
on the internals of the diagram package.

Regular users of [package diagram] can skip this section without
missing anything.

[para]

The main information seen here is the figure below, showing the
hierarchy of the classes implementing diagram.

[para][image figure-00-dependencies][para]

At the bottom, all at the same level are the supporting packages like
[package snit], etc. These can all be found in Tcllib.

[para]

Above them is the set of diagram classes implementing the various
aspects of the system, i.e.:

[list_begin definitions]

[def [class diagram]]
The main class, that which is seen by the user.

[def [class diagram::core]]
The core engine, itself distributed over four helper classes.

[def [class diagram::basic]]
The implementation of the standard shapes, like box, circle, etc.,
based on the extension features of the core.

[def [class diagram::element]]
Core support class, the database of created elements. It also keeps
the history, i.e. the order in which elements were created.

[def [class diagram::attribute]]
Core support class, the generic handling of definition and processing
of attributes.

[def [class diagram::direction]]
Core support class, the database of named directions.

[def [class diagram::navigation]]
Core support class, the state of layout engine, i.e. current position
and directin, and operations on it.

[def [class diagram::point]]
General support class handling various vector operations.

[list_end]
