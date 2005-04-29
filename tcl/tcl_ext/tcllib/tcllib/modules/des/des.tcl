#-----------------------------------------------------------------------------
#   Copyright (C) 1999 Jochen C. Loewer   (loewerj@hotmail.com,loewerj@web.de)
#-----------------------------------------------------------------------------
#   
#   A pure-Tcl DES implementation.
#
#
#   <OriginalCopyrightNotice>
#   This DES class has been extracted from package Acme.Crypto for use in VNC.
#   The bytebit[] array has been reversed so that the most significant bit
#   in each byte of the key is ignored, not the least significant.  Also the
#   unnecessary odd parity code has been removed.
#   
#   These changes are:
#    Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
#   
#   This software is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
#   
#   DesCipher - the DES encryption method
#   
#   The meat of this code is by Dave Zimmerman <dzimm@widget.com>, and is:
#   
#   Copyright (c) 1996 Widget Workshop, Inc. All Rights Reserved.
#   
#   Permission to use, copy, modify, and distribute this software
#   and its documentation for NON-COMMERCIAL or COMMERCIAL purposes and
#   without fee is hereby granted, provided that this copyright notice is kept 
#   intact. 
#   WIDGET WORKSHOP MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY
#   OF THE SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
#   TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
#   PARTICULAR PURPOSE, OR NON-INFRINGEMENT. WIDGET WORKSHOP SHALL NOT BE LIABLE
#   FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR
#   DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.
#   
#   THIS SOFTWARE IS NOT DESIGNED OR INTENDED FOR USE OR RESALE AS ON-LINE
#   CONTROL EQUIPMENT IN HAZARDOUS ENVIRONMENTS REQUIRING FAIL-SAFE
#   PERFORMANCE, SUCH AS IN THE OPERATION OF NUCLEAR FACILITIES, AIRCRAFT
#   NAVIGATION OR COMMUNICATION SYSTEMS, AIR TRAFFIC CONTROL, DIRECT LIFE
#   SUPPORT MACHINES, OR WEAPONS SYSTEMS, IN WHICH THE FAILURE OF THE
#   SOFTWARE COULD LEAD DIRECTLY TO DEATH, PERSONAL INJURY, OR SEVERE
#   PHYSICAL OR ENVIRONMENTAL DAMAGE ("HIGH RISK ACTIVITIES").  WIDGET WORKSHOP
#   SPECIFICALLY DISCLAIMS ANY EXPRESS OR IMPLIED WARRANTY OF FITNESS FOR
#   HIGH RISK ACTIVITIES.
#   
#   The rest is:
#   
#   Copyright (C) 1996 by Jef Poskanzer <jef@acme.com>.  All rights reserved.
#   
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#   1. Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#   2. Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#   
#   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS `AS IS'' AND
#   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
#   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
#   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
#   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
#   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
#   SUCH DAMAGE.
#   
#   Visit the ACME Labs Java page for up-to-date versions of this and other
#   fine Java utilities: http:#   www.acme.com/java/
#   </OriginalCopyrightNotice>
#
#
#   $Log: des.tcl,v $
#   Revision 1.7  2004/01/15 06:36:12  andreas_kupries
#
#   	* matrix.man:  Implemented Ed Suominen's sort methods, with
#   	* matrix.tcl:  modifications to speed things up, and to have
#   	* matrix.test: a more standard API (-options).
#
#   	* matrix.man: Documented method links.
#
#   	* matrix.test: Updated test to cover for method links.
#   	* matrix.tcl: Changed the code to determine the list of available
#   	  methods automatically for use in the error message when an
#   	  unknown method is called.
#
#   	* matrix.test:
#   	* matrix.tcl: Namespaces of objects now standalone, and not inside
#   	  of struct::matrix anymore. Arbitrary placement of objects is now
#   	  possible, including nested namespaces. Cleanup of all references
#   	  to instance variables.
#
#   	* matrix.tcl: Made the return of errors more regular.
#
#   	* matrix.tcl: Changed a number of eval calls to the more proper
#   	  'uplevel 1'. This means that an implementation of a method can
#   	  now assume that it is called on the same stack level as the
#   	  method itself.
#
#   	* matrix.tcl: Typo in comments fixed.
#   	* matrix.tcl (__set_rect): Fixed typos in var names causing the
#   	  system to retain bogus cache data.
#
#   Revision 1.6  2003/05/07 21:51:30  patthoyts
#   	* des.tcl, des.man, pkgIndex.tcl: Hiked the version to 0.8.1
#
#   Revision 1.5  2003/05/06 23:49:01  patthoyts
#   	* des.tcl (DesBlock): Change the final result from binary format
#   	to some bit-shifting for tcl < 8.4 to fix for 64 bit platforms.
#
#   Revision 1.4  2003/05/01 00:17:40  andreas_kupries
#
#   	* README-1.4txt: New, overview of changes from 1.3 to 1.4.
#
#   	* installed_modules.tcl: Excluded 'calendar' form the list of
#   	  installed modules/packages. Not yet ready.
#
#   	* sak.tcl (ppackages): Rewritten to use a sub-interpreter for
#   	  retrieving package version information instead of regexes
#   	  etc.
#
#   	  Reverted all changes made to [package provide] commands on
#   	  2003-04-24, except for minor details, like the actual version
#   	  numbers and typos.
#
#   	 Fixes SF Tcllib FR #727694
#
#   Revision 1.2  2003/04/11 18:55:43  andreas_kupries
#
#   	* des.tcl:  Fixed bug #614591.
#
#   Revision 1.1  2003/02/11 23:32:44  patthoyts
#   Initial import of des package.
#
#
#
#   written by Jochen Loewer
#   January 17, 2002
#
#-----------------------------------------------------------------------------


