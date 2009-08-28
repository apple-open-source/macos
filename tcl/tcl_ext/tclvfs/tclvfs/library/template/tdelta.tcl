# tdelta.tcl --
#
#	Produce an rdiff-style delta signature of one file with respect to another,
#	and re-create one file by applying the delta to the other.
#
# Written by Stephen Huntley (stephen.huntley@alum.mit.edu)
# License: Tcl license
# Version 1.0
#
# Usage:
#
# tdelta <reference file | channel> <target file | channel> [sizecheck [fingerprint]]
#	Returns a delta of the target file with respect to the reference file.
#	i.e., using patch to apply the delta to the target file will re-create the reference file.
#
#	sizecheck and fingerprint are booleans which enable time-saving checks: 
#
#	if sizecheck is True then if the file size is
#	less than five times the block size, then no delta calculation is done and the
#	signature contains the full reference file contents.  
#
#	if fingerprint is True then 10 small strings ("fingerprints") are taken from the target
#	file and searched for in the reference file.  If at least three aren't found, then
#	no delta calculation is done and the signature contains the full reference file contents.
#
# tpatch <target file | channel> <delta signature> <output file (duplicate of reference file) | channel>
#	Reconstitute original reference file by applying delta to target file.
#
#
# global variables:
#
# blockSize
#	Size of file segments to compare.
# 	Smaller blockSize tends to create smaller delta.
# 	Larger blockSize tends to take more time to compute delta.
# md5Size
#	Substring of md5 checksum to store in delta signature.
#	If security is less of a concern, set md5Size to a number
#	between 1-32 to create a more compact signature.

package provide trsync 1.0

