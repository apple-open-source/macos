#! /bin/sh
#$Xorg: Xmark.sh,v 1.3 2000/08/17 19:54:08 cpqbld Exp $
#XPC Header: Xmark,v 1.15 93/04/12 10:10:07 hmgr Exp
#
############################################################
# Xmark version XPC Revision: 1.15
#
# Usage: Xmark datafile
#
############################################################
# $XFree86: xc/programs/x11perf/Xmark.sh,v 1.3 2001/01/17 23:45:10 dawes Exp $
#
#
# CHANGE HISTORY:
#
# X11perfcompDR  --  Creates a Digital Review compatible breakdown of 
# x11perf v1.2 results as described in "Take Your Pick Of Graphics"
# by John Montgomery, Digital Review, May 7th, 1990, Page 44. 
# 8/30/90  v1.0 program created by Lonnie Mandigo 
# 1/18/91  Modification by  Lonnie Mandigo
#	   Fixed a bug to include UCIRC in data.
# 2/5/91   Modification by  Lonnie Mandigo
#          Now removes sum.tmp file when finished.
# 3/18/91  v1.01 Increased resolution to handle zero ratios.
#          Minor aesethic cleanups and better error handling.
#          Lonnie Mandigo and Jason Levitt (jason@cs.utexas.edu)
# 3/4/93   Converted to Xmark by Bob Kuseski
#          to produce a single number representing the ratio between 
#	   the xpc weighted averages of the output from x11perf Rev 1.3
#	   for the server under test and a SparcStation 1
# 3/10/93  Changed to produce THREE output numbers and added bounds
#	   checking of input data.
# 3/15/93  Removed usage of '-F' in grep for compatibility reasons
# 3/16/93  Corrected usage of substr() and used two greps versus fgrep
# 4/12/93  Eliminated use of functions and \n in echo
#
############################################################
# Copyright (c) 1993 by Hewlett-Packard Company
#
# Permission to use, copy, modify, and  distribute  this  software and its
# documentation  for  any  purpose  and  without  fee is  hereby  granted,
# provided that the above  copyright  notice appear in all copies and that
# both  the  copyright  notice  and  this  permission   notice  appear  in
# supporting  documentation, and that the name of  Hewlett-Packard  not be
# used in  advertising  or publicity  pertaining  to  distribution  of the
# software without specific, written prior permission.
#
############################################################
# Instructions:
# 
# Usage: Xmark datafile
#
# where: 'datafile' is created by running x11perf Rev 1.3
# with the flags below:
#
#      x11perf -display mysystem:0.0 -v1.3 -rop GXcopy GXxor -all > datafile
#   or
#      x11perf -display mysystem:0.0 -v1.3 -rop GXcopy -all > datafile
#      x11perf -display mysystem:0.0 -v1.3 -rop GXxor -all >> datafile
#   or
#      x11perf -display mysystem:0.0 -v1.3 -rop GXxor -all > datafile
#      x11perf -display mysystem:0.0 -v1.3 -rop GXcopy -all >> datafile
#
# Xmark summarizes the results relative to the data contained in
# the datafile.  Xmark writes to standard out, so if you want to
# capture the output in a file, use:
# 
#   Xmark datafile > output.comp
#
# The 'output.comp' contains THREE numbers:
#	    - Weighted x11perf number of the server under test
#	    - Weighted x11perf number of the SparcStation 1
#	    - Xmark = the ratio of the above two numbers
#
# Note the SparcStation 1 number comes from a:
#   X11R5 Xsun
#   Standard with SunOS 4.1.2
#   SunOs 4.1.2
#   CG3 dumb Color Frame Buffer
#
############################################################
############################################################

# Cleanup on interrupt
trap \
"echo Interrupt: removing temporary files. >& 2 ;rm -f temp.$$ rates.$$ awkfile.$$; exit 1" 1 2 3 9 15

# Must have only one file specified
if [ "$#" -ne 1 ]
then
    echo "Usage: $0 datafile" >& 2
    exit 1
fi

# Must be an ordinary file
if [ ! -f "$1" ]
then
    echo "Error: data file does not exist or is not ordinary." >& 2
    exit 1
fi

# See if the date file has the correct number of results.
LC1=`grep trep "$1" | wc -l`
LC2=441		# Number of test without Shared Memory Transport
LC3=447		# Number of test with Shared Memory Transport

