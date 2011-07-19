[def "[cmd anchor] [arg name]"]
[def [cmd ljust]]
[def [cmd rjust]]
[def [cmd above]]
[def [cmd below]]

[image figure-22-text-anchoring-3]
Specifies the anchor of the text which is to be placed at the
element's center, by name. I.e. this attribute defines the text's
position relative to the element's center.

The value is ignored if no text was specified for the element.

If not specified the system falls back to the value taken from the
language variable [var anchor], which itself defaults to
[const center].

[include ../values/anchor.inc]

The commands without arguments are all shorthands with the anchor
implied. Note that they do not combine, only the last is used. For
comined directions the main attribute command, [cmd anchor] has to be
used.

[list_begin definitions]
[def [cmd ljust]]	"anchor west"
[def [cmd rjust]]	"anchor east"
[def [cmd above]]	"anchor south"
[def [cmd below]]	"anchor north"
[list_end]