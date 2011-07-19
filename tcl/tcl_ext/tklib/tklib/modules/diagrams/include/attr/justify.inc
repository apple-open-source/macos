[def "[cmd justify] [const left]|[const center]|[const right]"]

Specifies how multi-line text associated with the element is
positioned within its box.

The value is ignored if no text was specified for the element.

If not specified the system falls back to the value taken from the
language variable [var justify], which itself defaults to
[const left].

[include ../values/justify.inc]