#-----------------------------------------------------------------------------
#  usage:
#
#    to encrypt a 8 byte block:
#    --------------------------
# 
#      DES::GetKey -encrypt <password> encryptKeysArray
#      DES::GetKey -encryptVNC <password> encryptKeysArray
#    
#      set encryptedBlock [DES::DoBlock <PlainText8ByteBlock> encryptKeysArray]
#
#
#    to encrypt a 8 byte block:
#    --------------------------
# 
#      DES::GetKey -decrypt <password> decryptKeysArray
#    
#      set plainText [DES::DoBlock <Encrypted8ByteBlock> decryptKeysArray]
#
#-----------------------------------------------------------------------------


## TODO: Check for weak keys: see http://www.cs.wm.edu/~hallyn/des/weak

namespace eval ::DES {

  variable version 0.8.1

  namespace export GetKey DesBlock

  #-------------------------------------------------------------------------
  #   setup lookup tables once
  #
  #-------------------------------------------------------------------------
  foreach { varName values } {
      bytebitOrig { 0x80 0x40 0x20 0x10 0x08 0x04 0x02 0x01 }
      bytebitVNC  { 0x01 0x02 0x04 0x08 0x10 0x20 0x40 0x80 } 
      bigbyte {
        0x800000 0x400000 0x200000 0x100000
        0x080000 0x040000 0x020000 0x010000
        0x008000 0x004000 0x002000 0x001000
        0x000800 0x000400 0x000200 0x000100
        0x000080 0x000040 0x000020 0x000010
        0x000008 0x000004 0x000002 0x000001
      }
      pc1 {
        56 48 40 32 24 16  8
         0 57 49 41 33 25 17
         9  1 58 50 42 34 26
        18 10  2 59 51 43 35
        62 54 46 38 30 22 14
         6 61 53 45 37 29 21
        13  5 60 52 44 36 28
        20 12  4 27 19 11  3
      }
      pc2 {
        13 16 10 23  0  4 
         2 27 14  5 20  9  
        22 18 11  3 25  7 
        15  6 26 19 12  1  
        40 51 30 36 46 54 
        29 39 50 44 32 47 
        43 48 38 55 33 52 
        45 41 49 35 28 31
      }
      totrot { 1 2 4 6 8 10 12 14 15 17 19 21 23 25 27 28 }      
      SP1A {
        0x01010400 0x00000000 0x00010000 0x01010404
        0x01010004 0x00010404 0x00000004 0x00010000
        0x00000400 0x01010400 0x01010404 0x00000400
        0x01000404 0x01010004 0x01000000 0x00000004
        0x00000404 0x01000400 0x01000400 0x00010400
        0x00010400 0x01010000 0x01010000 0x01000404
        0x00010004 0x01000004 0x01000004 0x00010004
        0x00000000 0x00000404 0x00010404 0x01000000
        0x00010000 0x01010404 0x00000004 0x01010000
        0x01010400 0x01000000 0x01000000 0x00000400
        0x01010004 0x00010000 0x00010400 0x01000004
        0x00000400 0x00000004 0x01000404 0x00010404
        0x01010404 0x00010004 0x01010000 0x01000404
        0x01000004 0x00000404 0x00010404 0x01010400
        0x00000404 0x01000400 0x01000400 0x00000000
        0x00010004 0x00010400 0x00000000 0x01010004 }
      SP2A {
        0x80108020 0x80008000 0x00008000 0x00108020
        0x00100000 0x00000020 0x80100020 0x80008020
        0x80000020 0x80108020 0x80108000 0x80000000
        0x80008000 0x00100000 0x00000020 0x80100020
        0x00108000 0x00100020 0x80008020 0x00000000
        0x80000000 0x00008000 0x00108020 0x80100000
        0x00100020 0x80000020 0x00000000 0x00108000
        0x00008020 0x80108000 0x80100000 0x00008020
        0x00000000 0x00108020 0x80100020 0x00100000
        0x80008020 0x80100000 0x80108000 0x00008000
        0x80100000 0x80008000 0x00000020 0x80108020
        0x00108020 0x00000020 0x00008000 0x80000000
        0x00008020 0x80108000 0x00100000 0x80000020
        0x00100020 0x80008020 0x80000020 0x00100020
        0x00108000 0x00000000 0x80008000 0x00008020
        0x80000000 0x80100020 0x80108020 0x00108000 }
      SP3A {
        0x00000208 0x08020200 0x00000000 0x08020008
        0x08000200 0x00000000 0x00020208 0x08000200
        0x00020008 0x08000008 0x08000008 0x00020000
        0x08020208 0x00020008 0x08020000 0x00000208
        0x08000000 0x00000008 0x08020200 0x00000200
        0x00020200 0x08020000 0x08020008 0x00020208
        0x08000208 0x00020200 0x00020000 0x08000208
        0x00000008 0x08020208 0x00000200 0x08000000
        0x08020200 0x08000000 0x00020008 0x00000208
        0x00020000 0x08020200 0x08000200 0x00000000
        0x00000200 0x00020008 0x08020208 0x08000200
        0x08000008 0x00000200 0x00000000 0x08020008
        0x08000208 0x00020000 0x08000000 0x08020208
        0x00000008 0x00020208 0x00020200 0x08000008
        0x08020000 0x08000208 0x00000208 0x08020000
        0x00020208 0x00000008 0x08020008 0x00020200 }
      SP4A {
        0x00802001 0x00002081 0x00002081 0x00000080
        0x00802080 0x00800081 0x00800001 0x00002001
        0x00000000 0x00802000 0x00802000 0x00802081
        0x00000081 0x00000000 0x00800080 0x00800001
        0x00000001 0x00002000 0x00800000 0x00802001
        0x00000080 0x00800000 0x00002001 0x00002080
        0x00800081 0x00000001 0x00002080 0x00800080
        0x00002000 0x00802080 0x00802081 0x00000081
        0x00800080 0x00800001 0x00802000 0x00802081
        0x00000081 0x00000000 0x00000000 0x00802000
        0x00002080 0x00800080 0x00800081 0x00000001
        0x00802001 0x00002081 0x00002081 0x00000080
        0x00802081 0x00000081 0x00000001 0x00002000
        0x00800001 0x00002001 0x00802080 0x00800081
        0x00002001 0x00002080 0x00800000 0x00802001
        0x00000080 0x00800000 0x00002000 0x00802080 }
      SP5A {
        0x00000100 0x02080100 0x02080000 0x42000100
        0x00080000 0x00000100 0x40000000 0x02080000
        0x40080100 0x00080000 0x02000100 0x40080100
        0x42000100 0x42080000 0x00080100 0x40000000
        0x02000000 0x40080000 0x40080000 0x00000000
        0x40000100 0x42080100 0x42080100 0x02000100
        0x42080000 0x40000100 0x00000000 0x42000000
        0x02080100 0x02000000 0x42000000 0x00080100
        0x00080000 0x42000100 0x00000100 0x02000000
        0x40000000 0x02080000 0x42000100 0x40080100
        0x02000100 0x40000000 0x42080000 0x02080100
        0x40080100 0x00000100 0x02000000 0x42080000
        0x42080100 0x00080100 0x42000000 0x42080100
        0x02080000 0x00000000 0x40080000 0x42000000
        0x00080100 0x02000100 0x40000100 0x00080000
        0x00000000 0x40080000 0x02080100 0x40000100 }
      SP6A {
        0x20000010 0x20400000 0x00004000 0x20404010
        0x20400000 0x00000010 0x20404010 0x00400000
        0x20004000 0x00404010 0x00400000 0x20000010
        0x00400010 0x20004000 0x20000000 0x00004010
        0x00000000 0x00400010 0x20004010 0x00004000
        0x00404000 0x20004010 0x00000010 0x20400010
        0x20400010 0x00000000 0x00404010 0x20404000
        0x00004010 0x00404000 0x20404000 0x20000000
        0x20004000 0x00000010 0x20400010 0x00404000
        0x20404010 0x00400000 0x00004010 0x20000010
        0x00400000 0x20004000 0x20000000 0x00004010
        0x20000010 0x20404010 0x00404000 0x20400000
        0x00404010 0x20404000 0x00000000 0x20400010
        0x00000010 0x00004000 0x20400000 0x00404010
        0x00004000 0x00400010 0x20004010 0x00000000
        0x20404000 0x20000000 0x00400010 0x20004010 }
      SP7A {
        0x00200000 0x04200002 0x04000802 0x00000000
        0x00000800 0x04000802 0x00200802 0x04200800
        0x04200802 0x00200000 0x00000000 0x04000002
        0x00000002 0x04000000 0x04200002 0x00000802
        0x04000800 0x00200802 0x00200002 0x04000800
        0x04000002 0x04200000 0x04200800 0x00200002
        0x04200000 0x00000800 0x00000802 0x04200802
        0x00200800 0x00000002 0x04000000 0x00200800
        0x04000000 0x00200800 0x00200000 0x04000802
        0x04000802 0x04200002 0x04200002 0x00000002
        0x00200002 0x04000000 0x04000800 0x00200000
        0x04200800 0x00000802 0x00200802 0x04200800
        0x00000802 0x04000002 0x04200802 0x04200000
        0x00200800 0x00000000 0x00000002 0x04200802
        0x00000000 0x00200802 0x04200000 0x00000800
        0x04000002 0x04000800 0x00000800 0x00200002 }
      SP8A {
        0x10001040 0x00001000 0x00040000 0x10041040
        0x10000000 0x10001040 0x00000040 0x10000000
        0x00040040 0x10040000 0x10041040 0x00041000
        0x10041000 0x00041040 0x00001000 0x00000040
        0x10040000 0x10000040 0x10001000 0x00001040
        0x00041000 0x00040040 0x10040040 0x10041000
        0x00001040 0x00000000 0x00000000 0x10040040
        0x10000040 0x10001000 0x00041040 0x00040000
        0x00041040 0x00040000 0x10041000 0x00001000
        0x00000040 0x10040040 0x00001000 0x00041040
        0x10001000 0x00000040 0x10000040 0x10040000
        0x10040040 0x10000000 0x00040000 0x10001040
        0x00000000 0x10041040 0x00040040 0x10000040
        0x10040000 0x10001000 0x10001040 0x00000000
        0x10041040 0x00041000 0x00041000 0x00001040
        0x00001040 0x00040040 0x10000000 0x10041000 }
   } { 
      set i -1
      foreach v $values { set ${varName}([incr i]) [expr $v] }
  }