if [ "$LC1" -ne "$LC2" ] && [ "$LC1" -ne "$LC3" ]
then
    echo "WARNING: datafile contains $LC1, not "$LC2" or "$LC3" 'trep' results;" >& 2
    if [ "$LC1" -gt "$LC2" ]
    then
	echo "extra results are probably OK." >& 2
	echo ""
    fi
fi
 
DATA=$1

grep trep $DATA		> temp.$$
grep server $DATA	>> temp.$$

# convert the averages to standard form.

cat > awkfile.$$ <<'EOS'
    BEGIN {
	weight["10x10 rectangle"] = 37;
	weight["Scroll 500x500 pixels"] = 33;
	weight["100x100 rectangle"] = 32;
	weight["10-pixel line segment"] = 31;
	weight["10-pixel line"] = 30;
	weight["PutImage 100x100 square"] = 30;
	weight["Scroll 100x100 pixels"] = 29;
	weight["PutImage 500x500 square"] = 29;
	weight["500x500 rectangle"] = 29;
	weight["100-pixel line"] = 28;
	weight["Char in 60-char line (9x15)"] = 28;
	weight["Char in 80-char image line (6x13)"] = 28;
	weight["Char in 80-char line (6x13)"] = 27;
	weight["Char in 80-char image line (TR 10)"] = 27;
	weight["100-pixel line segment"] = 27;
	weight["Char in 80-char line (TR 10)"] = 27;
	weight["Char in 60-char image line (9x15)"] = 26;
	weight["Copy 100x100 from pixmap to window"] = 26;
	weight["Copy 500x500 from pixmap to window"] = 26;
	weight["Char in 70-char line (8x13)"] = 24;
	weight["500-pixel line"] = 24;
	weight["Char in 20/40/20 line (6x13, TR 10)"] = 23;
	weight["Char in 70-char image line (8x13)"] = 23;
	weight["Change graphics context"] = 23;
	weight["Copy 100x100 from window to window"] = 23;
	weight["10-pixel horizontal line segment"] = 23;
	weight["10-pixel vertical line segment"] = 23;
	weight["Copy 500x500 from window to window"] = 22;
	weight["Char in 30-char line (TR 24)"] = 22;
	weight["Char16 in 40-char line (k14)"] = 22;
	weight["500-pixel line segment"] = 22;
	weight["Char in 30-char image line (TR 24)"] = 22;
	weight["100-pixel horizontal line segment"] = 22;
	weight["100-pixel vertical line segment"] = 22;
	weight["Scroll 10x10 pixels"] = 21;
	weight["Char16 in 40-char image line (k14)"] = 21;
	weight["GetImage 500x500 square"] = 21;
	weight["PutImage 10x10 square"] = 20;
	weight["GetImage 100x100 square"] = 20;
	weight["Move window (4 kids)"] = 20;
	weight["Copy 10x10 from pixmap to window"] = 20;
	weight["100x100 4x4 tiled rectangle"] = 20;
	weight["Copy 100x100 from window to pixmap"] = 19;
	weight["Hide/expose window via popup (4 kids)"] = 19;
	weight["Copy 500x500 from window to pixmap"] = 19;
	weight["500-pixel horizontal line segment"] = 19;
	weight["500-pixel vertical line segment"] = 19;
	weight["Fill 10x10 equivalent triangle"] = 19;
	weight["Char16 in 23-char line (k24)"] = 19;
	weight["1x1 rectangle"] = 19;
	weight["Char16 in 7/14/7 line (k14, k24)"] = 18;
	weight["10x10 4x4 tiled rectangle"] = 18;
	weight["100-pixel line segment (1 kid)"] = 18;
	weight["100x100 rectangle outline"] = 18;
	weight["Move window via parent (4 kids)"] = 18;
	weight["GetAtomName"] = 18;
	weight["GetProperty"] = 18;
	weight["Char16 in 23-char image line (k24)"] = 18;
	weight["100-pixel line segment (3 kids)"] = 17;
	weight["Resize window (4 kids)"] = 17;
	weight["Map window via parent (4 kids)"] = 17;
	weight["Copy 100x100 from pixmap to pixmap"] = 17;
	weight["Unmap window via parent (4 kids)"] = 17;
	weight["100-pixel line segment (2 kids)"] = 17;
	weight["10-pixel solid circle"] = 17;
	weight["1-pixel line"] = 17;
	weight["Move window (16 kids)"] = 16;
	weight["Fill 100x100 equivalent triangle"] = 16;
	weight["Hide/expose window via popup (16 kids)"] = 16;
	weight["Move window via parent (16 kids)"] = 16;
	weight["10x10 rectangle outline"] = 16;
	weight["Destroy window via parent (4 kids)"] = 16;
	weight["Create unmapped window (4 kids)"] = 16;
	weight["Copy 10x10 from window to window"] = 16;
	weight["Copy 100x100 1-bit deep plane"] = 16;
	weight["10-pixel circle"] = 16;
	weight["100x100 opaque stippled rectangle"] = 16;
	weight["500x500 4x4 tiled rectangle"] = 16;
	weight["100x100 stippled rectangle"] = 16;
	weight["Copy 500x500 from pixmap to pixmap"] = 16;
	weight["Create and map subwindows (16 kids)"] = 16;
	weight["10x10 stippled rectangle"] = 16;
	weight["Resize window (16 kids)"] = 16;
	weight["Map window via parent (16 kids)"] = 15;
	weight["100-pixel solid circle"] = 15;
	weight["Copy 500x500 1-bit deep plane"] = 15;
	weight["500x500 rectangle outline"] = 15;
	weight["Create and map subwindows (4 kids)"] = 15;
	weight["Destroy window via parent (16 kids)"] = 15;
	weight["10x1 wide horizontal line segment"] = 15;
	weight["10x1 wide vertical line segment"] = 15;
	weight["100x100 161x145 tiled rectangle"] = 15;
	weight["1-pixel line segment"] = 15;
	weight["Unmap window via parent (16 kids)"] = 15;
	weight["Fill 100x100 trapezoid"] = 14;
	weight["100-pixel circle"] = 14;
	weight["10x10 opaque stippled rectangle"] = 14;
	weight["100-pixel dashed line"] = 14;
	weight["500x500 161x145 tiled rectangle"] = 14;
	weight["Copy 10x10 from window to pixmap"] = 14;
	weight["100x10 wide horizontal line segment"] = 14;
	weight["100x10 wide vertical line segment"] = 14;
	weight["100x100 wide rectangle outline"] = 14;
	weight["100x100 216x208 tiled rectangle"] = 14;
	weight["Dot"] = 14;
	weight["10x10 161x145 tiled rectangle"] = 14;
	weight["Fill 100x100 equivalent complex polygons"] = 14;
	weight["Move window (50 kids)"] = 13;
	weight["Move window via parent (50 kids)"] = 13;
	weight["Circulate window (16 kids)"] = 13;
	weight["100-pixel dashed segment"] = 13;
	weight["Create and map subwindows (50 kids)"] = 13;
	weight["Create and map subwindows (25 kids)"] = 13;
	weight["10x10 wide rectangle outline"] = 13;
	weight["500x500 216x208 tiled rectangle"] = 13;
	weight["Hide/expose window via popup (25 kids)"] = 13;
	weight["Create and map subwindows (100 kids)"] = 13;
	weight["Map window via parent (50 kids)"] = 13;
	weight["Fill 10x10 trapezoid"] = 13;
	weight["100x10 wide line"] = 13;
	weight["10x10 216x208 tiled rectangle"] = 13;
	weight["10-pixel dashed line"] = 13;
	weight["Map window via parent (100 kids)"] = 13;
	weight["Copy 10x10 from pixmap to pixmap"] = 13;
	weight["Map window via parent (25 kids)"] = 13;
	weight["Create unmapped window (25 kids)"] = 13;
	weight["Move window (25 kids)"] = 13;
	weight["Circulate window (4 kids)"] = 12;
	weight["Move window via parent (25 kids)"] = 12;
	weight["Create unmapped window (16 kids)"] = 12;
	weight["Create and map subwindows (75 kids)"] = 12;
	weight["500x500 opaque stippled rectangle"] = 12;
	weight["Hide/expose window via popup (100 kids)"] = 12;
	weight["Create and map subwindows (200 kids)"] = 12;
	weight["Hide/expose window via popup (50 kids)"] = 12;
	weight["500x50 wide horizontal line segment"] = 12;
	weight["500x50 wide vertical line segment"] = 12;
	weight["100x100 161x145 opaque stippled rectangle"] = 12;
	weight["Resize window (25 kids)"] = 12;
	weight["Map window via parent (200 kids)"] = 12;
	weight["10x10 161x145 opaque stippled rectangle"] = 12;
	weight["X protocol NoOperation"] = 12;
	weight["10-pixel dashed segment"] = 12;
	weight["Resize unmapped window (16 kids)"] = 12;
	weight["Create unmapped window (100 kids)"] = 12;
	weight["Fill 10x10 equivalent complex polygon"] = 12;
	weight["Map window via parent (75 kids)"] = 12;
	weight["100x100 161x145 stippled rectangle"] = 12;
	weight["Moved unmapped window (16 kids)"] = 12;
	weight["Copy 10x10 1-bit deep plane"] = 12;
	weight["Unmap window via parent (50 kids)"] = 12;
	weight["GetImage 10x10 square"] = 12;
	weight["Fill 100x100 tiled trapezoid"] = 12;
	weight["500x500 wide rectangle outline"] = 12;
	weight["Fill 100x100 stippled trapezoid"] = 12;
	weight["500x500 stippled rectangle"] = 12;
	weight["Moved unmapped window (4 kids)"] = 12;
	weight["10x10 161x145 stippled rectangle"] = 12;
	weight["Unmap window via parent (25 kids)"] = 12;
	weight["500-pixel solid circle"] = 12;
	weight["Create unmapped window (50 kids)"] = 12;
	weight["Destroy window via parent (25 kids)"] = 11;
	weight["Resize window (200 kids)"] = 11;
	weight["100-pixel filled ellipse"] = 11;
	weight["Hide/expose window via popup (200 kids)"] = 11;
	weight["Unmap window via parent (100 kids)"] = 11;
	weight["Circulate window (25 kids)"] = 11;
	weight["Resize window (50 kids)"] = 11;
	weight["Hide/expose window via popup (75 kids)"] = 11;
	weight["Destroy window via parent (100 kids)"] = 11;
	weight["Destroy window via parent (50 kids)"] = 11;
	weight["Create unmapped window (200 kids)"] = 11;
	weight["Fill 100x100 opaque stippled trapezoid"] = 11;
	weight["Move window via parent (75 kids)"] = 11;
	weight["500-pixel filled ellipse"] = 11;
	weight["Resize unmapped window (4 kids)"] = 11;
	weight["Move window via parent (200 kids)"] = 11;
	weight["Move window (75 kids)"] = 11;
	weight["Fill 10x10 tiled trapezoid"] = 11;
	weight["Fill 100x100 161x145 stippled trapezoid"] = 11;
	weight["Move window (200 kids)"] = 11;
	weight["Fill 10x10 stippled trapezoid"] = 11;
	weight["Create unmapped window (75 kids)"] = 11;
	weight["100-pixel fill chord partial circle"] = 10;
	weight["Circulate Unmapped window (16 kids)"] = 10;
	weight["Circulate window (50 kids)"] = 10;
	weight["Move window (100 kids)"] = 10;
	weight["Circulate window (100 kids)"] = 10;
	weight["Move window via parent (100 kids)"] = 10;
	weight["500x50 wide line"] = 10;
	weight["500-pixel circle"] = 10;
	weight["100-pixel double-dashed line"] = 10;
	weight["Unmap window via parent (200 kids)"] = 10;
	weight["Moved unmapped window (50 kids)"] = 10;
	weight["10-pixel filled ellipse"] = 10;
	weight["Resize window (75 kids)"] = 10;
	weight["Fill 100x100 161x145 tiled trapezoid"] = 10;
	weight["500x500 161x145 opaque stippled rectangle"] = 10;
	weight["Fill 100x100 161x145 opaque stippled trapezoid"] = 10;
	weight["Unmap window via parent (75 kids)"] = 10;
	weight["Resize unmapped window (25 kids)"] = 10;
	weight["100-pixel ellipse"] = 10;
	weight["Destroy window via parent (75 kids)"] = 10;
	weight["Fill 1x1 equivalent triangle"] = 10;
	weight["500-pixel ellipse"] = 10;
	weight["Fill 10x10 opaque stippled trapezoid"] = 10;
	weight["Resize window (100 kids)"] = 10;
	weight["500x500 161x145 stippled rectangle"] = 10;
	weight["Destroy window via parent (200 kids)"] = 10;
	weight["100-pixel fill slice partial circle"] = 10;
	weight["Fill 100x100 216x208 tiled trapezoid"] = 9;
	weight["100-pixel wide circle"] = 9;
	weight["Moved unmapped window (25 kids)"] = 9;
	weight["10x1 wide line"] = 9;
	weight["Resize unmapped window (100 kids)"] = 9;
	weight["100-pixel double-dashed segment"] = 9;
	weight["10-pixel fill chord partial circle"] = 9;
	weight["Circulate window (75 kids)"] = 9;
	weight["100-pixel partial circle"] = 9;
	weight["Circulate window (200 kids)"] = 9;
	weight["Fill 10x10 161x145 stippled trapezoid"] = 9;
	weight["Circulate Unmapped window (4 kids)"] = 9;
	weight["100-pixel dashed circle"] = 9;
	weight["Circulate Unmapped window (25 kids)"] = 9;
	weight["10-pixel ellipse"] = 9;
	weight["Resize unmapped window (50 kids)"] = 9;
	weight["100-pixel partial ellipse"] = 9;
	weight["Fill 10x10 161x145 tiled trapezoid"] = 8;
	weight["100x10 wide dashed line"] = 8;
	weight["10-pixel partial circle"] = 8;
	weight["100-pixel wide ellipse"] = 8;
	weight["Fill 10x10 161x145 opaque stippled trapezoid"] = 8;
	weight["100-pixel wide partial circle"] = 8;
	weight["Moved unmapped window (75 kids)"] = 8;
	weight["Resize unmapped window (75 kids)"] = 8;
	weight["Circulate Unmapped window (100 kids)"] = 8;
	weight["100-pixel fill slice partial ellipse"] = 8;
	weight["100x10 wide double-dashed line"] = 8;
	weight["10-pixel fill slice partial circle"] = 8;
	weight["100-pixel dashed ellipse"] = 8;
	weight["100-pixel fill chord partial ellipse"] = 8;
	weight["100-pixel wide dashed circle"] = 8;
	weight["100-pixel double-dashed circle"] = 8;
	weight["Fill 10x10 216x208 tiled trapezoid"] = 7;
	weight["Moved unmapped window (100 kids)"] = 7;
	weight["10-pixel wide circle"] = 7;
	weight["Moved unmapped window (200 kids)"] = 7;
	weight["Resize unmapped window (200 kids)"] = 7;
	weight["Circulate Unmapped window (50 kids)"] = 7;
	weight["1-pixel circle"] = 7;
	weight["10-pixel partial ellipse"] = 7;
	weight["500-pixel wide circle"] = 7;
	weight["500-pixel wide ellipse"] = 7;
	weight["100-pixel wide partial ellipse"] = 7;
	weight["Circulate Unmapped window (75 kids)"] = 7;
	weight["100-pixel wide dashed ellipse"] = 7;
	weight["100-pixel double-dashed ellipse"] = 7;
	weight["10-pixel wide ellipse"] = 6;
	weight["10-pixel wide partial circle"] = 6;
	weight["1-pixel solid circle"] = 6;
	weight["100-pixel wide double-dashed circle"] = 6;
	weight["Circulate Unmapped window (200 kids)"] = 6;
	weight["1x1 4x4 tiled rectangle"] = 6;
	weight["10-pixel wide partial ellipse"] = 6;
	weight["10-pixel fill chord partial ellipse"] = 6;
	weight["10-pixel fill slice partial ellipse"] = 6;
	weight["100-pixel wide double-dashed ellipse"] = 5;
	weight["1x1 161x145 tiled rectangle"] = 5;
	weight["1x1 216x208 tiled rectangle"] = 5;
	weight["1x1 stippled rectangle"] = 5;
	weight["1x1 opaque stippled rectangle"] = 4;
	weight["1x1 161x145 opaque stippled rectangle"] = 4;
	weight["1x1 161x145 stippled rectangle"] = 4;
	weight["ShmPutImage 10x10 square"] = 0;
	weight["ShmPutImage 100x100 square"] = 0;
	weight["ShmPutImage 500x500 square"] = 0;
	weight["(xor) 100-pixel line"] = 16;
	weight["(xor) 10-pixel line segment"] = 14;
	weight["(xor) 10-pixel line"] = 13;
	weight["(xor) 100-pixel line segment"] = 13;
	weight["(xor) 500-pixel line segment"] = 13;
	weight["(xor) 500-pixel line"] = 12;
	weight["(xor) 10x10 rectangle"] = 12;
	weight["(xor) 100x100 rectangle"] = 12;
	weight["(xor) 100-pixel vertical line segment"] = 10;
	weight["(xor) 100x100 rectangle outline"] = 10;
	weight["(xor) 100-pixel horizontal line segment"] = 10;
	weight["(xor) 10-pixel vertical line segment"] = 10;
	weight["(xor) 100-pixel line segment (2 kids)"] = 10;
	weight["(xor) 10-pixel horizontal line segment"] = 10;
	weight["(xor) 100-pixel line segment (1 kid)"] = 9;
	weight["(xor) Char in 80-char line (6x13)"] = 9;
	weight["(xor) 500-pixel vertical line segment"] = 9;
	weight["(xor) Copy 100x100 from pixmap to window"] = 9;
	weight["(xor) 10x10 rectangle outline"] = 9;
	weight["(xor) 500-pixel horizontal line segment"] = 9;
	weight["(xor) 100-pixel line segment (3 kids)"] = 9;
	weight["(xor) PutImage 100x100 square"] = 9;
	weight["(xor) 500x500 rectangle outline"] = 9;
	weight["(xor) Char in 80-char line (TR 10)"] = 9;
	weight["(xor) 500x500 rectangle"] = 9;
	weight["(xor) Copy 500x500 from pixmap to window"] = 8;
	weight["(xor) Char in 20/40/20 line (6x13, TR 10)"] = 8;
	weight["(xor) 100-pixel dashed line"] = 8;
	weight["(xor) Copy 100x100 1-bit deep plane"] = 8;
	weight["(xor) Char in 60-char line (9x15)"] = 8;
	weight["(xor) Char16 in 40-char line (k14)"] = 7;
	weight["(xor) Fill 10x10 equivalent triangle"] = 7;
	weight["(xor) PutImage 500x500 square"] = 7;
	weight["(xor) Copy 100x100 from window to window"] = 7;
	weight["(xor) 100-pixel dashed segment"] = 7;
	weight["(xor) 100x100 wide rectangle outline"] = 6;
	weight["(xor) Char16 in 7/14/7 line (k14, k24)"] = 6;
	weight["(xor) Fill 100x100 trapezoid"] = 6;
	weight["(xor) 100-pixel solid circle"] = 6;
	weight["(xor) 10-pixel solid circle"] = 6;
	weight["(xor) 1-pixel line segment"] = 6;
	weight["(xor) 1-pixel line"] = 6;
	weight["(xor) 10x1 wide horizontal line segment"] = 6;
	weight["(xor) 10x1 wide vertical line segment"] = 6;
	weight["(xor) Copy 100x100 from pixmap to pixmap"] = 6;
	weight["(xor) 10-pixel dashed line"] = 6;
	weight["(xor) Char in 30-char line (TR 24)"] = 6;
	weight["(xor) Fill 100x100 equivalent triangle"] = 6;
	weight["(xor) Copy 10x10 from pixmap to window"] = 6;
	weight["(xor) Fill 10x10 trapezoid"] = 6;
	weight["(xor) Char in 70-char line (8x13)"] = 6;
	weight["(xor) 100-pixel circle"] = 6;
	weight["(xor) Copy 100x100 from window to pixmap"] = 6;
	weight["(xor) 10-pixel dashed segment"] = 5;
	weight["(xor) 10-pixel circle"] = 5;
	weight["(xor) 10x10 wide rectangle outline"] = 5;
	weight["(xor) 100x100 stippled rectangle"] = 5;
	weight["(xor) 100-pixel filled ellipse"] = 5;
	weight["(xor) 10x10 4x4 tiled rectangle"] = 5;
	weight["(xor) PutImage 10x10 square"] = 5;
	weight["(xor) Copy 500x500 from window to window"] = 5;
	weight["(xor) 500x500 wide rectangle outline"] = 5;
	weight["(xor) 10x10 stippled rectangle"] = 5;
	weight["(xor) 100x10 wide line"] = 5;
	weight["(xor) 100x10 wide horizontal line segment"] = 5;
	weight["(xor) 100x10 wide vertical line segment"] = 5;
	weight["(xor) Scroll 100x100 pixels"] = 5;
	weight["(xor) Char16 in 23-char line (k24)"] = 5;
	weight["(xor) Dot"] = 5;
	weight["(xor) Copy 500x500 1-bit deep plane"] = 5;
	weight["(xor) 100-pixel ellipse"] = 5;
	weight["(xor) 100x100 4x4 tiled rectangle"] = 5;
	weight["(xor) Copy 10x10 1-bit deep plane"] = 5;
	weight["(xor) 1x1 rectangle"] = 5;
	weight["(xor) 500-pixel solid circle"] = 4;
	weight["(xor) 500-pixel filled ellipse"] = 4;
	weight["(xor) 10-pixel filled ellipse"] = 4;
	weight["(xor) Fill 100x100 stippled trapezoid"] = 4;
	weight["(xor) 500x50 wide line"] = 4;
	weight["(xor) 500-pixel circle"] = 4;
	weight["(xor) 100-pixel double-dashed line"] = 4;
	weight["(xor) 500x50 wide horizontal line segment"] = 4;
	weight["(xor) 500x50 wide vertical line segment"] = 4;
	weight["(xor) 100-pixel dashed circle"] = 4;
	weight["(xor) 10x10 opaque stippled rectangle"] = 4;
	weight["(xor) 10-pixel ellipse"] = 4;
	weight["(xor) Copy 500x500 from pixmap to pixmap"] = 4;
	weight["(xor) 100x100 161x145 stippled rectangle"] = 4;
	weight["(xor) 10x10 161x145 tiled rectangle"] = 4;
	weight["(xor) Copy 10x10 from window to window"] = 4;
	weight["(xor) Copy 10x10 from pixmap to pixmap"] = 4;
	weight["(xor) 10x10 161x145 stippled rectangle"] = 4;
	weight["(xor) 100-pixel wide circle"] = 4;
	weight["(xor) 500x500 stippled rectangle"] = 4;
	weight["(xor) 500-pixel ellipse"] = 4;
	weight["(xor) 10x10 216x208 tiled rectangle"] = 4;
	weight["(xor) Fill 100x100 equivalent complex polygons"] = 4;
	weight["(xor) 100-pixel double-dashed segment"] = 3;
	weight["(xor) 10x10 161x145 opaque stippled rectangle"] = 3;
	weight["(xor) Fill 10x10 opaque stippled trapezoid"] = 3;
	weight["(xor) 100-pixel partial circle"] = 3;
	weight["(xor) Fill 10x10 161x145 stippled trapezoid"] = 3;
	weight["(xor) Fill 10x10 161x145 opaque stippled trapezoid"] = 3;
	weight["(xor) Scroll 500x500 pixels"] = 3;
	weight["(xor) 100x100 opaque stippled rectangle"] = 3;
	weight["(xor) Fill 10x10 stippled trapezoid"] = 3;
	weight["(xor) 100x10 wide dashed line"] = 3;
	weight["(xor) Copy 10x10 from window to pixmap"] = 3;
	weight["(xor) Copy 500x500 from window to pixmap"] = 3;
	weight["(xor) 500x500 opaque stippled rectangle"] = 3;
	weight["(xor) Fill 10x10 tiled trapezoid"] = 3;
	weight["(xor) 100x100 161x145 tiled rectangle"] = 3;
	weight["(xor) 100-pixel partial ellipse"] = 3;
	weight["(xor) 100-pixel fill slice partial circle"] = 3;
	weight["(xor) Fill 1x1 equivalent triangle"] = 3;
	weight["(xor) 100-pixel double-dashed circle"] = 3;
	weight["(xor) 500x500 4x4 tiled rectangle"] = 3;
	weight["(xor) 100-pixel wide ellipse"] = 3;
	weight["(xor) 100-pixel fill chord partial circle"] = 3;
	weight["(xor) 100x100 216x208 tiled rectangle"] = 3;
	weight["(xor) Fill 10x10 161x145 tiled trapezoid"] = 3;
	weight["(xor) Fill 100x100 216x208 tiled trapezoid"] = 3;
	weight["(xor) 500x500 161x145 stippled rectangle"] = 3;
	weight["(xor) 500x500 161x145 tiled rectangle"] = 3;
	weight["(xor) 100x100 161x145 opaque stippled rectangle"] = 3;
	weight["(xor) 500x500 161x145 opaque stippled rectangle"] = 3;
	weight["(xor) 10x1 wide line"] = 3;
	weight["(xor) 500x500 216x208 tiled rectangle"] = 3;
	weight["(xor) 100-pixel dashed ellipse"] = 3;
	weight["(xor) Fill 100x100 opaque stippled trapezoid"] = 3;
	weight["(xor) 10-pixel partial circle"] = 3;
	weight["(xor) 100x10 wide double-dashed line"] = 3;
	weight["(xor) Fill 100x100 161x145 stippled trapezoid"] = 3;
	weight["(xor) Fill 100x100 161x145 opaque stippled trapezoid"] = 3;
	weight["(xor) 100-pixel fill slice partial ellipse"] = 3;
	weight["(xor) 1-pixel circle"] = 3;
	weight["(xor) Fill 10x10 equivalent complex polygon"] = 3;
	weight["(xor) 100-pixel wide dashed circle"] = 2;
	weight["(xor) 100-pixel wide double-dashed circle"] = 2;
	weight["(xor) Scroll 10x10 pixels"] = 2;
	weight["(xor) 10-pixel wide circle"] = 2;
	weight["(xor) 100-pixel fill chord partial ellipse"] = 2;
	weight["(xor) Fill 100x100 tiled trapezoid"] = 2;
	weight["(xor) 100-pixel double-dashed ellipse"] = 2;
	weight["(xor) 100-pixel wide dashed ellipse"] = 2;
	weight["(xor) 100-pixel wide double-dashed ellipse"] = 2;
	weight["(xor) 10-pixel partial ellipse"] = 2;
	weight["(xor) 100-pixel wide partial circle"] = 2;
	weight["(xor) 100-pixel wide partial ellipse"] = 2;
	weight["(xor) 10-pixel fill slice partial circle"] = 2;
	weight["(xor) 10-pixel wide ellipse"] = 2;
	weight["(xor) Fill 100x100 161x145 tiled trapezoid"] = 2;
	weight["(xor) Fill 10x10 216x208 tiled trapezoid"] = 2;
	weight["(xor) 10-pixel fill chord partial circle"] = 2;
	weight["(xor) 500-pixel wide circle"] = 2;
	weight["(xor) 500-pixel wide ellipse"] = 2;
	weight["(xor) 1-pixel solid circle"] = 2;
	weight["(xor) 10-pixel fill chord partial ellipse"] = 2;
	weight["(xor) 10-pixel fill slice partial ellipse"] = 2;
	weight["(xor) 10-pixel wide partial circle"] = 1;
	weight["(xor) 10-pixel wide partial ellipse"] = 1;
	weight["(xor) 1x1 stippled rectangle"] = 1;
	weight["(xor) 1x1 161x145 stippled rectangle"] = 1;
	weight["(xor) 1x1 opaque stippled rectangle"] = 1;
	weight["(xor) 1x1 161x145 opaque stippled rectangle"] = 1;
	weight["(xor) 1x1 4x4 tiled rectangle"] = 1;
	weight["(xor) 1x1 161x145 tiled rectangle"] = 1;
	weight["(xor) 1x1 216x208 tiled rectangle"] = 1;
	weight["(xor) ShmPutImage 10x10 square"] = 0;
	weight["(xor) ShmPutImage 100x100 square"] = 0;
	weight["(xor) ShmPutImage 500x500 square"] = 0;
	sumofweights = 0;
    }
    $0 ~ /server/ {
	printf("name:%s\n",substr($0,1,index($0,"server")-2));
    }
    {
	split($0,parts,":");			# get rate and name
	start = index(parts[1],"(") + 1;	# find left parentheses
	end = index(parts[1],"/");		# find terminating '/'
	rate = substr(parts[1],start,end-start);# get ops/sec

	name = parts[2];
	while (substr(name,1,1) == " ") {	# remove leading spaces
	    name = substr(name,2,length(name));
	}

	thisweight = weight[name];
	weight[name] = 0;			# clear to avoid double counting
	sumofweights += thisweight;
	printf("%d:",thisweight);		# output in new format
	printf("%.1f\n",rate);
    }
    END{
	printf("sumof:%.1f\n",sumofweights);
    }
EOS

awk -f awkfile.$$ temp.$$ > rates.$$
rm -f awkfile.$$				# cleanup

# calculate the weighted average 

sumofweights=`grep sumof rates.$$ | awk -F: ' { print($2) }' - `
if [ "$sumofweights" != "4566.0" ]
then
    echo "ERROR: sum of weights =$sumofweights, not equal to 4566.0;"
    echo "ABORTING!"
    rm -f rates.$$ temp.$$
    exit 1
fi

awk -F: '
    BEGIN {
	logsum = 0;
	name = "tested"
    }
    $1 == "name" { name = $2;next }
    {
	weight = $1;
	rate = $2;
	if (rate > 0.0) {
	    # generate weighted log sum
	    logsum += ( log( rate ) * weight );
	}
    }
    END {
	SparcStation1 = 2118.51;
	WeightedAverage = exp(logsum/4566.0);
	printf("Weighted x11perf of %s server =%6.0f\n", name,WeightedAverage);
	printf("Weighted x11perf of SparcStation 1 server =%5.0f\n", SparcStation1);
	printf("Xmark =%8.4f\n", WeightedAverage/SparcStation1);
    }' rates.$$	

rm -f temp.$$ rates.$$ awkfile.$$			# cleanup

exit 0