namespace eval ::trsync {

if ![info exists blockSize] {variable blockSize 100}
if ![info exists Mod] {variable Mod [expr pow(2,16)]}
if ![info exists md5Size] {variable md5Size 32}

variable temp
if ![info exists temp] {
	catch {set temp $::env(TMP)}
	catch {set temp $::env(TEMP)}
	catch {set temp $::env(TRSYNC_TEMP)}
	if [catch {file mkdir $temp}] {set temp [pwd]}
}
if ![file writable $temp] {error "temp location not writable"}

proc Backup {args} {
	return
}

proc ConstructFile {copyinstructions {eolNative 0} {backup {}}} {
	if [catch {package present md5 2}] {package forget md5 ; package require md5 2}

	set fileToConstruct [lindex $copyinstructions 0]
	set existingFile [lindex $copyinstructions 1]
	set blockSize [lindex $copyinstructions 2]
	array set fileStats [lindex $copyinstructions 3]
	array set digestInstructionArray [DigestInstructionsExpand [lindex $copyinstructions 4] $blockSize]
	array set dataInstructionArray [lindex $copyinstructions 5]
	unset copyinstructions

	if {[lsearch [file channels] $existingFile] == -1} {
		set existingFile [FileNameNormalize $existingFile]
		if {$fileToConstruct == {}} {file delete -force $existingFile ; return}
		catch {
			set existingID [open $existingFile r]
			fconfigure $existingID -translation binary
		}
	} else {
		set existingID $existingFile
		fconfigure $existingID -translation binary
	}

	set temp $::trsync::temp

	if {[lsearch [file channels] $fileToConstruct] == -1} {
		set fileToConstruct [FileNameNormalize $fileToConstruct]
		set constructTag "trsync.[md5::md5 -hex "[clock seconds] [clock clicks]"]"
		set constructID [open $temp/$constructTag w]
	} else {
		set constructID $fileToConstruct
	}
	fconfigure $constructID -translation binary

	if $eolNative {set eolNative [string is ascii -strict [array get dataInstructionArray]]}

	set filePointer 1
	while {$filePointer <= $fileStats(size)} {
		if {[array names dataInstructionArray $filePointer] != {}} {
			puts -nonewline $constructID $dataInstructionArray($filePointer)
			set segmentLength [string length $dataInstructionArray($filePointer)]
			array unset dataInstructionArray $filePointer
			set filePointer [expr $filePointer + $segmentLength]
		} elseif {[array names digestInstructionArray $filePointer] != {}} {
			if ![info exists existingID] {error "Corrupt copy instructions."}
			set blockNumber [lindex $digestInstructionArray($filePointer) 0]
			set blockMd5Sum [lindex $digestInstructionArray($filePointer) 1]

			seek $existingID [expr $blockNumber * $blockSize]

			set existingBlock [read $existingID $blockSize]
			set existingBlockMd5Sum [string range [md5::md5 -hex -- $existingBlock] 0 [expr [string length $blockMd5Sum] - 1]]
			if ![string equal -nocase $blockMd5Sum $existingBlockMd5Sum] {error "digest file contents mismatch"}
			puts -nonewline $constructID $existingBlock

			if $eolNative {set eolNative [string is ascii -strict $existingBlock]}
			unset existingBlock
			set filePointer [expr $filePointer + $blockSize]
		} else {
			error "Corrupt copy instructions."
		}
	}
	catch {close $existingID}
	set fileStats(eolNative) $eolNative
	if {[lsearch [file channels] $fileToConstruct] > -1} {return [array get fileStats]}

	close $constructID

	if $eolNative {
		fcopy [set fin [open $temp/$constructTag r]] [set fout [open $temp/${constructTag}fcopy w]]
		close $fin
		close $fout
		file delete -force $temp/$constructTag
		set constructTag "${constructTag}fcopy"
	}

	catch {file attributes $temp/$constructTag -readonly 0} result
	catch {file attributes $temp/$constructTag -permissions rw-rw-rw-} result
	catch {file attributes $temp/$constructTag -owner $fileStats(uid)} result
	catch {file attributes $temp/$constructTag -group $fileStats(gid)} result
	catch {file mtime $temp/$constructTag $fileStats(mtime)} result
	catch {file atime $temp/$constructTag $fileStats(atime)} result
	if [string equal $fileToConstruct $existingFile] {
		catch {file attributes $existingFile -readonly 0} result
		catch {file attributes $existingFile -permissions rw-rw-rw-} result
	}

	Backup $backup $fileToConstruct

	file mkdir [file dirname $fileToConstruct]
	file rename -force $temp/$constructTag $fileToConstruct
	array set attributes $fileStats(attributes)
	array set attrConstruct [file attributes $fileToConstruct]
	foreach attr [array names attributes] {
		if [string equal [array get attributes $attr] [array get attrConstruct $attr]] {continue}
		if {[string equal $attr "-longname"] || [string equal $attr "-shortname"] || [string equal $attr "-permissions"]} {continue}
		catch {file attributes $fileToConstruct $attr $attributes($attr)} result
	}
	catch {file attributes $fileToConstruct -permissions $fileStats(mode)} result
	return
}

proc CopyInstructions {filename digest} {
	if [catch {package present md5 2}] {package forget md5 ; package require md5 2}

	if {[lsearch [file channels] $filename] == -1} {
		set filename [FileNameNormalize $filename]
		file stat $filename fileStats
		array set fileAttributes [file attributes $filename]
		array unset fileAttributes -longname
		array unset fileAttributes -shortname
		set arrayadd attributes ; lappend arrayadd [array get fileAttributes] ; array set fileStats $arrayadd
		set f [open $filename r]
	} else {
		set f $filename
		set fileStats(attributes) {}
	}
	fconfigure $f -translation binary
	seek $f 0 end
	set fileSize [tell $f]
	seek $f 0
	set fileStats(size) $fileSize
	set digestFileName [lindex $digest 0]
	set blockSize [lindex $digest 1]
	set digest [lrange $digest 2 end]

	if {[lsearch -exact $digest fingerprints] > -1} {
		set fingerPrints [lindex $digest end]
		set digest [lrange $digest 0 end-2]
		set fileContents [read $f]
		set matchCount 0
		foreach fP $fingerPrints {
			if {[string first $fP $fileContents] > -1} {incr matchCount}
			if {$matchCount > 3} {break}
		}
		unset fileContents
		seek $f 0
		if {$matchCount < 3} {set digest {}}
	}

	set digestLength [llength $digest]
	for {set i 0} {$i < $digestLength} {incr i} {
		set arrayadd [lindex [lindex $digest $i] 1]
		lappend arrayadd $i
		array set Checksums $arrayadd
	}
	set digestInstructions {}
	set dataInstructions {}
	set weakChecksum {}
	set startBlockPointer 0
	set endBlockPointer 0

	if ![array exists Checksums] {
		set dataInstructions 1
		lappend dataInstructions [read $f]
		set endBlockPointer $fileSize
	}

	while {$endBlockPointer < $fileSize} {
		set endBlockPointer [expr $startBlockPointer + $blockSize]
		incr startBlockPointer
		if {$weakChecksum == {}} {
			set blockContents [read $f $blockSize]
			set blockNumberSequence [SequenceBlock $blockContents]
			set weakChecksumInfo [WeakChecksum $blockNumberSequence]
			set weakChecksum [format %.0f [lindex $weakChecksumInfo 0]]
			set startDataPointer $startBlockPointer
			set endDataPointer $startDataPointer
			set dataBuffer {}
		}
		if {[array names Checksums $weakChecksum] != {}} {
			set md5Sum [md5::md5 -hex -- $blockContents]
			set blockIndex $Checksums($weakChecksum)
			set digestmd5Sum [lindex [lindex $digest $blockIndex] 0]
			if [string equal -nocase $digestmd5Sum $md5Sum] {
				if {$endDataPointer > $startDataPointer} {
					lappend dataInstructions $startDataPointer
					lappend dataInstructions $dataBuffer
				}
				lappend digestInstructions $startBlockPointer
				lappend digestInstructions "$blockIndex [string range $md5Sum 0 [expr $::trsync::md5Size - 1]]"
				set weakChecksum {}
				set startBlockPointer $endBlockPointer
				continue
			}
		}
		if {$endBlockPointer >= $fileSize} {
			lappend dataInstructions $startDataPointer
			lappend dataInstructions $dataBuffer$blockContents
			break
		}
		set rollChar [read $f 1]
		binary scan $rollChar c* rollNumber
		set rollNumber [expr ($rollNumber + 0x100)%0x100]
		lappend blockNumberSequence $rollNumber
		set blockNumberSequence [lrange $blockNumberSequence 1 end]

		binary scan $blockContents a1a* rollOffChar blockContents
		set blockContents $blockContents$rollChar
		set dataBuffer $dataBuffer$rollOffChar
		incr endDataPointer

		set weakChecksumInfo "[eval RollChecksum [lrange $weakChecksumInfo 1 5] $rollNumber] [lindex $blockNumberSequence 0]"
		set weakChecksum [format %.0f [lindex $weakChecksumInfo 0]]
	}
	close $f

	lappend copyInstructions $filename
	lappend copyInstructions $digestFileName
	lappend copyInstructions $blockSize
	lappend copyInstructions [array get fileStats]
	lappend copyInstructions [DigestInstructionsCompress $digestInstructions $blockSize]
	lappend copyInstructions $dataInstructions
	return $copyInstructions
}

proc Digest {filename blockSize {sizecheck 0} {fingerprint 0}} {
	if [catch {package present md5 2}] {package forget md5 ; package require md5 2}

	set digest "[list $filename] $blockSize"
	if {[lsearch [file channels] $filename] == -1} {
		set filename [FileNameNormalize $filename]
		set digest "[list $filename] $blockSize"
		if {!([file isfile $filename] && [file readable $filename])} {return $digest}
		set f [open $filename r]
	} else {
		set f $filename
	}
	fconfigure $f -translation binary
	seek $f 0 end
	set fileSize [tell $f]
	seek $f 0
	if {$sizecheck && ($fileSize < [expr $blockSize * 5])} {close $f ; return $digest}

	while {![eof $f]} {
		set blockContents [read $f $blockSize]
		set md5Sum [md5::md5 -hex -- $blockContents]
		set blockNumberSequence [SequenceBlock $blockContents]
		set weakChecksum [lindex [WeakChecksum $blockNumberSequence] 0]
		lappend digest "$md5Sum [format %.0f $weakChecksum]"
	}
	if $fingerprint {
		set fileIncrement [expr $fileSize/10]
		set fpLocation [expr $fileSize - 21]
		set i 0
		while {$i < 10} {
			if {$fpLocation < 0} {set fpLocation 0}
			seek $f $fpLocation
			lappend fingerPrints [read $f 20]
			set fpLocation [expr $fpLocation - $fileIncrement]
			incr i
		}
		lappend digest fingerprints
		lappend digest [lsort -unique $fingerPrints]
	}
	close $f
	return $digest
}

proc DigestInstructionsCompress {digestInstructions blockSize} {
	if [string equal $digestInstructions {}] {return {}}
	set blockSpan $blockSize
	foreach {pointer blockInfo} $digestInstructions {
		if ![info exists currentBlockInfo] {
			set currentPointer $pointer
			set currentBlockInfo $blockInfo
			set md5Size [string length [lindex $blockInfo 1]]
			continue
		}
		if {$pointer == [expr $currentPointer + $blockSpan]} {
			set md5 [lindex $blockInfo 1]
			lappend currentBlockInfo $md5
			incr blockSpan $blockSize
		} else {
			lappend newDigestInstructions $currentPointer
			lappend newDigestInstructions "[lindex $currentBlockInfo 0] [list "$md5Size [string map {{ } {}} [lrange $currentBlockInfo 1 end]]"]"

			set currentPointer $pointer
			set currentBlockInfo $blockInfo
			set blockSpan $blockSize
		}
	}
	lappend newDigestInstructions $currentPointer
	lappend newDigestInstructions "[lindex $currentBlockInfo 0] [list "$md5Size [string map {{ } {}} [lrange $currentBlockInfo 1 end]]"]"
	return $newDigestInstructions
}

proc DigestInstructionsExpand {digestInstructions blockSize} {
	if [string equal $digestInstructions {}] {return {}}
	foreach {pointer blockInfo} $digestInstructions {
		set blockNumber [lindex $blockInfo 0]
		set md5Size [lindex [lindex $blockInfo 1] 0]
		set blockString [lindex [lindex $blockInfo 1] 1]
		set blockLength [string length $blockString]

		set expandedBlock {}
		for {set i $md5Size} {$i <= $blockLength} {incr i $md5Size} {
			append expandedBlock " [string range $blockString [expr $i - $md5Size] [expr $i - 1]]"
		}

		set blockInfo "$blockNumber $expandedBlock"
		foreach md5 [lrange $blockInfo 1 end] {
			lappend newDigestInstructions $pointer
			lappend newDigestInstructions "$blockNumber $md5"
			incr pointer $blockSize
			incr blockNumber
		}
	}
	return $newDigestInstructions
}

proc FileNameNormalize {filename} {
	file normalize $filename
}

proc RollChecksum {a(k,l)_ b(k,l)_ k l Xsub_k Xsub_l+1 } {
	set Mod $trsync::Mod

	set a(k+1,l+1)_ [expr ${a(k,l)_} - $Xsub_k + ${Xsub_l+1}]
	set b(k+1,l+1)_ [expr ${b(k,l)_} - (($l - $k + 1) * $Xsub_k) + ${a(k+1,l+1)_}]

	set a(k+1,l+1)_ [expr fmod(${a(k+1,l+1)_},$Mod)]
	set b(k+1,l+1)_ [expr fmod(${b(k+1,l+1)_},$Mod)]
	set s(k+1,l+1)_ [expr ${a(k+1,l+1)_} + ($Mod * ${b(k+1,l+1)_})]
	return "${s(k+1,l+1)_} ${a(k+1,l+1)_} ${b(k+1,l+1)_} [incr k] [incr l]"
}

proc SequenceBlock {blockcontents} {
	binary scan $blockcontents c* blockNumberSequence
	set blockNumberSequenceLength [llength $blockNumberSequence]
	for {set i 0} {$i < $blockNumberSequenceLength} {incr i} {
		set blockNumberSequence [lreplace $blockNumberSequence $i $i [expr ([lindex $blockNumberSequence $i] + 0x100)%0x100]]
	}
	return $blockNumberSequence
}

proc WeakChecksum {Xsub_k...Xsub_l} {
	set a(k,i)_ 0
	set b(k,i)_ 0
	set Mod $trsync::Mod
	set k 1
	set l [llength ${Xsub_k...Xsub_l}]
	for {set i $k} {$i <= $l} {incr i} {
		set Xsub_i [lindex ${Xsub_k...Xsub_l} [expr $i - 1]]
		set a(k,i)_ [expr ${a(k,i)_} + $Xsub_i]
		set b(k,i)_ [expr ${b(k,i)_} + (($l - $i + 1) * $Xsub_i)]
	}
	set a(k,l)_ [expr fmod(${a(k,i)_},$Mod)]
	set b(k,l)_ [expr fmod(${b(k,i)_},$Mod)]
	set s(k,l)_ [expr ${a(k,l)_} + ($Mod * ${b(k,l)_})]
	return "${s(k,l)_} ${a(k,l)_} ${b(k,l)_} $k $l [lindex ${Xsub_k...Xsub_l} 0]"
}

proc tdelta {referenceFile targetFile blockSize {sizecheck 0} {fingerprint 0}} {
	if {$::trsync::md5Size < 1} {error "md5Size must be greater than zero."}
	set signature [Digest $targetFile $blockSize $sizecheck $fingerprint]
	return [CopyInstructions $referenceFile $signature]
}

proc tpatch {targetFile copyInstructions fileToConstruct {eolNative 0}} {
	set copyInstructions [lreplace $copyInstructions 0 1 $fileToConstruct $targetFile]
	return [ConstructFile $copyInstructions $eolNative]
}

namespace export tdelta tpatch

}
# end namespace eval ::trsync