  #-------------------------------------------------------------------------
  #   get internal keys for a later de-/encrypt phase
  #
  #-------------------------------------------------------------------------
  proc GetKey { mode keyString keys_var } {

      upvar $keys_var keys

      # fill keyString up to at least 8 bytes (pad with NULL bytes!)
      append keyString "\0\0\0\0\0\0\0\0"
      binary scan $keyString c8 bytes
      set i  -1
      foreach b $bytes {
         set keyBlock([incr i]) [expr { $b & 0x0ff }]
      }
      switch -- $mode {
          -encrypt { 
              array set keys [makeInternalKeys keyBlock 1 0]
          }
          -encryptVNC { 
              array set keys [makeInternalKeys keyBlock 1 1]
          }
          -decrypt { 
              array set keys [makeInternalKeys keyBlock 0 0]
          }
          -decryptVNC { 
              array set keys [makeInternalKeys keyBlock 0 1]
          }
          default {
              error "mode must be '-encrypt|-encryptVNC|-decrypt|-decryptVNC' !"
          }
      }    
  }

  #-------------------------------------------------------------------------
  #   appplies DES algorithm on a 8 byte block
  #
  #-------------------------------------------------------------------------
  proc DesBlock { in keys_var } {

      upvar $keys_var keys

      if {[info tclversion] == "8.0"} {
          set l [string length $in]
      } else {
          #set l [string bytelength $in]
          set l [string length $in]
      }
      if {$l != 8} {
          error "DES operates only on blocks of 8 bytes, but got $l bytes!"
      }
      binary scan $in II left right
      
      if {[package vsatisfies [package provide Tcl] 8.4]} {
          set r [binary format I* [desAlgorithm $left $right keys]]
      } else {
          foreach r_elt [desAlgorithm $left $right keys] {
              append r [bytes $r_elt]
          }
      }
      return $r
  }

