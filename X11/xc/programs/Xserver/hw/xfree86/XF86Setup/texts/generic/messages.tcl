# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/texts/generic/messages.tcl,v 1.5 1999/04/05 07:13:05 dawes Exp $
#
# messages in done.tcl :
set messages(done.1)	"\n\n\
		If you've finished configuring everything press the\n\
		Okay button to start the X server using the\
		configuration you've selected.\n\n\
		If you still wish to configure some things,\n\
		press one of the buttons at the top and then\n\
		press \"Done\" again, when you've finished."
set messages(done.2) "Okay."
set messages(done.3) "Just a moment..."

# messages in srvflags :
set messages(srvflags.1) "Optional server settings\n\n\
		These should be set to reasonable values, by default,\n\
		so you probably don't need to change anything"
set messages(srvflags.2) "Allow server to be killed with\
		hotkey sequence (Ctrl-Alt-Backspace)"
set messages(srvflags.3) "Allow video mode switching"
set messages(srvflags.4) "Don't Trap Signals\
			- prevents the server from exitting cleanly"
set messages(srvflags.5) "Allow video mode changes from other hosts"
set messages(srvflags.6) "Allow changes to keyboard and mouse settings\
		from other hosts"

# messages in phase1 :
set messages(phase1.1) "Not all of the"
set messages(phase1.2) "are installed. The file"
set messages(phase1.3) "is missing"
set messages(phase1.4) \
	"Warning! Not all of the READMEs are\
	installed. You may not be able to view some of\
	the instructions regarding setting up your card,\
	but otherwise, everything should work correctly"
set messages(phase1.5) "Error encountered reading existing\
	configuration file"
set messages(phase1.6) \
	"It appears that you are currently \
	running under X11. If this is correct \
	and you are interested in making some \
	adjustments to your current setup, \
	answer yes to the following question."
set messages(phase1.7) \
	"If this is incorrect or you \
	would like to go through the full \
	configuration process, then answer no."
set messages(phase1.8) "Is this a reconfiguration?"
set messages(phase1.9) \
	"You are not running as root.\n\n\
	Superuser privileges are usually required to save any changes\n\
	you make in a directory that is searched by the server and\n\
	are required to change the mouse device.\n\n\
	Would you like to continue anyway?"
set messages(phase1.10) \
	"You need to be root to set the initial\
	configuration with this program"
set messages(phase1.11) \
	"Can't find the XFree86 loader - falling back\n\
	 to text interface for configuration"
set messages(phase1.12) \
	"The XFree86 loader (XF98_LOADER) or the EGC\n\
	or the PEGC server is required when using\n\
	this program to set the initial configuration"
set messages(phase1.13) \
	"Would you like to use the\
	existing XF86Config file for defaults?"
set messages(phase1.14) "You need to be root to run this program"
set messages(phase1.15) \
	"Can't find the XFree86 loader - falling back\n\
	 to text interface for configuration"
set messages(phase1.16) \
	"Either the XFree86 loader (XF98_LOADER) or the\n\
	EGC server or the PEGC server is required to run\n\
	this program" 
set messages(phase1.17) \
	"Would you like to use the Xqueue driver\n\
	for mouse and keyboard input?"
set messages(phase1.18) \
	"Would you like to use the system event\
	queue for mouse input?" 
set messages(phase1.19) "Unable to make directory "
set messages(phase1.20) "\nfor storing temporary files"
set messages(phase1.23) "Ready to switch to graphics mode.\n\
	\nIt may take a while"
set messages(phase1.24) "Unable to start X server!\n\
	Falling back to text or curses interface...."
set messages(phase1.25) "Unable to communicate with X server!\n\
	Falling back to text or curses interface...."
set messages(phase1.26) "Please wait\n\nThis may take a while..."
set messages(phase1.27) "Unable to startup curses - maybe your terminal\n\
	type (%s) is unknown?  Falling back to plain text interface...."
set messages(phase1.28) "\$TERM no set.\n\
	Falling back to plain text interface...."

# messages in phase2 :
set messages(phase2.1) "Loading  -  Please wait...\n\n\n"
set messages(phase2.2) \
	    "Unable to read keyboard information from the server.\n\n\
	    This problem most often occurs when you are running when\n\
	    you are running a server which does not have the XKEYBOARD\n\
	    extension or which has it disabled.\n\n\
	    The ability of this program to configure the keyboard is\n\
	    reduced without the XKEYBOARD extension, but is still\
	    functional.\n\n\
	    Continuing..."
