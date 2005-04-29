##################################################
#
# sha1.tcl - SHA1 in Tcl
# Author: Don Libes <libes@nist.gov>, May 2001
# Version 1.0.3
#
# SHA1 defined by FIPS 180-1, "The SHA1 Message-Digest Algorithm",
#          http://www.itl.nist.gov/fipspubs/fip180-1.htm
# HMAC defined by RFC 2104, "Keyed-Hashing for Message Authentication"
#
# Some of the comments below come right out of FIPS 180-1; That's why
# they have such peculiar numbers.  In addition, I have retained
# original syntax, etc. from the FIPS.  All remaining bugs are mine.
#
# HMAC implementation by D. J. Hagberg <dhagberg@millibits.com> and
# is based on C code in FIPS 2104.
#
# For more info, see: http://expect.nist.gov/sha1pure
#
# - Don
##################################################

### Code speedups by Donal Fellows <fellowsd@cs.man.ac.uk> who may well
### have added some extra bugs of his own...  :^)

### Changed the code to use Trf if this package is present on the
### system requiring the sha1 package. Analogous to md5.

package require Tcl 8.2
namespace eval ::sha1 {
}

if {![catch {package require Trf 2.0}] && ![catch {::sha1 -- test}]} {
    # Trf is available, so implement the functionality provided here
    # in terms of calls to Trf for speed.

    proc ::sha1::sha1 {msg} {
	string tolower [::hex -mode encode -- [::sha1 -- $msg]]
    }

    # hmac: hash for message authentication

    # SHA1 of Trf and SHA1 as defined by this package have slightly
    # different results. Trf returns the digest in binary, here we get
    # it as hex-string. In the computation of the HMAC the latter
    # requires back conversion into binary in some places. With Trf we
    # can use omit these. (Not all, the first place must not the changed,
    # see [x]

    proc ::sha1::hmac {key text} {
	# if key is longer than 64 bytes, reset it to SHA1(key).  If shorter, 
	# pad it out with null (\x00) chars.
	set keyLen [string length $key]
	if {$keyLen > 64} {
	    set key [binary format H32 [sha1 $key]]
	    # [x] set key [::sha1 -- $key]
	    set keyLen [string length $key]
	}
    
	# ensure the key is padded out to 64 chars with nulls.
	set padLen [expr {64 - $keyLen}]
	append key [binary format "a$padLen" {}]

	# Split apart the key into a list of 16 little-endian words
	binary scan $key i16 blocks

	# XOR key with ipad and opad values
	set k_ipad {}
	set k_opad {}
	foreach i $blocks {
	    append k_ipad [binary format i [expr {$i ^ 0x36363636}]]
	    append k_opad [binary format i [expr {$i ^ 0x5c5c5c5c}]]
	}
    
	# Perform inner sha1, appending its results to the outer key
	append k_ipad $text
	#append k_opad [binary format H* [sha1 $k_ipad]]
	append k_opad [::sha1 -- $k_ipad]

	# Perform outer sha1
	#sha1 $k_opad
	string tolower [::hex -mode encode -- [::sha1 -- $k_opad]]
    }

} else {
    # Without Trf use the all-tcl implementation by Don Libes.

    namespace eval ::sha1 {
	variable K

	proc initK {} {
	    variable K {}
	    foreach t {
		0x5A827999
		0x6ED9EBA1
		0x8F1BBCDC
		0xCA62C1D6
	    } {
		for {set i 0} {$i < 20} {incr i} {
		    lappend K [expr {int($t)}]
		}
	    }
	}
	initK
    }

    # test sha1
    #
    # This proc is not necessary during runtime and may be omitted if you
    # are simply inserting this file into a production program.
    #
    proc ::sha1::test {} {
	foreach {msg expected} {
	    "abc"
	    "a9993e364706816aba3e25717850c26c9cd0d89d"
	    "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
	    "84983e441c3bd26ebaae4aa1f95129e5e54670f1"
	    "[string repeat a 1000000]"
	    "34aa973cd4c4daa4f61eeb2bdbad27316534016f"
	} {
	    puts "testing: sha1 \"$msg\""
	    set msg [subst $msg]
	    set msgLen [string length $msg]
	    if {$msgLen > 10000} {
		puts "warning: msg length = $msgLen; this may take a while . . ."
	    }
	    set computed [sha1 $msg]
	    puts "expected: $expected"
	    puts "computed: $computed"
	    if {0 != [string compare $computed $expected]} {
		puts "FAILED"
	    } else {
		puts "SUCCEEDED"
	    }
	}
    }

    # time sha1
    #
    # This proc is not necessary during runtime and may be omitted if you
    # are simply inserting this file into a production program.
    #
    proc ::sha1::time {} {
	foreach len {10 50 100 500 1000 5000 10000} {
	    set time [::time {sha1 [format %$len.0s ""]} 10]
	    set msec [lindex $time 0]
	    puts "input length $len: [expr {$msec/1000}] milliseconds per interation"
	}
    }

    proc ::sha1::sha1 {msg} {
	variable K

	#
	# 4. MESSAGE PADDING
	#

	# pad to 512 bits (512/8 = 64 bytes)

	set msgLen [string length $msg]

	# last 8 bytes are reserved for msgLen
	# plus 1 for "1"

	set padLen [expr {56 - $msgLen%64}]
	if {$msgLen % 64 >= 56} {
	    incr padLen 64
	}

	# 4a. and b. append single 1b followed by 0b's
	append msg [binary format "a$padLen" \200]

	# 4c. append 64-bit length
	# Our implementation obviously limits string length to 32bits.
	append msg \0\0\0\0[binary format "I" [expr {8*$msgLen}]]
    
	#
	# 7. COMPUTING THE MESSAGE DIGEST
	#

	# initial H buffer

	set H0 [expr {int(0x67452301)}]
	set H1 [expr {int(0xEFCDAB89)}]
	set H2 [expr {int(0x98BADCFE)}]
	set H3 [expr {int(0x10325476)}]
	set H4 [expr {int(0xC3D2E1F0)}]

	#
	# process message in 16-word blocks (64-byte blocks)
	#

	# convert message to array of 32-bit integers
	# each block of 16-words is stored in M($i,0-16)

	binary scan $msg I* words
	set blockLen [llength $words]

	for {set i 0} {$i < $blockLen} {incr i 16} {
	    # 7a. Divide M[i] into 16 words W[0], W[1], ...
	    set W [lrange $words $i [expr {$i+15}]]

	    # 7b. For t = 16 to 79 let W[t] = ....
	    set t   16
	    set t3  12
	    set t8   7
	    set t14  1
	    set t16 -1
	    for {} {$t < 80} {incr t} {
		set x [expr {[lindex $W [incr t3]] ^ [lindex $W [incr t8]] ^ \
			[lindex $W [incr t14]] ^ [lindex $W [incr t16]]}]
		lappend W [expr {($x << 1) | (($x >> 31) & 1)}]
	    }

	    # 7c. Let A = H[0] ....
	    set A $H0
	    set B $H1
	    set C $H2
	    set D $H3
	    set E $H4

	    # 7d. For t = 0 to 79 do
	    for {set t 0} {$t < 20} {incr t} {
		set TEMP [expr {(($A << 5) | (($A >> 27) & 0x1f)) + \
			(($B & $C) | ((~$B) & $D)) \
			+ $E + [lindex $W $t] + [lindex $K $t]}]
		set E $D
		set D $C
		set C [expr {($B << 30) | (($B >> 2) & 0x3fffffff)}]
		set B $A
		set A $TEMP
	    }
	    for {} {$t<40} {incr t} {
		set TEMP [expr {(($A << 5) | (($A >> 27) & 0x1f)) + \
			($B ^ $C ^ $D) \
			+ $E + [lindex $W $t] + [lindex $K $t]}]
		set E $D
		set D $C
		set C [expr {($B << 30) | (($B >> 2) & 0x3fffffff)}]
		set B $A
		set A $TEMP
	    }
	    for {} {$t<60} {incr t} {
		set TEMP [expr {(($A << 5) | (($A >> 27) & 0x1f)) + \
			(($B & $C) | ($B & $D) | ($C & $D)) \
			+ $E + [lindex $W $t] + [lindex $K $t]}]
		set E $D
		set D $C
		set C [expr {($B << 30) | (($B >> 2) & 0x3fffffff)}]
		set B $A
		set A $TEMP
	    }
	    for {} {$t<80} {incr t} {
		set TEMP [expr {(($A << 5) | (($A >> 27) & 0x1f)) + \
			($B ^ $C ^ $D) \
			+ $E + [lindex $W $t] + [lindex $K $t]}]
		set E $D
		set D $C
		set C [expr {($B << 30) | (($B >> 2) & 0x3fffffff)}]
		set B $A
		set A $TEMP
	    }

	    set H0 [expr {int(($H0 + $A) & 0xffffffff)}]
	    set H1 [expr {int(($H1 + $B) & 0xffffffff)}]
	    set H2 [expr {int(($H2 + $C) & 0xffffffff)}]
	    set H3 [expr {int(($H3 + $D) & 0xffffffff)}]
	    set H4 [expr {int(($H4 + $E) & 0xffffffff)}]
	}

	return [format %0.8x%0.8x%0.8x%0.8x%0.8x $H0 $H1 $H2 $H3 $H4]
    }

    ### These procedures are either inlined or replaced with a normal [format]!
    #
    #proc ::sha1::f {t B C D} {
    #    switch [expr {$t/20}] {
    #	 0 {
    #	     expr {($B & $C) | ((~$B) & $D)}
    #	 } 1 - 3 {
    #	     expr {$B ^ $C ^ $D}
    #	 } 2 {
    #	     expr {($B & $C) | ($B & $D) | ($C & $D)}
    #	 }
    #    }
    #}
    #
    #proc ::sha1::byte0 {i} {expr {0xff & $i}}
    #proc ::sha1::byte1 {i} {expr {(0xff00 & $i) >> 8}}
    #proc ::sha1::byte2 {i} {expr {(0xff0000 & $i) >> 16}}
    #proc ::sha1::byte3 {i} {expr {((0xff000000 & $i) >> 24) & 0xff}}
    #
    #proc ::sha1::bytes {i} {
    #    format %0.2x%0.2x%0.2x%0.2x [byte3 $i] [byte2 $i] [byte1 $i] [byte0 $i]
    #}

    # hmac: hash for message authentication
    proc ::sha1::hmac {key text} {
	# if key is longer than 64 bytes, reset it to SHA1(key).  If shorter, 
	# pad it out with null (\x00) chars.
	set keyLen [string length $key]
	if {$keyLen > 64} {
	    set key [binary format H32 [sha1 $key]]
	    set keyLen [string length $key]
	}

	# ensure the key is padded out to 64 chars with nulls.
	set padLen [expr {64 - $keyLen}]
	append key [binary format "a$padLen" {}]

	# Split apart the key into a list of 16 little-endian words
	binary scan $key i16 blocks

	# XOR key with ipad and opad values
	set k_ipad {}
	set k_opad {}
	foreach i $blocks {
	    append k_ipad [binary format i [expr {$i ^ 0x36363636}]]
	    append k_opad [binary format i [expr {$i ^ 0x5c5c5c5c}]]
	}
    
	# Perform inner sha1, appending its results to the outer key
	append k_ipad $text
	append k_opad [binary format H* [sha1 $k_ipad]]

	# Perform outer sha1
	sha1 $k_opad
    }
}

package provide sha1 1.0.3
