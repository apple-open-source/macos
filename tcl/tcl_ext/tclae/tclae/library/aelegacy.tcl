## -*- mode: Tcl; coding: utf-8; -*-
namespace eval tclAE {}

proc tclAE::print {theAEDesc} {
	return $theAEDesc
}

proc tclAE::disposeDesc {theAEDesc} {
	# not sure what to do here, so do nothing
}

proc tclAE::encode {string} {
    binary scan [encoding convertto macRoman $string] H* hex
    return "\u00ab${hex}\u00bb"
}

proc tclAE::legacyQueueHandler {theAppleEvent theReply} {
    handleReply $theAppleEvent
}
