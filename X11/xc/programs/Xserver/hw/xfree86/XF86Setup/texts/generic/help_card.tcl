# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/texts/generic/help_card.tcl,v 1.2 1998/04/05 15:30:28 robin Exp $
#
global cardDetail

if { $cardDetail == "std" } {
    set message "\n\n\
	Pick your card from the list.\n\n\
	If there are README files that may pertain to your card\n\
	the 'Read README file' button will then be usable (i.e. not\
	greyed out).\n\
	Please read them.\n\n\
	If your card is not in the list, or if there are any\
	special settings\n\
	listed in the README file as required by your card, you\
	can press the\n\
	'Detailed Setup' button to make sure that\
	they have been selected."
} else {
    global DeviceIDs
    if { [llength $DeviceIDs] > 1 } {
	set message \
	    " If you picked a card from the Card list, at least most\
	    things should\n\
	    already be set properly.\n\n"
    } else {
	set message "\n\n"
    }
    append message \
	" First select the appropriate server for your card.\n\
	Then read the README file corresponding to the selected\
	server by pressing\n\
	the 'Read README file' button (it won't do anything, if\
	there is no README).\n\n\
	Next, pick the chipset, and Ramdac of your card, if\
	directed by the README\n\
	file.  In most cases, you don't need to select these,\
	as the server will\n\
	detect (probe) them automatically.\n\n\
	The clockchip should generally be picked, if your card\
	has one, as these\n\
	are often impossible to probe (the exception is when\
	the clockchip is built\n\
	into one of the other chips).\n\n\
	Choose whatever options are appropriate (again,\
	according to the README).\n\n\
	You can also set the maximum speed of your Ramdac. Some\
	Ramdacs are available\n\
	with various speed ratings.  The max speed cannot be\
	detected by the server\n\
	so it will use the speed rating of the slowest version\
	of the specified Ramdac.\n\n\
	Additionally, you can also specify the amount of RAM on\
	your card, though\n\
	the server will usually be able to detect this."
}