set messages(phase2.3) Mouse
set messages(phase2.4) Keyboard
set messages(phase2.5) Card
set messages(phase2.6) Monitor
set messages(phase2.7) Modeselection
set messages(phase2.8) Other
set messages(phase2.9) Abort
set messages(phase2.10) Done
set messages(phase2.11) Help
set messages(phase2.12) "\n\
		There are five areas of configuration that need to\
			be completed, corresponding to the buttons\n\
		along the top:\n\n\
		\tMouse\t\t- Use this to set the protocol, baud rate, etc.\
			used by your mouse\n\
		\tKeyboard\t- Set the nationality and layout of\
			your keyboard\n\
		\tCard\t\t- Used to select the chipset, RAMDAC, etc.\
			of your card\n\
		\tMonitor\t\t- Use this to enter your\
			monitor's capabilities\n\
		\tModeselction\t\t- Use this to chose the modes\
			that you want to use\n\
		\tOther\t\t- Configure some miscellaneous settings\n\n\
		You'll probably want to start with configuring your\
			mouse (you can just press \[Enter\] to do so)\n\
		and when you've finished configuring all five of these,\
			select the Done button.\n\n\
		To select any of the buttons, press the underlined\
			letter together with either Control or Alt.\n\
		You can also press ? or click on the Help button at\
			any time for additional instructions\n\n"
set messages(phase2.13) "Dismiss"
set messages(phase2.14) "The program is running on a different\
			virtual terminal\n\n\
			Please switch to the correct virtual terminal"

# messages in phase3 :
set messages(phase3.1) "Attempting to start server..."
set messages(phase3.2) "Unable to communicate with X server"
set messages(phase3.3) "Unable to start X server"
set messages(phase3.4) "\n\nPress \[Enter\] to try configuration again"
set messages(phase3.5) \
	"Ack! Unable to get the VGA16 server going again!"

# messages in phase4 :
set messages(phase4.1) "Loading  -  Please wait..."
# phase.2-5 is generated by proc 'make_message_phase4'.
set messages(phase4.6) "Okay"
set messages(phase4.7) \
	"You can now run xvidtune to adjust your display settings,\n\
	if you want to change the size or placement of the screen image\n\n\
	If not, go ahead and exit\n\n\n\
	If you choose to save the configuration, a backup copy will be\n\
	made, if the file already exists"
set messages(phase4.8) "Save configuration to:"
set messages(phase4.9) "Run xvidtune"
set messages(phase4.10) "Save the configuration and exit"
set messages(phase4.11) "Abort - Don't save the configuration"
set messages(phase4.12) "Aborted"
set messages(phase4.13) "Congratulations, you've got a running server!\n\n"
set messages(phase4.14) "Just a moment..."
set messages(phase4.15) ""
set messages(phase4.16) "The program is running on a different\
			virtual terminal\n\n\
			Please switch to the correct virtual terminal"

# messages in phase5.tcl :
set messages(phase5.1) "Do you want to create an 'X' link to the "
set messages(phase5.2) \
	" server?\n\n(the link will be created in the directory:"
set messages(phase5.3) ") Okay?"
set messages(phase5.4) "Link creation failed!\n\
	You'll have to do it yourself"
set messages(phase5.5) "Link created successfully."
set messages(phase5.6) "\n\nConfiguration complete."

# messages in setuplib.tcl :
set messages(setuplib.1) "\n\nPress \[Enter\] to continue..."
set messages(setuplib.2) "The temporary files directory ("
set messages(setuplib.3) ")\nis no longer secure!"

# messages in card.tcl :
set messages(card.1) "Card selected:"
set messages(card.2) "Card selected: None"
set messages(card.3) "Read README file"
set messages(card.4) "Detailed Setup"
set messages(card.5) Server:
set messages(card.7) Chipset
set messages(card.8) RamDac
set messages(card.9) ClockChip
set messages(card.10) "RAMDAC Max Speed"
set messages(card.11) "Probed"
set messages(card.12) "Video RAM"
set messages(card.13) "Probed"
set messages(card.14) "256K"
set messages(card.15) "512K"
set messages(card.16) "1Meg"
set messages(card.17) "2Meg"
set messages(card.18) "3Meg"
set messages(card.19) "4Meg"
set messages(card.20) "6Meg"
set messages(card.21) "8Meg"
set messages(card.22) "Selected options:"
set messages(card.23) "Additional lines to\
	add to Device section of the XF86Config file:"
