##
# Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
# Reserved.  This file contains Original Code and/or Modifications of
# Original Code as defined in and that are subject to the Apple Public
# Source License Version 1.0 (the 'License').  You may not use this file
# except in compliance with the License.  Please obtain a copy of the
# License at http://www.apple.com/publicsource and read it before using
# this file.
# 
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License."
# 
# @APPLE_LICENSE_HEADER_END@
##
# Version: 1.2.1  Date: 1-30-2001
use-nvramrc? true
load-base 600000
diag-device 
nvramrc hex\
: $D find-device ;\
: $E device-end ;\
: $L BLpatch ; : $R BRpatch ;\
: $X execute ;\
: $p 0 to my-self property ;\
: $a " /chosen" $D $p $E ;\
: helpb ['] install-interrupt-vectors ['] noop $R\
0 4000 release-mem 8000 2000 release-mem ;\
10 buffer: km\
dev kbd\
get-key-map km swap move\
$E\
: ck 0 do swap dup 3 >> km + c@ 1 rot 7 and << and or loop ;\
: bootr 0d word count encode-string " machargs" $a\
0 0 1 ck if 0 and else dup 1 = if 3d 0 1 else f 3d 0 2 then ck if 40 or then then\
40 and if bye else helpb 1e 0 do ['] boot catch drop 1f4 ms loop then bye ;\
: myboot boot-command eval ;\
dev /packages/mac-parts\
: $M 7F00 - 4 ;\
' my-init-program 34 + ' $M $L\
' load-partition dup\
80 + ' 2drop $L\
104 + ' 0 $L\
' load 15C + ' 0 $L\
$E\
dev /packages/obp-tftp\
: $M dup 24 - HIS-ENET-HA 6 move 14 + ;\
' open 66C - ' $M $L\
$E\
dev mac-io\
: decode-unit parse-1hex ;\
$E\
ff000000 dup dup 400 28 do-map 4+ w@ 10 and 0=\
if 90b7 f3000032 w! then\
unselect-dev
