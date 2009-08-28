## -*- mode: Tcl; coding: utf-8; -*-
namespace eval error {}

proc error::throwOSErr {err {msg ""}} {
	global error::OSErr
	
	if {[info exists error::OSErr($err)]} {
		set err [set error::OSErr($err)]
		
		if {$msg == ""} {
			set msg [lindex $err 2]
		} else {
			set msg [join [list $msg [lindex $err 2]] ": "]
		}
		
		error $msg "" $err
	} elseif {$msg != ""} {
	    error "$msg: $err"
	} elseif {$err != 0} {
	    error "OS Error: $err"
	}
}

# Error messages from
# <http://devworld.apple.com/dev/techsupport/insidemac/
# AppleScriptLang/AppleScriptLang-271.html#HEADING271-0>

# Many, obviously, aren't relevant

set error::OSErr(-34)		{System -34 {Disk is full.}}
set error::OSErr(-35)		{System -35 {Disk wasn't found.}}
set error::OSErr(-37)		{System -37 {Bad name for file.}}
set error::OSErr(-38)		{System -38 {File wasn't open.}}
set error::OSErr(-39)		{System -39 {End of file error.}}
set error::OSErr(-42)		{System -42 {Too many files open.}}
set error::OSErr(-43)		{System -43 {File wasn't found.}}
set error::OSErr(-44)		{System -44 {Disk is write protected.}}
set error::OSErr(-45)		{System -45 {File is locked.}}
set error::OSErr(-46)		{System -46 {Disk is locked.}}
set error::OSErr(-47)		{System -47 {File is busy.}}
set error::OSErr(-48)		{System -48 {Duplicate file name.}}
set error::OSErr(-49)		{System -49 {File is already open.}}
set error::OSErr(-50)		{System -50 {Parameter error.}}
set error::OSErr(-51)		{System -51 {File reference number error.}}
set error::OSErr(-61)		{System -61 {File not open with write permission.}}
set error::OSErr(-108)		{System -108 {Out of memory.}}
set error::OSErr(-120)		{System -120 {Folder wasn't found.}}
set error::OSErr(-124)		{System -124 {Disk is disconnected.}}
set error::OSErr(-128)		{System -128 {User canceled.}}
set error::OSErr(-192)		{System -192 {A resource wasn't found.}}
set error::OSErr(-600)		{System -600 {Application isn't running.}}
set error::OSErr(-601)		{System -601 {Not enough room to launch application with special requirements.}}
set error::OSErr(-602)		{System -602 {Application is not 32-bit clean.}}         
set error::OSErr(-605)		{System -605 {More memory is needed than is specified in the size resource.}}
set error::OSErr(-606)		{System -606 {Application is background-only.}}
set error::OSErr(-607)		{System -607 {Buffer is too small.}}
set error::OSErr(-608)		{System -608 {No outstanding high-level event.}}
set error::OSErr(-609)		{System -609 {Connection is invalid.}}
set error::OSErr(-904)		{System -904 {Not enough system memory to connect to remote application.}}
set error::OSErr(-905)		{System -905 {Remote access is not allowed.}}
set error::OSErr(-906)		{System -906 {Program isn't running or program linking isn't enabled.}}
set error::OSErr(-915)		{System -915 {Can't find remote machine.}}
set error::OSErr(-30720)	{System -30720 {Invalid date and time.}}
	
# AppleEvent Errors
	
set error::OSErr(-1700)	{AppleEvent -1700 {Can't make some data into the expected type.}}
set error::OSErr(-1701)	{AppleEvent -1701 {Descriptor was not found.}}
set error::OSErr(-1702)	{AppleEvent -1702 {Some data could not be read.}}
set error::OSErr(-1703)	{AppleEvent -1703 {Some data was the wrong type.}}
set error::OSErr(-1704)	{AppleEvent -1704 {Some parameter was invalid.}}
set error::OSErr(-1705)	{AppleEvent -1705 {Operation involving a list item failed.}}
set error::OSErr(-1706)	{AppleEvent -1706 {Need a newer version of the AppleEvent manager.}}
set error::OSErr(-1707)	{AppleEvent -1707 {Event isn't an AppleEvent.}}
set error::OSErr(-1708)	{AppleEvent -1708 {<reference> doesn't understand the <commandName> message.}}
set error::OSErr(-1709)	{AppleEvent -1709 {AEResetTimer was passed an invalid reply.}}
set error::OSErr(-1710)	{AppleEvent -1710 {Invalid sending mode was passed.}}
set error::OSErr(-1711)	{AppleEvent -1711 {User canceled out of wait loop for reply or receipt.}}
set error::OSErr(-1712)	{AppleEvent -1712 {AppleEvent timed out.}}
set error::OSErr(-1713)	{AppleEvent -1713 {No user interaction allowed.}}
set error::OSErr(-1714)	{AppleEvent -1714 {Wrong keyword for a special function.}}
set error::OSErr(-1715)	{AppleEvent -1715 {Some parameter wasn't understood.}}
set error::OSErr(-1716)	{AppleEvent -1716 {Unknown AppleEvent address type.}}
set error::OSErr(-1717)	{AppleEvent -1717 {The handler is not defined.}}
set error::OSErr(-1718)	{AppleEvent -1718 {Reply has not yet arrived.}}
set error::OSErr(-1719)	{AppleEvent -1719 {Can't get <reference>. Invalid index.}}
set error::OSErr(-1720)	{AppleEvent -1720 {Invalid range.}}
set error::OSErr(-1721)	{AppleEvent -1721 {<expression> doesn't match the parameters <parameterNames> for <commandName>.}}
set error::OSErr(-1723)	{AppleEvent -1723 {Can't get <expression>. Access not allowed.}}
set error::OSErr(-1725)	{AppleEvent -1725 {Illegal logical operator called.}}
set error::OSErr(-1726)	{AppleEvent -1726 {Illegal comparison or logical.}}
set error::OSErr(-1727)	{AppleEvent -1727 {Expected a reference.}}
set error::OSErr(-1728)	{AppleEvent -1728 {Can't get <reference>.}}
set error::OSErr(-1729)	{AppleEvent -1729 {Object counting procedure returned a negative count.}}
set error::OSErr(-1730)	{AppleEvent -1730 {Container specified was an empty list.}}
set error::OSErr(-1731)	{AppleEvent -1731 {Unknown object type.}}
set error::OSErr(-1750)	{AppleEvent -1750 {Scripting component error.}}
set error::OSErr(-1751)	{AppleEvent -1751 {Invalid script id.}}
set error::OSErr(-1752)	{AppleEvent -1752 {Script doesn't seem to belong to AppleScript.}}
set error::OSErr(-1753)	{AppleEvent -1753 {Script error.}}
set error::OSErr(-1754)	{AppleEvent -1754 {Invalid selector given.}}
set error::OSErr(-1755)	{AppleEvent -1755 {Invalid access.}}
set error::OSErr(-1756)	{AppleEvent -1756 {Source not available.}}
set error::OSErr(-1757)	{AppleEvent -1757 {No such dialect.}}
set error::OSErr(-1758)	{AppleEvent -1758 {Data couldn't be read because its format is obsolete.}}
set error::OSErr(-1759)	{AppleEvent -1759 {Data couldn't be read because its format is too new.}}
set error::OSErr(-1760)	{AppleEvent -1760 {Recording is already on.}}

# AppleEvent Registry Errors

set error::OSErr(-10000)	{AERegistry -10000 {AppleEvent handler failed.}}
set error::OSErr(-10001)	{AERegistry -10001 {A descriptor type mismatch occurred.}}
set error::OSErr(-10002)	{AERegistry -10002 {Invalid key form.}}
set error::OSErr(-10003)	{AERegistry -10003 {Can't set <object or data> to <object or data>. Access not allowed.}}
set error::OSErr(-10004)	{AERegistry -10004 {A privilege violation occurred.}}
set error::OSErr(-10005)	{AERegistry -10005 {The read operation wasn't allowed.}}
set error::OSErr(-10006)	{AERegistry -10006 {Can't set <object or data> to <object or data>.}}
set error::OSErr(-10007)	{AERegistry -10007 {The index of the event is too large to be valid.}}
set error::OSErr(-10008)	{AERegistry -10008 {The specified object is a property, not an element.}}
set error::OSErr(-10009)	{AERegistry -10009 {Can't supply the requested descriptor type for the data.}}
set error::OSErr(-10010)	{AERegistry -10010 {The AppleEvent handler can't handle objects of this class.}}
set error::OSErr(-10011)	{AERegistry -10011 {Couldn't handle this command because it wasn't part of the current transaction.}}
set error::OSErr(-10012)	{AERegistry -10012 {The transaction to which this command belonged isn't a valid transaction.}}
set error::OSErr(-10013)	{AERegistry -10013 {There is no user selection.}}
set error::OSErr(-10014)	{AERegistry -10014 {Handler only handles single objects.}}
set error::OSErr(-10015)	{AERegistry -10015 {Can't undo the previous AppleEvent or user action.}}