#set messages(card.24) "Probed: Yes"
#set messages(card.25) "Probed: No"
set messages(card.26) "Card List"
set messages(card.27) "Detailed Setup"
set messages(card.28) "Card selected: "
set messages(card.29) "Dismiss"
set messages(card.30) \
	"First make sure the correct server is selected,\
	then make whatever changes are needed\n\
	If the Chipset, RamDac, or ClockChip entries\
	are left blank, they will be probed"
set messages(card.31) \
	"Select your card from the list.\n\
	If your card is not listed,\
	click on the Detailed Setup button"
set messages(card.32) \
	"That's all there is to configuring your card\n\
	unless you would like to make changes to the\
	standard settings (by pressing Detailed Setup)"
set messages(card.33) \
	"That's probably all there is to configuring\
	your card, but you should probably check the\n\
	README to make sure. If any changes are needed,\
	press the Detailed Setup button"
set messages(card.34) \
	"You have selected a card which is not fully\
	supported by XFree86, however all of the proper\n\
	configuration options have been set such that it\
	should work in standard VGA mode"

# messages in keyboard.tcl :
set messages(keyboard.1) "Model:"
set messages(keyboard.2) "Layout (language):"
set messages(keyboard.3) "Apply"
set messages(keyboard.4) \
		"Select the appropriate model and layout"
set messages(keyboard.5) "Available options:"
set messages(keyboard.6) \
		"Variant (non U.S. Keyboards only):"
set messages(keyboard.7) "Use default setting"
set messages(keyboard.8) "Failed! Try again"
set messages(keyboard.9) "Applying..."
set messages(keyboard.10) "Please wait..."

# messages in modeselect.tcl :

set messages(modeselect.1) "Select the modes you want to use"
set messages(modeselect.2) 640x480
set messages(modeselect.3) 800x600
set messages(modeselect.4) 1024x768
set messages(modeselect.5) 1152x864
set messages(modeselect.6) 1280x1024
set messages(modeselect.7) 1600x1200
set messages(modeselect.8) 640x400
set messages(modeselect.9) 320x200
set messages(modeselect.10) 320x240
set messages(modeselect.11) 400x300
set messages(modeselect.12) 480x300
set messages(modeselect.13) 512x384
set messages(modeselect.14) "Select the default color depth you want to use"
set messages(modeselect.15) " 8bpp "
set messages(modeselect.16) " 16bpp "
set messages(modeselect.17) " 24bpp "
set messages(modeselect.18) " 32bpp "

# messages in monitor.tcl :

set messages(monitor.1) "Monitor sync rates"
set messages(monitor.2) "Monitor selected:"
set messages(monitor.3) "Horizontal"
set messages(monitor.4) "Vertical"
set messages(monitor.5) \
	"Enter the Horizontal and Vertical Sync ranges for your monitor\n\
	or if you do not have that information, choose from the list"

# messages in mouse.tcl :

set messages(mouse.1) "Lines/inch"
set messages(mouse.2) "Sample Rate"
set messages(mouse.3) "Select the mouse protocol"
set messages(mouse.4) "Applying..."
set messages(mouse.5) "Mouse Device"
set messages(mouse.6) Emulate3Buttons
set messages(mouse.7) ChordMiddle
set messages(mouse.8) "Baud Rate"
set messages(mouse.9) "Press ? or Alt-H for a list of key bindings"
set messages(mouse.10) Flags
set messages(mouse.11) ClearDTR
set messages(mouse.12) ClearRTS
set messages(mouse.13) "Sample Rate"
set messages(mouse.14) "Emulate3Timeout"
set messages(mouse.15) "Apply"
set messages(mouse.16) "Press ? or Alt-H for a list of key bindings"
set messages(mouse.17) "Exit"
set messages(mouse.18) 1
set messages(mouse.19) "Resolution"
set messages(mouse.20) "High"
set messages(mouse.21) "Medium"
set messages(mouse.22) "Low"
set messages(mouse.23) "Buttons"
