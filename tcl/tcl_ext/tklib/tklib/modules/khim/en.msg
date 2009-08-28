# en.msg --
#
#	English-language messages for KHIM
#
# Copyright (c) 2005 by Kevin B. Kenny.  All rights reserved.
#
# Refer to the file "license.terms" for the terms and conditions of
# use and redistribution of this file, and a DISCLAIMER OF ALL WARRANTEES.
#
# $Id: en.msg,v 1.3 2007/06/08 19:24:31 kennykb Exp $
# $Source: /cvsroot/tcllib/tklib/modules/khim/en.msg,v $
#
#----------------------------------------------------------------------

namespace eval ::khim {

    # If you edit HELPTEXT or {SELECT COMPOSE KEY}, also edit the corresponding
    # messages in ROOT.msg

    ::msgcat::mcset en HELPTEXT {

	Kevin's Hacky Input Method (KHIM)

	KHIM allows you to input international characters from a
	keyboard that doesn't support them.  It works independently of
	any input method that the operating system may supply; it is
	intended for when you don't have control over your keyboard
	mapping and still need to input text in other languages.

	To use KHIM, bring up the KHIM Controls (the way this is done
        depends on your application) and enable KHIM by checking "Use
        KHIM".  You also need to choose a key on your keyboard that is
        seldom used, and designate it as the "Compose" key by pressing
        the button labelled, "Compose key:" then striking the key you
        wish to designate.  Generally speaking, this key should not be
        the key designated as "Compose" on the keyboard; that key will
        continue to invoke whatever input method the local operating
        system supplies.

	Once KHIM is enabled, you can enter international characters
	in any widget that is configured to use KHIM by pressing the
	Compose key followed by a two-character sequence.  The listbox
	in the KHIM controls shows the available sequences.  In
	addition, if you strike the Compose key twice, you get a
	dialog that allows you to input arbitrary symbols from a
	Unicode character map. In the map, you can navigate among the
	characters using either the cursor keys or the mouse, and you
	can select the current character for insertion by
	double-clicking it, pressing the space bar, or pressing the
	Enter (or Return) key.

	To define a new sequence for use with the Compose key, bring
	up the KHIM controls, enter the two characters in the
	"Input key sequence" entry and the desired character to insert
	into the "Character" entry, and press "Change".  (You may copy
	and paste the character from another application, or use the
	"Unicode..." button (or press the Compose key twice) to select
	the character from a map of all available Unicode code
	points.) To remove a sequence, select it in the listbox and
	press "Delete".

    }

    ::msgcat::mcset en {SELECT COMPOSE KEY} [string map [list \n\t \n] {
	Please press the 
	key that you want 
	to use as the 
	"Compose" key.
    }]

    ::msgcat::mcset en {Apply} {Apply}

    ::msgcat::mcset en {Cancel} {Cancel}

    ::msgcat::mcset en {Change} {Change}

    ::msgcat::mcset en {Character} {Character}

    ::msgcat::mcset en {Compose Key} {Compose Key}

    ::msgcat::mcset en {Compose key:} {Compose key:}

    ::msgcat::mcset en {Composed sequence must be two characters long} \
	{Composed sequence must be two characters long}

    ::msgcat::mcset en {Delete} {Delete}

    ::msgcat::mcset en {KHIM Help} {KHIM Help}

    ::msgcat::mcset en {Help...} {Help...}

    ::msgcat::mcset en {Input key sequence} {Input key sequence}

    ::msgcat::mcset en {Insert Character} {Insert Character}

    ::msgcat::mcset en {Invalid sequence} {Invalid sequence}

    ::msgcat::mcset en {Key sequences} {Key sequences}

    ::msgcat::mcset en {KHIM Controls} {KHIM Controls}

    ::msgcat::mcset en {OK} {OK}

    ::msgcat::mcset en {Select code page:} {Select code page:}

    ::msgcat::mcset en {Unicode...} {Unicode...}

    ::msgcat::mcset en {Use KHIM} {Use KHIM}

}

# Local Variables:
# mode: tcl
# End:
