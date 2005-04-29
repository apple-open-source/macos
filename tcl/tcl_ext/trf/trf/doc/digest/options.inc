[comment {-*- tcl -*- doctools = digest_options.inc}]

[lst_item "[option -mode] [const absorb]|[const write]|[const transparent]"]

This option has to be present. The specified argument determines the
behaviour of the digest in [term attached] mode.

[nl]

Beyond the argument values listed above all unique abbreviations are
recognized too. Their meaning is explained below:

[list_begin definitions]
[lst_item [const absorb]]

All data written to the channel is used to calculate the value of the
message digest and then passed unchanged to the next level in the
stack of transformations for the channel the digest is attached to.

When the channel is closed the completed digest is written out too,
essentially attaching the vlaue of the diggest after the information
actually written to the channel.

[nl]

When reading from the channel a value for the digest is computed too,
and after closing of the channel compared to the digest which was
attached, i.e. came behind the actual data.

The option [option -matchflag] has to be specified so that the digest
knows where to store the result of said comparison. This result is a
string and either "[const ok]", or "[const failed]".


[lst_item [const write]]

All data read from or written to the channel the digest is attached to
is ignored and thrown away. Only a value for the digest of the data is
computed.

When the channel is closed the computed values are stored as ordered
through the options [option -write-destination], [option -write-type],
[option -read-destination], and [option -read-type].


[lst_item [const transparent]]

This mode is a mixture of both [const absorb] and [const write]
modes. As for [const absorb] all data, read or written, passes through
the digest unchanged. The generated values for the digest however are
handled in the same way as for [const write].

[list_end]
[nl]

[lst_item "[option -matchflag] [arg varname]"]

This option can be used if and only if the option "[option -mode]
[const absorb]" is present. In that situation the argument is the name
of a global or namespaced variable. The digest will write the result
of comparing two digest values into this variable. The option will be
ignored if the channel is write-only, because in that case there will
be no comparison of digest values.


[lst_item "[option -write-type] [const variable]|[const channel]"]

This option can be used for digests in mode [const write] or
[const transparent]. Beyond the values listed above all their unique
abbreviations are also allowed as argument values.

The option determines the type of the argument to option
[option -write-destination]. It defaults to [const variable].


[lst_item "[option -read-type] [const variable]|[const channel]"]

Like option [option -write-type], but for option [option -read-destination].


[lst_item "[option -write-destination] [arg data]"]

This option can be used for digests in mode [const write] or
[const transparent]. 

The value [arg data] is either the name of a global (or namespaced)
variable or the handle of a writable channel, dependent on the value
of option [option -write-type]. The message digest computed for data
written to the attached channel is written into it after the attached
channel was closed.

The option is ignored if the channel is read-only.

[nl]

[emph Note] that using a variable may yield incorrect results under
tcl 7.6, due to embedded \0's.


[lst_item "[option -read-destination] [arg data]"]

This option can be used for digests in mode [const write] or
[const transparent]. 

The value [arg data] is either the name of a global (or namespaced)
variable or the handle of a writable channel, dependent on the value
of option [option -read-type]. The message digest computed for data
read from the attached channel is written into it after the attached
channel was closed.

The option is ignored if the channel is write-only.

[nl]

[emph Note] that using a variable may yield incorrect results under
tcl 7.6, due to embedded \0's.
