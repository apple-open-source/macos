                           MacsBug Interface for GDB
                                    6/8/01

1. Introduction

   The "MacsBug" supported here is gdb extended to support a subset of the Mac
   Classic MacsBug commands and a MacsBug-like screen UI. Thus it basically is
   a variant of MacsBug with source-level debugging!

   Along with this README there are three other files in this directory:

   * gdbinit-MacsBug-without-plugin
   * gdbinit-MacsBug
   * MacsBug_plugin

   Both gdbinit-MacsBug-without-plugin and gdbinit-MacsBug are gdb command 
   language scripts one of which you SOURCE from your ~/.gdbinit script (the
   script that gdb always looks for, and for what it's worth, it looks for a
   .gdbinit script in the current directory as well).  In the following sections
   the difference in the two MacsBug scripts will be explained.
   
   The MacsBug_plugin is just that; the gdb plugin that implement the MacsBug
   UI and commands.  The plugin is loaded by the gdbinit-MacsBug script.


2. Background

   Once upon a time there was an implementation of Classic MacsBug for gdb
   using nothing but the command language provided by gdb.  The file was
   called "gdbinit-MacsBug" (and since renamed gdbinit-MacsBug-without-plugin)
   and it is included with this release. It supports about 40 of the MacsBug
   commands.

   It has the benefit of being totally portable, i.e., it should be able to be
   used with any version of gdb, on any Unix system, anywhere.  It's down side
   is that it cannot do any more that what is provided by the rather limited
   gdb command language, is interpretive, and thus rather inefficient and in
   some cases slow, and cannot provide a MacsBug-like screen UI.

   To get around these limitations the plugin support in Apple's gdb was
   utilized which allows the implementation be done in a "real" language.  Thus
   the script "gdbinit-MacsBug" is provided which appears to be a very reduced
   form or "gdbinit-MacsBug-without-plugin" because most of the implementation
   is provided in the plugin ("MacsBug_plugin") which "gdbinit-MacsBug" loads
   with a LOAD-PLUGIN Apple gdb command.

   The benefit of using the plugin is that it is very efficient because it
   "talks" directly to gdb.  It can provide functionality that the pure script
   variant cannot like some additional MacsBug commands and behavior as well
   as a MacsBug-like screen UI.  It's down side is of course that it is tied
   directly to the version of gdb (and system) for which it was built. Further,
   because the script uses the LOAD-PLUGIN gdb command it will only work with
   Apple's version of gdb. Since MacsBug is mainly used by Mac developers this
   should be not be a serious limitation.

   You thus have two choices:

   (1) Install the totally portable gdbinit-MacsBug-without-plugin script, or,
   (2) Install the gdbinit-MacsBug script and the MacsBug_plugin file.


3. Installation

   The MacsBug files should already be installed in,

      /usr/libexec/gdb/plugins/MacsBug

   If you elect choice (1) then just add the following to your ~/.gdbinit 
   script:

      source /usr/libexec/gdb/plugins/MacsBug/gdbinit-MacsBug-without-plugin

   If you want to use the plugin then add the following source command instead:

      source /usr/libexec/gdb/plugins/MacsBug/gdbinit-MacsBug

   The gdbinit-MacsBug script is installed with it's LOAD-PLUGIN command defined
   as follows:

      load-plugin /usr/libexec/gdb/plugins/MacsBug/MacsBug_plugin

   The load-plugin command will load the MacsBug_plugin from it's installed
   directory when "gdbinit-MacsBug" is read (i.e., when the SOURCE command in
   your ~/.gdbinit script is executed).
   
   Of course you don't have to use the files in their installed location. They
   can be anywhere so long as the ~/.gdbinit's SOURCE command can access the
   "gdbinit-MacsBug" file with an appropriate pathname and the "gdbinit-MacsBug"
   script's LOAD-PLUGIN command can, in turn, access the "MacsBug_plugin" file.
   The only restriction on the LOAD-PLUGIN command is that the specified
   pathname must be a full pathname ('~' cannot be used).

   From this point on this README assumes you are installing the plugin.


