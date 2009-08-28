[comment {-*- tcl -*- doctools = common_sections.inc}]

[section {IMMEDIATE versus ATTACHED}]

The transformation distinguishes between two main ways of using
it. These are the [term immediate] and [term attached] operation
modes.

[para]

For the [term attached] mode the option [option -attach] is used to
associate the transformation with an existing channel. During the
execution of the command no transformation is performed, instead the
channel is changed in such a way, that from then on all data written
to or read from it passes through the transformation and is modified
by it according to the definition above.

This attachment can be revoked by executing the command [cmd unstack]
for the chosen channel. This is the only way to do this at the Tcl
level.

[para]

In the second mode, which can be detected by the absence of option
[option -attach], the transformation immediately takes data from
either its commandline or a channel, transforms it, and returns the
result either as result of the command, or writes it into a channel.

The mode is named after the immediate nature of its execution.

[para]

Where the data is taken from, and delivered to, is governed by the
presence and absence of the options [option -in] and [option -out].

It should be noted that this ability to immediately read from and/or
write to a channel is an historic artifact which was introduced at the
beginning of Trf's life when Tcl version 7.6 was current as this and
earlier versions have trouble to deal with \0 characters embedded into
either input or output.
