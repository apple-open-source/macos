# ParseQTAtoms.tcl --
# 
#       Some random code for parsing (QT) atoms.

namespace eval ::quicktimetcl { }

proc ::quicktimetcl::ParseQTAtomFile {fileName} {

    set fd [open $fileName RDONLY]
    fconfigure $fd -translation binary
    set version [GetAtomVersion $fd]
    if {$version == 0} {
	ParseContainerAtom $fd
    } else {
	set offset 12
	foreach {size type id count} [ParseQTAtomHeader $fd $offset] {break}
	set offset 32
	for {set i 0} {$i < $count} {incr i} {
	    foreach {size type} [ParseAtom $fd $offset] {break}
	    incr offset $size
	}
    }
    close $fd
}

proc ::quicktimetcl::GetAtomVersion {fd} {
    
    # Classic atoms have 4 byte size + 4 byte type.
    # QT atom container header starts with a 10 byte null element.
    set data [read $fd 8]
    binary scan $data II size type
    if {($size == 0) && ($type == 0)} {
	return 1
    } else {
	return 0
    }
}

proc ::quicktimetcl::ParseContainerAtom {fd {offset 0}} {
        
    # Classic atoms have 4 byte size + 4 byte type.
    seek $fd $offset
    set data [read $fd 8]
    binary scan $data Ia4 size type
    puts "Classic atom type: type='$type', size=$size"
    return [list $size $type]
}

proc ::quicktimetcl::ParseQTAtomHeader {fd {offset 0}} {
    
    seek $fd $offset
    set data [read $fd 20]    
    binary scan $data Ia4ISSI size type id zero count zero
    puts "QT atom header: type='$type', size=$size, id=$id, count=$count"
    return [list $size $type $id $count]
}

proc ::quicktimetcl::ParseAtom {fd {offset 0}} {
	
    # Classic atoms have 4 byte size + 4 byte type.
    seek $fd $offset
    set data [read $fd 8]
    binary scan $data Ia4 size type
    puts "Atom: type='$type', size=$size"
    return [list $size $type]
}