4. Using Gdb's HELP Commands For MacsBug

   Once gdb is loaded type HELP to see a summary of all gdb command classes.  
   There are three additional classes for MacsBug: "macsbug", "screen", and 
   "useful".  

   Typing "help macsbug" give a summary of all the MacsBug commands.

   Typing "help screen" give a summary of all MacsBug screen UI-related commands
   which are additions to this MacsBug implementation.  

   Typing "help useful" gives a list of "useful" gdb commands that could be
   used to extend the MacsBug script or for your own gdb DEFINE commands.
   Some of these are used internally, or are plugin descendants from the original
   gdb script, or are just plain useful in their own right and should probably
   be in gdb in the first place!.

   Two existing gdb help classes, "aliases" and "support" also list some MacsBug
   commands.  The "aliases" class lists aliases for some commands that don't
   follow the standard gdb command abbreviation conventions (e.g., SC6 is the
   same as SC).  The "support" class lists the SET options.

   As indicated at the end of the HELP class summary you can type "mb-notes" to
   get some additional information specifically related to this implementation
   of MacsBug.  These notes are also repeated in the next section.

   Finally, also as indicated in the HELP summary, typing "help" followed by a
   command name displays a summary for that specific command.


5. General Comments About Gdb MacsBug 

   Below are a list of some general notes about the Gdb MacsBug.  Much of this
   information is also available by type "mb-notes" (or help mb-notes) into
   gdb.

   * The MacsBug support for gdb is as faithful a simulation as possible within
     the limits of the environment.  There are also some extensions where it
     makes sense in the context of the debugging paradigm imposed by gdb.  Also
     a MacsBug-like UI is supported (use the MB command to turn on the MacsBug
     screen).

   * The $dot gdb variable is the last address referenced by certain commands.
     This corresponds to Mac Classic's MacsBug "." variable.  For example, SM
     sets $dot to the first address that was changed.  The default for many
     commands is to use $dot as their address argument.  Typing DM will display
     the memory just set and set the last command to DM.  A return after the
     parameterless DM command will use the $dot set by it to display then next
     block of memory in sequence.  Note that this is the normal MacsBug behavior
     but that is different from gdb which would repeat the last command.

   * The $colon gdb variable is the address of the start of the proc containing
     the current PC when the target is running.

   * Only C/C++ can be debugged since the gdb commands use C/C++ syntax for
     their implementation (restriction imposed by this implementation's use of
     gdb).

   * Arguments are written using C/C++ syntax (e.g., use -> instead of ^, !=
     instead of <>, etc.).

   * Only one command per line allowed (gdb restriction).

   * Some restrictions on the commands that are supported.  Do help on
     individual commands for details.

   * Decimal values are shown without leading '#' and hex is shown with a '0x'
     prefix (except in disassemblies and other displays where it's obvious it's
     hex).

   * The input radix is as defined by the gdb "SET input-radix" command.  The
     default is decimal unlike MacsBug which defaults to hex.  Use the gdb
     command "SHOW input-radix" to verify the input radix.

   * Many of the commands are not permitted until there is a target program
     running (using the MacsBug G or gdb RUN command).  Most of the commands
     will cause some kind of error report until there is a target running.  Of
     course with Mac Classic MacsBug there is always something running.  But
     that's not the case with gdb.

   * Some of the MacsBug SET options are supported.  Use HELP support to see a
     summary of SET commands.  All the MacsBug SET options start with "mb-"
     so you can use standard command completion (hitting tab after typing the
     "mb" will show you all those SET options.  For compatibility with Mac
     Classic MacsBug, SET ditto and SET unmangle are also supported.

   * Some Mac Classic MacsBug commands like SC6, SC7, and SC or ILP and IL
     are identical in the gdb implementation.  These alternatives take the
     form of gdb aliases.  Use HELP aliases to see the supported aliases.

   * SI is defined as the MacsBug S command since gdb already has a S command.
     But this overrides gdb's SI abbreviation for STEPI.

   * When the MacsBug screen is being used command lines may be continued (using
     '\') within the area defined for commands.  The default number of lines is
     2 but may be increased up to 15 using the SET mb-cmd-area command. 
     Continuing more than the allotted space is allowed but will cause the
     command lines to scroll up within their area.  Unfortunately this does not
     happen if a command is interactively created with DEFINE and you try to
     continue one of the command's lines.  The continuations are allowed.  But
     they just don't scroll.

   * Unlike Mac Classic MacsBug, there is only one screen for gdb and the target
     program.  Further, gdb has no control over output generated by the target
     program or caused by it (e.g., dyld messages).  Thus the MacsBug screen
     could get "mangled". Use the REFRESH command to restore the MacsBug screen
     or turn off the MacsBug screen and debug with gdb's normal scroll mode.
     All the MacsBug commands still work in scroll mode and any commands that
     cause disassembly will display the current registers on the right side of
     the terminal screen (to minimize the affect on scroll back viewing).

   * A set of possibly useful commands for extending this script are available.
     Typing "help useful" will list those commands.  You will see some output
     commands that write their output to stderr instead of stdout. This is only
     significant in the context of the MacsBug screen.  When the MacsBug screen
     is on (MB on) all output goes into the history area.  Output to stderr is
     written in red.  By convention this is be used for error reporting only.


6. Terminal Considerations

   Gdb MacsBug assumes it is writing to a terminal window that should be
   configured as follows:

   * Xterm enabled

   * VT100 or VT220 emulation

   * 8-bit connections

   * Window size of at least 44 rows by 80 columns if the MacsBug screen is
     turned on

   * Allows bolding of characters

   * Supports colors red and blue

   * Should use a mono-spaced font

   * No translation

   * Recommended font is Monaco 9 or at least a small enough font to fit within
     the MacsBug screen 44-line limitation

   With the exception of bolding, the OSX standard terminal window supports all
   these characteristics (use its preferences to configure for extra bolding).
   The only colors MacsBug uses are red and blue.  All other output uses the
   default font color.  

   Color and bolding are, of course, not a strict requirement. But Xterm, 8-bit
   support is.  While an OSX standard terminal has no problems with these
   requirements you do need to verify them when using a Telenet program.

   Finally, different terminal programs as well as the OSX terminal window
   itself have their own unique "quirks" and timing considerations or
   communication failures may mangle the display.  If this should happen while
   the MacsBug screen is up you will need to type a REFRESH command to restore
   the proper display.


7. Changes in MacsBug 1.1

   * Removed all references to the install-MacsBug script which was documented
     but never installed in 1.0.
   
   * Changed plugin name from "MacsBug to "MacsBug_plugin" which seems more
     appropriate.  If you have your own copy of "gdbinit-MacsBug" you will have
     to change the LOAD-PLUGIN command (or continue naming the plugin with its
     original name).

   * Removed the MSR and MQ registers from the sidebar.  Thus the minimum screen
     size is 44 lines.
   
   * Added bullet to Terminal Considerations above noting that you need a small
     enough font (recommended Monaco 9) to fit within the 44-line MacsBug screen
     limitation (the default OSX Monaco 10 did not allow the screen on iBooks).

   * Fixed installation bug where the LOAD-PLUGIN pathname reflected the build
     directory as opposed to simply referencing /usr/libexec/gdb/plugins/MacsBug.

   * Fixed LOG command to handle '~'s in the pathname.

   * Fixed plugin code to more gracefully coexist with Project Builder. There were
     redirection interactions between Project Builder and the plugin.

   * Added tests in key points to prevent MacsBug from doing xterm operations when
     not talking to a terminal as in the case with Project Builder.
     
   * Fixed a bug in the TD command which had a format error displaying the XER SOC
     bits.
     
   * Added fixes so that the MB-NOTES help defined by DOCUMENT in gdbinit-MacsBug
     displays correctly if mb-notes is used explicitly in Project Builder's gdb.
     The MB-NOTES as defined in gdbinit-MacsBug has leading option-spaces to keep
     gdb's DOCUMENT reading from skipping leading blanks and all blank lines. 
     These work fine on a terminal but do "funny things" to Project Builder's
     display of them.
  
   * Added recognition of all the abbreviations for the LIST command to allow these
     to produce contiguous listings when the MacsBug screen is off.  Previously it
     only worked for L and LIST.
   
   * Made NEXT, STEP, NEXTI, and STEPI output be contiguous when repeatedly
     specified with no arguments when the MacsBug screen is off.
   
   * Fixed bug in all contiguous listing commands (when MacsBug screen is off) 
     when run from gdb DEFINE commands.  Backup over gdb prompt should not be
     done under such conditions.
   
   * Enhanced the MacsBug WH command to display file/line information along with the
     source line if possible.  Syntax of argument made same as gdb's INFO LINE.

   * Fixed bug in register display to adjust the stack display to account for a
     changed window size.

   * Fixed scroll mode disassembly displays to wrap lines instead of being chopped
     off by register sidebar.

   * Added SET mb-sidebar mode setting command to enable/disable the display of the
     register sidebar when the ID, IL, IP, SO, or SI commands are done in gdb
     scroll mode.

   * Moved TF from gdbinit-MacsBug to the plugin to allow the display to be more
     MacsBug-like.

   * Implemented TV.