  proc bytes {v} { 
      #format %c%c%c%c [byte 0 $v] [byte 1 $v] [byte 2 $v] [byte 3 $v]
      format %c%c%c%c \
          [expr {((0xFF000000 & $v) >> 24) & 0xFF}] \
          [expr {(0xFF0000 & $v) >> 16}] \
          [expr {(0xFF00 & $v) >> 8}] \
          [expr {0xFF & $v}]
  }

  #-------------------------------------------------------------------------
  #   generate internal key array
  #
  #-------------------------------------------------------------------------
  proc makeInternalKeys { keyBlock_var encDec useVNC } {
 
      upvar $keyBlock_var keyBlock

      variable pc1
      variable pc2
      variable totrot
      variable bigbyte
      variable bytebitOrig
      variable bytebitVNC
     
      for { set j  0 } { $j < 56 } { incr j } {
          set l $pc1($j)
          set m [expr $l & 07]
          if {$useVNC} {
              set pc1m($j) [expr { ( ($keyBlock([expr {$l >> 3}]) & $bytebitVNC($m)) != 0 ) ? 1: 0 }]
          } else {
              set pc1m($j) [expr { ( ($keyBlock([expr {$l >> 3}]) & $bytebitOrig($m)) != 0 ) ? 1: 0 }]
          }
      }
      for { set i 0 } { $i < 16 } { incr i } {

          set m [expr { $encDec ? ($i << 1) : ((15-$i) << 1) }]
          set n [expr $m + 1]
          set kn($m) 0 
          set kn($n) 0
          for { set j 0 } { $j < 28 } { incr j } {

              set l [expr { $j + $totrot($i) }]
              if { $l < 28 } { 
                  set pcr($j) $pc1m($l)
              } else {
                  set pcr($j) $pc1m([expr { $l - 28 }])
              }
          }
          for { set j 28 } { $j < 56 } { incr j } {
              set l [expr { $j + $totrot($i) }]
              if { $l < 56 } { 
                  set pcr($j) $pc1m($l)
              } else {
                  set pcr($j) $pc1m([expr { $l - 28 }])
              }
          }
          for { set j 0 } { $j < 24 } { incr j } {
              if {$pcr($pc2($j)) != 0} { 
                  set kn($m) [expr { $kn($m) | $bigbyte($j) }] 
              }
              if {$pcr($pc2([expr $j+24])) != 0} {
                  set kn($n) [expr { $kn($n) | $bigbyte($j) }]
              }
          }
      }
      for { set i 0; set rawi 0; set KnLi 0 } { $i < 16 } { incr i } {
          set raw0 $kn($rawi); incr rawi
          set raw1 $kn($rawi); incr rawi
          set KnL($KnLi) [expr { (($raw0 & 0x00fc0000) <<  6)
                                |(($raw0 & 0x00000fc0) << 10)
                                |(($raw1 & 0x00fc0000) >> 10)
                                |(($raw1 & 0x00000fc0) >>  6) }]
          incr KnLi
          set KnL($KnLi) [expr { (($raw0 & 0x0003f000) <<  12)
                                |(($raw0 & 0x0000003f) <<  16)
                                |(($raw1 & 0x0003f000) >>  4)
                                |( $raw1 & 0x0000003f)        }] 
          incr KnLi
      }
      return [array get KnL]
  }


