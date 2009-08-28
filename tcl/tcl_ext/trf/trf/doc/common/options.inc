[comment {-*- tcl -*- doctools = common_options.inc}]

[lst_item "[option -attach] [arg channel]"]

The presence/absence of this option determines the main operation mode
of the transformation.

[nl]

If present the transformation will be stacked onto the [arg channel]
whose handle was given to the option and run in [term attached]
mode. More about this in section [sectref {IMMEDIATE versus ATTACHED}].

[nl]

If the option is absent the transformation is used in [term immediate]
mode and the options listed below are recognized. More about this in
section [sectref {IMMEDIATE versus ATTACHED}].


[lst_item "[option -in] [arg channel]"]

This options is legal if and only if the transformation is used in
[term immediate] mode. It provides the handle of the channel the data
to transform has to be read from.

[nl]

If the transformation is in [term immediate] mode and this option is
absent the data to transform is expected as the last argument to the
transformation.


[lst_item "[option -out] [arg channel]"]

This options is legal if and only if the transformation is used in
[term immediate] mode. It provides the handle of the channel the
generated transformation result is written to.

[nl]

If the transformation is in [term immediate] mode and this option is
absent the generated data is returned as the result of the command
itself.
