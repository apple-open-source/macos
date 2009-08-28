[comment {-*- tcl -*- doctools = compress/options.inc}]

[lst_item "[option -mode] [const compress]|[const decompress]"]

This option has to be present and is always understood by the
compression.

[nl]

For [term immediate] mode the argument value specifies the operation
to use.  For an [term attached] compress it specifies the operation to
use for [emph writing]. Reading will automatically use the reverse
operation.

See section [sectref {IMMEDIATE versus ATTACHED}] for explanations of
these two terms.

[nl]

Beyond the argument values listed above all unique abbreviations are
recognized too.

[nl]

[const Compress] causes the compression of arbitrary (most likely
binary) data. [const Decompression] does the reverse .

[lst_item "[option -level] [arg integer]"]

Specifies the compression level. Is either the string [const default]
or an integer number in the range [const 1] (minimal compression) to
[const 9] (maximal compression).
