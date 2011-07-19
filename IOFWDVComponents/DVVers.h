/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
	File:		DVVers.h

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Written by:	George D. Wilson Jr.

	Copyright:	© 1998-2000 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				xxx put dri here xxx

		Other Contact:		xxx put other contact here xxx

		Technology:			xxx put technology here xxx

	Writers:

		(GDW)	George D. Wilson Jr.
		(RS)	Richard Sepulveda
		(jkl)	Jay Lloyd
		(KW)	Kevin Williams
		(CK)	Casey King
		(SS)	Steve Smith

	Change History (most recent first):

	 <DV115>	 2/28/00	CK		Bump to 2.2d3.
	 <DV114>	 2/15/00	CK		bump revision.
	 <DV113>	  2/3/00	CK		Bump version to 2.2d1.
	 <DV112>	  2/2/00	CK		bump version
	 <DV111>	11/10/99	GDW		2.1.2a7e1
	 <DV110>	 11/3/99	jkl		2.1.2a6e2
	 <DV109>	10/29/99	jkl		2.1.1a6e2
	 <DV108>	10/11/99	GDW		2.1.1a4
	 <DV107>	 10/4/99	GDW		2.1.1a3 build.
	 <DV106>	 9/22/99	jkl		2.1.1a2e1
	   <105>	 8/31/99	GDW		Start the next one.
	   <104>	 8/31/99	GDW		Start the next one.
	   <103>	 8/30/99	jkl		2.1.1a1
	   <102>	 8/26/99	jkl		d17e1
	   <101>	 8/24/99	GDW		Bumped rev for gamma changes.
	   <100>	 8/24/99	jkl		d15e2
		<99>	 8/20/99	jkl		d15e1
		<98>	 8/19/99	jkl		d14e3
		<97>	 8/19/99	jkl		d14e3
		<96>	 8/18/99	jkl		d14e2
		<95>	 8/18/99	jkl		d14e1
		<94>	 8/10/99	RS		d13e2
		<93>	 8/10/99	RS		d13e1
		<92>	  8/6/99	jkl		d12e3
		<91>	  8/4/99	jkl		d12e2
		<90>	  8/4/99	jkl		d12e1
		<89>	 7/27/99	jkl		d11e1
		<88>	 7/21/99	jkl		2.1d10e1
		<87>	 7/19/99	jkl		2.1d9e3, e2 was a ghost.
		<86>	 7/14/99	jkl		2.1d9e1
		<85>	  7/7/99	jkl		2.1d8e1
		<84>	  7/1/99	jkl		2.1d7e2
		<83>	  7/1/99	jkl		2.1d7e1
		<82>	 6/25/99	jkl		2.1d6e3
		<81>	 6/25/99	jkl		2.1d6e2
		<80>	 6/24/99	jkl		Bump 2.1d6e1.
		<79>	 6/21/99	jkl		Version 2.1d5e2.
		<78>	 6/17/99	jkl		Version 2.1d5e1.
		<77>	  6/8/99	jkl		Bumped version.
		<76>	  6/2/99	jkl		Bumped version to d3.
		<75>	 5/21/99	jkl		Updated version for build.
		<74>	 4/26/99	ck		Change to v2.1
		<73>	 3/22/99	ck		Version should really be 17e2, so make it so.
		<72>	 3/22/99	jkl		Bump to d18e1.
		<71>	 3/17/99	KW		b17e1 VDIG now examines all sync blocks when looking for
									timecode.
		<70>	  3/8/99	ck		event recording got left on.
		<69>	  3/8/99	ck		Kevin found a significant bug in idlemux.
		<68>	  3/8/99	ck		Turn symbols off for final build(s).
		<67>	  3/7/99	ck		Do the build Sunday.
		<66>	  3/1/99	ck		Another try, deep icons and dvdrvr version change.
		<65>	 2/28/99	ck		b14 ... final candidate
		<64>	 2/25/99	ck		b13e2 was the fixed Yak 16583 (long duration frames to Canvas).
									b13e3 finishes audio only muxing by fabricating video frames.
		<63>	 2/22/99	ck		b13.
		<62>	 2/14/99	ck		b12
		<61>	  2/5/99	ck		Do the build early this week.
		<60>	  2/1/99	ck		Another try.
		<59>	  2/1/99	ck		b10.
		<58>	 1/25/99	ck		b9.
		<57>	 1/18/99	ck		b8.
		<56>	 1/15/99	ck		Made the numeric version 1.1.0 instead of 1.0.1. Thanks Duano!
		<55>	 1/11/99	ck		b7 and add 1999.
		<54>	12/23/98	CK		go to b6 for macworld
		<53>	12/22/98	ck		For some reason QT bumped to B5, so we will too. This build
									would correspond to b4e3.
		<52>	12/20/98	ck		Do the weekly build early.
		<51>	12/17/98	jkl		Version to get top of trunk to Ralph since we are all confused
									about what is in what build.
		<50>	12/14/98	ck		b3e2 build
		<49>	12/14/98	ck		time for b3
		<48>	 12/9/98	ck		Had an extra character in there.
		<47>	 12/9/98	ck		A second e build today to fix some fw clock issues.
		<46>	 12/9/98	ck		Beta at last.
		<45>	 12/7/98	ck		Still keeping the alpha build descripter til QT goes Beta for
									real!
		<44>	11/30/98	ck		a9
		<43>	11/25/98	ck		3 builds a week or we're not working hard enough.
		<42>	11/23/98	ck		yet another build.
		<41>	11/23/98	ck		Another Monday, another build. When will we bump this to Beta??
		<40>	11/19/98	ck		A test version for FCP, to check audio latency bug.
		<39>	11/17/98	ck		Why can't we ever get anything right?
		<38>	11/16/98	ck		New release, a7 or is it b2??
		<37>	11/11/98	ck		Another intermediate build to fix scratchy audio.
		<36>	 11/9/98	ck		Another try.
		<35>	 11/9/98	ck		Getting ready for weekly build.
		<34>	 11/6/98	SS		a5e2 Yak build for the testers.
		<33>	10/30/98	ck		A test, no change.
		<32>	10/30/98	KW		Bump to next rev a5.
		<31>	10/26/98	CK		Time to go to 1.1.a4
		<30>	10/22/98	CK		intermediate build for "Yak", Labeled as a3e2.
		<29>	10/15/98	GDW		We are moving to alpha.
		<28>	 10/5/98	CK		Another engr build
		<27>	 9/29/98	GDW		...
		<26>	 9/24/98	GDW		Engineering release of d15.
		<25>	 9/18/98	GDW		New release.
		<24>	 8/26/98	GDW		Incr.
		<23>	 8/11/98	GDW		New release.
		<22>	 7/31/97	GDW		Guess again.
		<21>	 7/23/98	GDW		1.1d9.
		<20>	 7/13/97	GDW		1.1d8
		<19>	  7/6/98	GDW		Guess?
		<18>	 6/27/98	GDW		New one
		<17>	 6/17/98	GDW		New one
		<16>	 6/16/98	GDW		We are at d4.
		<14>	 5/27/98	GDW		New dev version.
		<13>	 5/21/98	GDW		First 1.1 development revision.
		<12>	  4/9/98	GDW		Bump.
		<11>	  4/3/98	GDW		Bumped rev for final candidate.
		<10>	  4/2/98	GDW		One more bump.
		 <9>	  4/1/98	GDW		For stopusing selector.
		 <8>	 3/30/98	GDW		New rev.
		 <7>	 3/30/98	GDW		New one.
		 <6>	 3/25/98	GDW		Guess?
		 <5>	 3/24/98	GDW		Next Beta bump.
		 <4>	 3/17/98	GDW		Another early mornin build!
		 <3>	 3/16/98	GDW		Gone beta!
		 <2>	 3/10/98	SS		bumped version to a9e1.
		 <1>	  3/3/98	GDW		first checked in

	To Do:
*/

#define DVVersion 						4
#define DVRevision 						0
#define DVBuildStage 					developStage
#define DVBuildNumber 					5		// every time this file is changed, increment this
#define DVShortVersionString 			"4.0d3e1"
#define DVLongVersionString 			"4.0d3e1, © Apple Computer, Inc. 1998-2001"
