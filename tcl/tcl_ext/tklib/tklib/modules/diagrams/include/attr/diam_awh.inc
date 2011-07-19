
[para]

If both [cmd width], and [cmd height] are specified then any
specification of [cmd aspect] is ignored, as it is implicitly defined
in the width and height as well, and this takes precedence. A missing
specification is ignored in that case well, i.e. no defaults are
required.

[para]

If the [cmd aspect] is specified, and one of the attributes
[cmd width] or [cmd height], then the missing attribute is calculated
from the two which are specified. No defaults are required for these
cases either.

[para]

If only one of the attributes [cmd width] or [cmd height] is specified
then the system uses a fallback for the [cmd aspect], the value taken
from the language variable [var diamondaspect], which itselfs defaults
to [const 2].

[para]

If none of of the attributes [cmd width] or [cmd height] is specified
then the system uses a fallback for the [cmd width], the value taken
from the language variable [var boxwidth], which itselfs defaults to
the pixel equivalent of [const {2 cm}]. For the aspect it uses either
the user-specified value or the default taken as described in the
previous paragraph.
