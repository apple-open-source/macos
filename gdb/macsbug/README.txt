                           MacsBug Interface for GDB
                                    2/7/01

1. Introduction

   The "MacsBug" supported here is gdb extended to support a subset of the Mac
   Classic MacsBug commands and a MacsBug-like screen UI. Thus it basically is
   a variant of MacsBug with source-level debugging!

   Along with this README there are four other files in this directory:

   * gdbinit-MacsBug-without-plugin
   * gdbinit-MacsBug
   * MacsBug
   * install-MacsBug

   Both gdbinit-MacsBug-without-plugin and gdbinit-MacsBug are gdb command 
   language scripts one of which you SOURCE from your ~/.gdbinit script (the
   script that gdb always looks for, and for what it's worth, it looks for a
   .gdbinit in the current directory as well).  We'll explain in the following
   sections the difference in the two MacsBug scripts.
   
   The MacsBug_plugins are just that; the gdb plugins that implement the MacsBug
   UI and commands.  The plugin is loaded by the gdbinit-MacsBug script.
   
   The install-MacsBug script is used for private installs of the MacsBug support
   files and/or to edit your .gdbinit script to add the proper SOURCE command
   to load MacsBug.


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
   is provided in a plugin (MacsBug) which "gdbinit-MacsBug" loads with a
   LOAD-PLUGIN Apple gdb command.

   The benefit of using the plugins is that it is very efficient because it
   "talks" directly to gdb.  It can provide functionality that the pure script
   variant cannot like some additional MacsBug commands and behavior as well
   as w MacsBug-like screen UI.  It's down side is of course that it is tied
   directly to the version of gdb (and system) for which it was built. Further,
   because the script uses the LOAD-PLUGIN gdb command it will only work with
   Apple's version of gdb. Since MacsBug is mainly used by Mac developers this
   should be not be a serious limitation.

   You thus have two choices:

   (1) Install the totally portable gdbinit-MacsBug-without-plugin script, or,
   (2) Install the gdbinit-MacsBug script and the MacsBug plugin file.


3. Installation

   The MacsBug files are all installed in,

      /usr/libexec/gdb/plugins/MacsBug

   If you elect choice (1) then just add the following to your ~/.gdbinit 
   script:

      source /usr/libexec/gdb/plugins/MacsBug/gdbinit-MacsBug-without-plugin

   If you want to use the plugins then add the following source command instead:

      source /usr/libexec/gdb/plugins/MacsBug/gdbinit-MacsBug

   The gdbinit-MacsBug script is installed with it's LOAD-PLUGIN command defined
   as follows:

     load-plugin /usr/libexec/gdb/plugins/MacsBug/MacsBug

   Note that load-plugin commands require a full pathname and no '~'s.

   From this point on this README assumes you are installing the plugin.


4. The "install-MacsBug" Script

   install-Macsbug [where] [--gdbinit=pathname] [--v[erbose]] [--f[orce]]
                           [--help]

   In /usr/libexec/gdb/plugins/MacsBug the two key files needed for the MacsBug
   plugins are:

   * gdbinit-MacsBug
   * MacsBug

   The gdbinit-MacsBug file is a gdb script which, among other things, does
   the load-plugin gdb command to load the MacsBug plugin file.  The
   load-plugin command requires a full pathname to the MacsBug plugin.  So
   it is preset to load from the system MacsBug directory.  The ~/.gdbinit
   script then needs to do a SOURCE command to load the gdbinit-MacsBug script,
   again from the system directory.

   As stated in the installation above you can explicitly edit your ~/.gdbinit
   script to do the appropriate source command.  Alternatively install-Macsbug
   can do it for you.

   It is also possible that there may be intermediate releases of the MacsBug
   plugins which are compatible with your system. Intermediate releases are not
   expected to be placed in /usr/libexec/gdb/plugins/MacsBug.  So the installer
   can also be used to install a newer MacsBug directory in a directory of your
   choosing.

   Thus install-Macsbug does three things:

   1. Optionally edits your ~/.gdbinit script (if --gdbinit is specified) to
      create or modify a SOURCE command that loads gdbinit-MacsBug from a copy
      of the MacsBug directory or the system-installed MacsBug directory.

   2. Optionally copies an entire MacsBug directory to a chosen directory
      ("where").  The source MacsBug directory is the directory containing the
      install-MacsBug installer script.

   3. Modifies the copied gdbinit-MacsBug load-plugin command to load the
      MacsBug plugin from the copied folder.

   The parameters to install-MacsBug have the following meaning:

   where                A pathname specifying a directory which is created
                        containing the contents of the MacsBug directory.  It
                        doesn't have to be called "MacsBug" and it need not
                        be a "visible" directory, i.e., you could name it with
                        a leading '.'.

                        The default is to not copy the MacsBug directory and set
                        the --gdbinit specified script to use the MacsBug script
                        defined in the folder containing the install-MacsBug
                        installer script.

   --gdbinit=pathname   Optionally specifies the location of a .gdbinit file.
                        For use in all gdb invocations this should be ~/.gdbinit.
                        If this parameter is not specified your ~/.gdbinit script
                        will not be modified.

                        Note, if the ~/.gdbinit script is modified, the original
                        version of the script is renamed with "-bkup" added to
                        the filename.

   --force              This script checks that the "where" is not in the same
                        place from which the install_macsBug script is running.
                        If it is, that implies the source and destination
                        directories are the same.  In addition a check is done
                        to see if the "where" has already been installed for
                        MacsBug.  In either of these cases no further action
                        is taken the and script terminates.  Specifying --force
                        overrides these tests and "forces" the installation.

   --verbose            Script confirms what it is doing.


5. Using Gdb's HELP Commands For MacsBug

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


6. General Comments About Gdb MacsBug 

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


7. Terminal Considerations

   Gdb MacsBug assumes it is writing to a terminal window that should be
   configured as follows:

   * Xterm enabled

   * VT100 or VT220 emulation

   * 8-bit connections

   * Window size of at least 46 rows by 80 columns if the MacsBug screen is
     turned on

   * Allows bolding of characters

   * Supports colors red and blue

   * Should use a mono-spaced font

   * No translation

   * Screen size of at least 46 rows and 80 columns if the MacsBug screen is
     used.

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