  #-------------------------------------------------------------------------
  #   applies the DES algorithm to two 4 byte integers (8 byte block)
  #   using the internal de-/encrypt keys
  #
  #-------------------------------------------------------------------------
  proc desAlgorithm { leftt right keys_var } {

      upvar $keys_var keys

      variable SP1A
      variable SP2A
      variable SP3A
      variable SP4A
      variable SP5A
      variable SP6A
      variable SP7A
      variable SP8A

      set keysi 0

      set work  [expr { ((($leftt >> 4)&0x0fffffff) ^ $right) & 0x0f0f0f0f }]
      set right [expr { $right ^ $work }]
      set leftt [expr { $leftt ^ ($work << 4) }]

      set work  [expr { ((($leftt >> 16)&0x0000ffff) ^ $right) & 0x0000ffff }]
      set right [expr { $right ^ $work }]
      set leftt [expr { $leftt ^ ($work << 16) }]

      set work  [expr { ((($right >> 2)&0x3fffffff) ^ $leftt) & 0x33333333 }]
      set leftt [expr { $leftt ^ $work }]
      set right [expr { $right ^ ($work << 2) }]

      set work  [expr { ((($right >> 8)&0x00ffffff) ^ $leftt) & 0x00ff00ff }]
      set leftt [expr { $leftt ^ $work }]
      set right [expr { $right ^ ($work << 8) }]
      set right [expr { ($right << 1) | (($right >> 31) & 1)  }]

      set work  [expr { ($leftt ^ $right) & 0xaaaaaaaa }]
      set leftt [expr { $leftt ^ $work }]
      set right [expr { $right ^ $work }]
      set leftt [expr { ($leftt << 1) | (($leftt >> 31) & 1) }]
  
      for { set round 0 } { $round < 8 } { incr round } {
          set work [expr { ($right << 28) | (($right >> 4)&0x0fffffff) }]
          set work [expr { $work ^ $keys($keysi) } ]
          incr keysi
          set fval [expr {  $SP7A([expr {  $work        & 0x0000003f }])
                          | $SP5A([expr { ($work >>  8) & 0x0000003f }])
                          | $SP3A([expr { ($work >> 16) & 0x0000003f }])
                          | $SP1A([expr { ($work >> 24) & 0x0000003f }]) }]
          set work [expr { $right ^ $keys($keysi) }]
          incr keysi
          set fval [expr {  $fval 
                          | $SP8A([expr {  $work        & 0x0000003f }])
                          | $SP6A([expr { ($work >>  8) & 0x0000003f }])
                          | $SP4A([expr { ($work >> 16) & 0x0000003f }])
                          | $SP2A([expr { ($work >> 24) & 0x0000003f }]) }]
          set leftt [expr { $leftt ^ $fval }]
          set work  [expr { ($leftt << 28) | (($leftt >> 4)&0x0fffffff) }]
          set work  [expr { $work ^ $keys($keysi) }]
          incr keysi
          set fval [expr {  $SP7A([expr {  $work        & 0x0000003f }])
                          | $SP5A([expr { ($work >>  8) & 0x0000003f }])
                          | $SP3A([expr { ($work >> 16) & 0x0000003f }])
                          | $SP1A([expr { ($work >> 24) & 0x0000003f }]) }]
          set work [expr { $leftt ^ $keys($keysi) }]
          incr keysi
          set fval [expr {  $fval 
                          | $SP8A([expr {  $work        & 0x0000003f }])
                          | $SP6A([expr { ($work >>  8) & 0x0000003f }])
                          | $SP4A([expr { ($work >> 16) & 0x0000003f }])
                          | $SP2A([expr { ($work >> 24) & 0x0000003f }]) }]
          set right [expr { $right ^ $fval }]
      }
      set right [expr { ($right << 31) | (($right >> 1)&0x7fffffff) }]
      set work  [expr { ($leftt ^ $right) & 0xaaaaaaaa }]
      set leftt [expr { $leftt ^ $work }]
      set right [expr { $right ^ $work }]

      set leftt [expr { ($leftt << 31) | (($leftt >> 1)&0x7fffffff) }]

      set work  [expr { ((($leftt >> 8)&0x00ffffff) ^ $right) & 0x00ff00ff }]
      set right [expr { $right ^ $work }]
      set leftt [expr { $leftt ^ ($work << 8) }]

      set work  [expr { ((($leftt >> 2)&0x3fffffff) ^ $right) & 0x33333333 }]
      set right [expr { $right ^ $work }]
      set leftt [expr { $leftt ^ ($work << 2) }]

      set work  [expr { ((($right >> 16)&0x0000ffff) ^ $leftt) & 0x0000ffff }]
      set leftt [expr { $leftt ^ $work }]
      set right [expr { $right ^ ($work << 16) }]
      
      set work  [expr { ((($right >>  4)&0x0fffffff) ^ $leftt) & 0x0f0f0f0f }]
      set leftt [expr { ($leftt ^ $work) &0xffffffff }]
      set right [expr { ($right ^ ($work << 4)) & 0xffffffff }]
      
      return [list $right $leftt]
  }

}

