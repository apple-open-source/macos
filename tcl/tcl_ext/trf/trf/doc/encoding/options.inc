[comment {-*- tcl -*- doctools = encoding_options.inc}]

[lst_item "[option -mode] [const encode]|[const decode]"]

This option has to be present and is always understood by the
encoding.

[nl]

For [term immediate] mode the argument value specifies the operation
to use.  For an [term attached] encoding it specifies the operation to
use for [emph writing]. Reading will automatically use the reverse
operation.

See section [sectref {IMMEDIATE versus ATTACHED}] for explanations of
these two terms.

[nl]

Beyond the argument values listed above all unique abbreviations are
recognized too.

[nl]

[const Encode] converts from arbitrary (most likely binary) data into
the described representation, [const decode] does the reverse .
