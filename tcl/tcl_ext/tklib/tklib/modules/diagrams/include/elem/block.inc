[call [cmd block] [arg script] [arg attr]...]

[include cat_closed.inc]

The main effect is the aggregration of all elements created by the
[arg script] into one element.

This also means that while the elements created by the script are
visible in the element history while the script is executing,
afterward the history contains only the block itself, and not the
elements it is composed of.

[para]

The script has access to the current state of all variables in the
language context.

Any changes to the variables will be reverted after execution of the
block.

However, also, any variables set in the script will be exported as
corners of the element, allowing users to define their own named
locations in the block.

[para]

Regarding the layout mechanism any changes made by the script are
reverted after the element is done.

In other words, a block is an implicit [cmd group].

[para]

Blocks handle all attributes, propgating their settings into the
script as the default values active during script execution.