# -------------------------------------------------------------------------
# Description:
#  Pop the nth element off a list. Used in options processing.
#
proc ::DES::Pop {varname {nth 0}} {
    upvar $varname args
    set r [lindex $args $nth]
    set args [lreplace $args $nth $nth]
    return $r
}

# -------------------------------------------------------------------------

proc ::DES::des {args} {
    array set opts [list filename {} mode {encode} key {I love Tcl!}]
    while {[string match -* [lindex $args 0]]} {
        switch -glob -- [lindex $args 0] {
            -f* {set opts(filename) [Pop args 1]}
            -m* {set opts(mode) [Pop args 1]}
            -k* {set opts(key) [Pop args 1]}
            --   {Pop args ; break }
            default {
                set err [join [lsort [array names opts]] ", -"]
                return -code error "bad option [lindex $args 0]:\
                       must be one of -$options"
            }
        }
        Pop args
    }

    # Build the key
    switch -exact -- $opts(mode) {
        encode { GetKey -encrypt $opts(key) key }
        decode { GetKey -decrypt $opts(key) key }
        default {
            return -code error "bad option \"$opts(mode)\": \
                   must be either \"encode\" or \"decode\""
        }
    }

    set r {}
    if {$opts(filename) != {}} {
        set f [open $opts(filename) r]
        fconfigure $f -translation binary
        while {![eof $f]} {
            set d [read $f 8]
            if {[set n [string length $d]] < 8} {
                append d [string repeat \0 [expr {8 - $n}]]
            }
            append r [DesBlock $d key]
        }
        close $f
    } else {
        set data [lindex $args 0]
        if {[set n [expr {[string length $data] % 8}]] != 0} {
            append data [string repeat \0 [expr {8 - $n}]]
        }
        for {set n 0} {$n < [string length $data]} {incr n 8} {
            append r [DesBlock [string range $data $n [expr {$n + 7}]] key]
        }
    }

    return $r
}

# -------------------------------------------------------------------------

package provide des $DES::version

# -------------------------------------------------------------------------
#
# Local variables:
#   mode: tcl
#   indent-tabs-mode: nil
# End:
