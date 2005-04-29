
/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  raid.c - Main functions for BootX.
 *
 *  Copyright (c) 1998-2005 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare (BootX), Soren Spies (RAID support)
 */


#include <sl.h>
#include <IOKit/IOKitKeys.h>

typedef unsigned long long  UInt64;
#define KERNEL
#include <IOKit/storage/RAID/AppleRAIDUserLib.h>   // hooray
//#include "/System/Library/Frameworks/IOKit.framework/Versions/A/PrivateHeaders/storage/RAID/AppleRAIDUserLib.h"

/* things we could probably get from elsewhere */
typedef enum RAIDType {
  kRAIDTypeUseless = -1,
  kRAIDTypeMirror,
  kRAIDTypeStripe,
  kRAIDTypeConcat,
  // kRAIDTypeFive,
} RAIDType;


/* stolen from AppleRAIDMember.h */

#define kAppleRAIDSignature "AppleRAIDHeader"
#define kAppleRAIDHeaderSize    0x1000

#define ARHEADER_OFFSET(s)  ((UInt64)(s) / kAppleRAIDHeaderSize * kAppleRAIDHeaderSize - kAppleRAIDHeaderSize )

struct AppleRAIDHeaderV2 {
    char    raidSignature[16];
    char    raidUUID[64];
    char    memberUUID[64];
    UInt64  size;
    char    plist[0];
};
typedef struct AppleRAIDHeaderV2 AppleRAIDHeaderV2;


/* our data structure */
typedef struct RAIDMember {
  // our providers
  char      path[256];   // sized like gBootDevice
  UInt64    size;        // becomes data-bearing portion once known
  union {  // switch on totMembers
    struct RAIDMember **members;  // dynamic array of pointers
                         // totMembers big; curMembers elements
    CICell    ih;        // after we Open() path; NULL if useless
  };


  // --- everything below here used for members; not leaves ---
  UInt64    curOffset;  // absolute for Seek/RAIDRead()
  int       curMembers; // updated as we add members and do I/O

  // derived from the header(s - struct and plist) stored in the providers
  char      setUUID[64]; // id used (w/seqNum) to find other members
  RAIDType  type;        // mirror, stripe, concat
  int       seqNum;      // member generation id
  int       totMembers;  // total members in set
  UInt64    chunkSize;   // stripe width
  UInt64    chunkCount;  // number of chunks (unused?)
} RAIDDevice;  // *RAIDDevicePtr;

/* for our paths */
#define kAppleRAIDOFPathPrefix "AppleRAID/"

/* -- static variables -- */
static int gMaxMembers;
static int gTotalMembers;
static struct RAIDMember *gMembers;  // an array of structs
static RAIDDevicePtr gRAIDMaster = NULL;

/* -- internal helper prototypes -- */
static long FillInLeaf(TagPtr tagElem, RAIDDevicePtr member);
static long AssimilateMember(RAIDDevicePtr member);
static RAIDDevicePtr DetermineParent(TagPtr candidateDict, AppleRAIDHeaderV2*);
static long AdoptChild(RAIDDevicePtr parent, RAIDDevicePtr child, TagPtr cDict);
static long isComplete(RAIDDevicePtr member);
static long NextPartition(char *loaderDev, char *memberDev);	// XX share?
// must call these with non-"-1" offsets
static long ReadMirror(RAIDDevicePtr raid, long buf, long nbytes,UInt64 offset);
static long ReadStripe(RAIDDevicePtr raid, long buf, long nbytes,UInt64 offset);
static long ReadConcat(RAIDDevicePtr raid, long buf, long nbytes,UInt64 offset);

/* -- accessors -- */
static RAIDType GetRAIDType(TagPtr dict);
static long long GetWholeNumber(TagPtr dict, char *key);

/* -- cruft XX :) -- */
static void problemCode();	// need to isolate crasher

/* -- public helpers -- */
int isRAIDPath(char *devSpec)
{
  int rval;

  rval = (devSpec && !strncmp(devSpec, kAppleRAIDOFPathPrefix,
      sizeof(kAppleRAIDOFPathPrefix)-1));
  return rval;
}

/* only publicly a RAID device if it has members */
/* we could have magic in our struct to be really sure :) */
int isRAIDDevice(void *ih)
{
  int rval;

  if(!ih)  return 0;

  RAIDDevicePtr p = (RAIDDevicePtr)ih;
  // is the pointer within range
  rval = (p >= &gMembers[0] && p < &gMembers[gTotalMembers] && p->curMembers);

  return rval;
}

// --- open/close/read/!/seek ---

// RAIDClose()
void RAIDClose(RAIDDevicePtr raid)
{
  int i = raid->totMembers;
  RAIDDevicePtr mem;

  if(raid->curMembers) {
    while(i-- > 0) {
      mem = raid->members[i];
      if(mem) {
	RAIDClose(mem);
	raid->members[i] = NULL;
	if(raid->curMembers)
	  raid->curMembers--;
      }
    }
  }
  else if(raid->ih)
    Close(raid->ih);
}

// RAIDOpen()
RAIDDevicePtr RAIDOpen(char *devSpec)
{
  // steal RAID "devices"
  int ridx;
  char *p = devSpec, *endp;
  RAIDDevicePtr rval = NULL;

  if(!devSpec)  return NULL;

  // walk past kAppleRAIDOFPathPrefix
  while (*p && *(p++) != '/');
  if(*p) {
    ridx = strtol(p, &endp, 10);
    if(ridx < gTotalMembers)
      rval = &gMembers[ridx];
  }

  return rval;
}

// RAIDSeek()
long RAIDSeek(RAIDDevicePtr raid, long long position)
{
  if(!raid)  return -1;
  if(position < 0)  return -1;

  raid->curOffset = position;
  return position;
}

// RAIDRead()
long RAIDRead(RAIDDevicePtr raid, long buf, long nbytes, long long offset)
{
  long rval = -1;
static long logs = 0;

  // only RAIDRead() supports offset == -1 -> curOffset
  if(offset == -1)
    offset = raid->curOffset;

  if(offset > raid->size) {
    printf("RAIDRead() error: offset (%x%x) > raid->size (%x%x)\n",
	offset, raid->size);
    return -1;
  }

  if(raid->totMembers) {
if(++logs < 15)
printf("RAIDRead(%x (curMembers: %d), %x, %d, %x%x)\n",
raid, raid->curMembers, buf, nbytes, offset);
    switch(raid->type) {
      case kRAIDTypeMirror:
	rval = ReadMirror(raid, buf, nbytes, offset);
	break;

      case kRAIDTypeStripe:
	rval = ReadStripe(raid, buf, nbytes, offset);
	break;

      case kRAIDTypeConcat:
	rval = ReadConcat(raid, buf, nbytes, offset);
	break;

      // case kRAIDTypeFive:
      default:
        printf("unsupported RAID type id: %d\n", raid->type);	// XX -> isComplete?
	rval = -1;
    }
  }
  else {  // leaf member, read from the CI; we open on demand
    do {  
      if(!raid->ih) {
	// open underlying device
	if(!(raid->ih = Open(raid->path)))  break;
      }
      if(Seek(raid->ih, offset) < 0)  break;
//if(logs < 25)
//printf("(RR): calling CI (%x from %x @ %x)\n", nbytes, raid->ih, (int)offset);
      rval = Read(raid->ih, buf, nbytes);
    } while(0);
  }

//if((raid->curMembers && logs < 15) || rval != nbytes)
//printf("RAIDRead() returning %d (vs. %d)\n", rval, nbytes);

  return rval;
}

// read-only, man
// ! long WriteToRAID(RAIDDevicePtr ... )


// simple for now (could spare out errors, etc)
static long ReadMirror(RAIDDevicePtr raid, long buf, long nbytes, UInt64 offset)
{
  int ridx = 0;

  // find a working member
  while(ridx < raid->totMembers) {
    if(raid->members[ridx])
      break;
    else
      ridx++;
  }

  if(ridx == raid->totMembers)
    return -1;

  return RAIDRead(raid->members[ridx], buf, nbytes, offset);
}

static long ReadStripe(RAIDDevicePtr raid, long buf, long nbytes, UInt64 offset)
{
  UInt64 chunkSize = raid->chunkSize;
  int nMems = raid->totMembers;
  UInt64 subChunkStart = offset % chunkSize;	// where in the final chunk?

  UInt64 chunkIdx = offset / chunkSize;		// which chunk of all of them?
  int midx = chunkIdx % nMems;			// on which member?
  UInt64 mChunk = chunkIdx / nMems;		// which chunk in the member?
  UInt64 mChunkOffset = mChunk * chunkSize;	// which offset in the member?

  long thisTime, totalRead = 0;
  long bytesRead, bytesLeft = nbytes;

  long bufp = buf;

  if(subChunkStart + nbytes > chunkSize)
    thisTime = chunkSize - subChunkStart;
  else
    thisTime = nbytes;	// nbytes is within the chunk

  // tempting to do a partial read here ...

  while(bytesLeft /* ALT: bufp < buf + nbytes */) {
    bytesRead = RAIDRead(raid->members[midx], bufp, thisTime, mChunkOffset+subChunkStart);
    if(bytesRead == -1)  goto fail;
    totalRead += bytesRead;		// record what we read
    bytesLeft -= bytesRead;
    if(bytesRead != thisTime)  break; 	// detect partial read and return

    // set up for next read

    // effect "chunkIdx++" by walking across the members
    midx++;				// next member
    if(midx % nMems == 0) {		// did we wrap?
      midx = 0;				// back to 0th member
      mChunkOffset += chunkSize;	// next chunk ("mChunk++")
    }

    // adjust other parameters
    bufp += bytesRead;			// bytesRead == thisTime
    if(bytesLeft > chunkSize)		// are we in the last chunk yet?
      thisTime = chunkSize;
    else
      thisTime = bytesLeft;
    subChunkStart = 0;			// reset after first read (& beyond :P)
  }

  return totalRead;

fail:
  totalRead = -1;
  return totalRead;
}

// XX okay to compare UInt64 values to -1?
static long ReadConcat(RAIDDevicePtr raid, long buf, long nbytes, UInt64 offset)
{
//static UInt64 predoffset = -1;
  int midx;
  UInt64 nextStart = 0, curStart = 0;
  long totalRead = 0, bytesRead;

  // assume curMembers!=0 since RAIDRead() should be our only caller
  for(midx = 0; midx < raid->curMembers; midx++) {
    nextStart = curStart + raid->members[midx]->size;

    // are we in range yet?
    if(offset < nextStart) {
      long thisTime;
      UInt64 curOffset = offset - curStart;

      // make sure we aren't reading out too far
      if(offset + nbytes < nextStart)
	thisTime = nbytes;
      else
	thisTime = nextStart - offset;	// distance to the end

      bytesRead = RAIDRead(raid->members[midx],buf,thisTime,curOffset);
      if(bytesRead == -1)  goto fail;
      totalRead += bytesRead;		 // record what was read
      if(bytesRead != thisTime)  break;	 // detect partial read and return

      // XX need to lay out a volume to test crossing boundary in one read
      if(thisTime != nbytes) {
printf("ReadConcat crossing a boundary in a single read\n");
	offset = nextStart;
	nbytes -= thisTime;
	buf += thisTime;
	continue;	// take another spin around the loop
      }
      break;	// exit from the enclosing loop
    }
    curStart = nextStart;
  }

  return totalRead;

fail:
printf("ReadConcat: %d for %x%x (ended %x%x..%x%x)\n",
totalRead, offset, curStart, nextStart);
  totalRead = -1;
  return totalRead;
}


// --- seek out RAID members and build sets ---

/* 
  How we find and build RAID sets (not our first stab :):
  if(have Boot.plist)
    1) initialize with the OF paths in Boot.plist (Tiger)
  else
    *) there is no else until we find a way to divine partition sizes

  while(unread potential members)
    3) read new members and populate structs
    4) assimilate them ("resistance is futile!")
       * DetermineParent() looks through existing sets and creates on demand
       * give the child to the parent for adoption
       * if the parent becomes complete, proactively probe it as a member
*/

// process gBootDict looking for path/size pairs where we can sniff for RAID
// allocate gMembers (3x the number of members)
// make some stub members pointing to these paths
long LookForRAID(TagPtr bootDict)
{
  TagPtr partSpec;
int nsuccesses = 0;

  do {
    // count and allocate gMembers array
    // assume no Apple_RAID could contribute > 2 members
    if(bootDict->type == kTagTypeArray)
      for(partSpec = bootDict->tag; partSpec; partSpec = partSpec->tagNext)
	gMaxMembers += 3;	// XX [re]think degraded mirrors trying to stack
    else
      gMaxMembers = 3;

    gMembers = AllocateBootXMemory(gMaxMembers*sizeof(RAIDDevice));
    if(!gMembers)  break;
    bzero(gMembers, gMaxMembers*sizeof(RAIDDevice));

    // walk list of potential members, skipping on error
    //   FillInLeaf() on each
    //   AssimilateMember() on each
    if(bootDict->type == kTagTypeArray) {
      char masterMemberPath[256];

      if(NextPartition(gBootDevice, masterMemberPath))  break;

      for(partSpec = bootDict->tag; partSpec; partSpec = partSpec->tagNext) {
	RAIDDevicePtr newLeaf = &gMembers[gTotalMembers++];  // burn entries
	if(FillInLeaf(partSpec, newLeaf))  continue;

	// is this the master (from whose Apple_Boot we loaded?
        if(FindDevice(newLeaf->path) == FindDevice(masterMemberPath)) {
printf("raid: found member affiliated with our boot device\n");
          gRAIDMaster = newLeaf;
       }

	// assertion: leaves are always complete
printf("raid: assimilating leaf %x\n", newLeaf);
	if(AssimilateMember(newLeaf))  continue;
nsuccesses++;
      }
    }
    // even degraded mirror (one member) is in an array
    else if(bootDict->type == kTagTypeDict) {
      if(FillInLeaf(bootDict, &gMembers[0]))  break;
      if(AssimilateMember(&gMembers[0]))  break;
      gRAIDMaster = &gMembers[1];
    }
    else 
      break;

printf("LookForRAID() made %d happy structs ", nsuccesses);
printf("which resulted in %d total objects\n", gTotalMembers);

    if(isComplete(gRAIDMaster)) {
      int masteridx = gRAIDMaster - gMembers;
      sprintf(gBootDevice, "%s%d:0,\\\\tbxi",kAppleRAIDOFPathPrefix,masteridx);
printf("raid: set gBootDevice to %s\n", gBootDevice);
    } else {
      if(!gRAIDMaster)
	printf("raid: boot-device didn't lead to a RAID device\n");
      else
	printf("raid: master RAID device %x missing members (has %d of %d)\n",
	    gRAIDMaster, gRAIDMaster->curMembers, gRAIDMaster->totMembers);
      break;
    }

    return 0;
  } while(0);

  return -1;
}

// FillInLeaf() fills in a member's struct so the partition can later be probed
// (just need the path and size)
static long FillInLeaf(TagPtr partSpec, RAIDDevicePtr newMember)
{
  TagPtr prop;
  long long size;
  char *path;
  long rval = -1;

  do {
    prop = GetProperty(partSpec, kIOBootDevicePathKey);
    if(!prop || prop->type != kTagTypeString)  break;
    path = prop->string + sizeof(kIODeviceTreePlane);  // +1(':') -1('\0')
    if((size = GetWholeNumber(partSpec, kIOBootDeviceSizeKey)) < 0)  break;

//printf("got path and size (%x%x)\n", size);

    // and copy in the values
    if(!strcpy(newMember->path, path))  break;
    newMember->size = size;

    // gMembers got the big bzero() so all else is fine

    rval = 0;
  } while(0);

  return rval;
}
 
// need to extract logical bits so we can find a parent
static long AssimilateMember(RAIDDevicePtr candidate)
{
  TagPtr candDict = NULL;
  void *bufp = (char*)kLoadAddr;
  AppleRAIDHeaderV2 *cHeader = (AppleRAIDHeaderV2*)bufp;
  RAIDDevicePtr parent = NULL;
  long rval = -1;

  do {
    // suck headers from candidate
    if(RAIDRead(candidate, (long)bufp, kAppleRAIDHeaderSize,
	ARHEADER_OFFSET(candidate->size)) != kAppleRAIDHeaderSize)  break;
printf("raid: checking for magic: %s\n", bufp);  // should point to magic

    // validate the magic
    if(strcmp(kAppleRAIDSignature, bufp)) {
      printf("RAID magic not found in member %x\n", candidate);
      break;
    }
//printf("raid set UUID: %s\n", cHeader->raidUUID);
//printf("memberUUID: %s\n", cHeader->memberUUID);

    // batten down the size to hide the header
printf("raid: member size (%x%x) -> data size (%x%x)\n",candidate->size,
cHeader->size);
    candidate->size = cHeader->size;	// -msoft-flaot required!

    // and skip to and parse the embedded plist (to get the type, etc)
    bufp += sizeof(AppleRAIDHeaderV2);
//printf("bufp: %s\n", bufp);
    if((ParseXML(bufp, &candDict) < 0) || !candDict)  break;
//printf("parsed RAID member dict (%x):\n", candDict);
//DumpTag(candDict, 0);
//printf("\n");

    // DetermineParent will create a parent if it didn't already exist
    if(!(parent = DetermineParent(candDict, cHeader)))  break;
printf("raid: %x being given to %x for adoption\n", candidate, parent);
    if(AdoptChild(parent, candidate, candDict))  break;

printf("raid: checking if set is ready to be probed...\n");
    if(isComplete(parent))
      (void)AssimilateMember(parent);  // we might be done ...

    rval = 0;
  } while(0);

  return rval;
}

static long AdoptChild(RAIDDevicePtr parent, RAIDDevicePtr child, TagPtr cDict)
{
  int candIdx, childSeq;
  long rval = -1;

  do {
    if((childSeq = GetWholeNumber(cDict, kAppleRAIDSequenceNumberKey)) < 0)
	break;

    // check for "one"ness early (the child might be otherwise unwanted)
    if(child == gRAIDMaster) {
printf("raid: SUCCESSION %x was master; now to parent %x\n", child, parent);
      gRAIDMaster = parent;
    }

    // do per-type actions (e.g. adjust size, cast out degraded mirrors, etc)
    switch(parent->type) {
      case kRAIDTypeMirror:
      // only want the most up to date n-way twins (n=1..k)
      if(childSeq < parent->seqNum) {
printf("child seq # (%d) < parent's (%d) skip\n",childSeq,parent->seqNum);
	return 0;
      }
      if(childSeq > parent->seqNum) {
	int midx;
	// more recent; take the new seqNum and cast out old members
printf("child seq (%d) > parent's (%d) update\n",childSeq,parent->seqNum);
        parent->seqNum = childSeq;
	for(midx = 0; midx < parent->totMembers; midx++)
	  parent->members[midx] = NULL;
	parent->curMembers = 0;
      }
      parent->size = child->size;

      break;

      case kRAIDTypeStripe:
printf("st parent->size(%x%x) += child->size(%x%x)\n",parent->size,child->size);
      parent->size += child->size;
      /* could validate that sizes/#totMembers match */

      break;

      // yes, concat and stripe are the same for now
      case kRAIDTypeConcat:
      parent->size += child->size;

      break;

      case kRAIDTypeUseless: return -1;  // yuck
    }

    // get the index of this member in the set
    if((candIdx = GetWholeNumber(cDict, kAppleRAIDMemberIndexKey)) == -1) break;

    // just in case we find the same member twice?
    // invariant: curMembers == PopCount(members)
    if(!parent->members[candIdx])
      parent->curMembers++;
    else
printf("raid: members[%d] non-NULL: %x\n", candIdx, parent->members[candIdx]);

printf("raid: assigning %x to parent->members[%d]\n", child, candIdx);
    parent->members[candIdx] = child;

    rval = 0;
  } while(0);

  return rval;
}

// aka "MakeNewParent()" if parent doesn't exist
static RAIDDevicePtr DetermineParent(TagPtr cDict, AppleRAIDHeaderV2 *cHeader)
{
  int ridx;
  RAIDDevicePtr rval = NULL;
  int seqNum = GetWholeNumber(cDict, kAppleRAIDSequenceNumberKey);
  int candIdx = GetWholeNumber(cDict, kAppleRAIDMemberIndexKey);

  if(seqNum == -1 || candIdx == -1)  return rval;

  // search all non-leaf parents for a potential match
  for(ridx = 0; ridx < gTotalMembers; ridx++) {
    RAIDDevicePtr potential = &gMembers[ridx];
    if(!potential->totMembers)  continue;  // not interested in children
    if(potential->totMembers <= candIdx)  continue;  // no slot for child
    if(potential->type != kRAIDTypeMirror) // mirrors will consider any child
      if(potential->seqNum != seqNum)  continue;  // wrong generation

    // XX could check for UUID in members-UUID array

    // if the IDs match, hooray
    if(!strcmp(cHeader->raidUUID, potential->setUUID)) {
printf("raid: matched a child to a parent in %s\n", cHeader->raidUUID);
      rval = potential;
      break;
    }
  }


  // If we didn't find a parent, make one
  if(ridx == gTotalMembers)
    // get values, minimally validate, and populate new parent
    do {
      RAIDDevicePtr parent = &gMembers[gTotalMembers++];
      int totMembers = 0;
      RAIDType type;
      TagPtr prop;
      UInt64 chunkSize = 0;
      UInt64 chunkCount = 0;

printf("raid: making new parent %x\n", parent);
      // count up totMembers
      prop = GetProperty(cDict, kAppleRAIDMembersKey);
      if(!prop || prop->type != kTagTypeArray)  break;
//printf("cDict->raidMembers:\n");
//DumpTag(prop, 0);
//printf("\n");

      // count elements of the array
      prop = prop->tag;
      while(prop) {
	totMembers++;
	prop = prop->tagNext;
      }
printf("raid: parent->totMembers: %d\n", totMembers);
      if(totMembers <= candIdx) {
printf("raid: candidate claims index %d in a %d-member set; ignoring\n",
candIdx, totMembers);
        break;
      }

      // grab type, chunk info from child dict (sequence # above)
      type = GetRAIDType(cDict);
      if(type == kRAIDTypeUseless)  break;
      if(type == kRAIDTypeStripe) {
	chunkSize = GetWholeNumber(cDict, kAppleRAIDChunkSizeKey);
	if(chunkSize == -1)  break;
	chunkCount = GetWholeNumber(cDict, kAppleRAIDChunkCountKey);
	if(chunkCount == -1)  break;
      }

      // and set up the parent structure's values
      parent->size = 0;     	// let AssimilateMember increment the size

      parent->members = AllocateBootXMemory(totMembers*sizeof(RAIDDevicePtr));
      if(!parent->members)  break;
      bzero(parent->members, totMembers*sizeof(RAIDDevicePtr));

      // curMembers, curOffset == 0 from initial bzero()

      if(!strcpy(parent->setUUID, cHeader->raidUUID))  break;
      parent->type = type;
      parent->seqNum = seqNum;
      parent->totMembers = totMembers;
      if(type == kRAIDTypeStripe) {
	parent->chunkSize = chunkSize;
	parent->chunkCount = chunkCount;
//printf("stripe: size * count %x%x vs. size %x%x\n", chunkSize * chunkCount, cHeader->size);
      }

      rval = parent;
    } while(0);

  return rval;
}

static long NextPartition(char *loaderDev, char *memberDev)
{
  int cnt, cnt2, partNum;
  // stolen from main.c

  // Find the first ':'.
  cnt = 0;
  while ((loaderDev[cnt] != '\0') && (loaderDev[cnt] != ':')) cnt++;
  if (loaderDev[cnt] == '\0') return -1;
  
  // Find the comma after the ':'.
  cnt2 = cnt + 1;
  while ((loaderDev[cnt2]  != '\0') && (loaderDev[cnt] != ',')) cnt2++;
  
  // Get just the partition number
  strncpy(memberDev, loaderDev + cnt + 1, cnt2 - cnt - 1);
  partNum = strtol(memberDev, 0, 10);
  if (partNum == 0) partNum = strtol(gBootFile, 0, 16);
  
  // looking for the Apple_RAID following the Apple_Boot
  partNum++;
  
  // Construct the boot-file
  strncpy(memberDev, loaderDev, cnt + 1);
  sprintf(memberDev + cnt + 1, "%d", partNum);

  return 0;
}

static long isComplete(RAIDDevicePtr member)
{
  int rval = 0;

  do {
    if(!member || !member->curMembers)  break;

    switch(member->type) {
      case kRAIDTypeMirror:
	rval = (member->curMembers > 0);
	break;

      case kRAIDTypeStripe:
      case kRAIDTypeConcat:
	rval = (member->curMembers == member->totMembers);

      default:
	break;
    }
  } while(0);

printf("raid: isComplete on %x (type %d, #mems %d): %d\n",
member, member->type, member->curMembers, rval);
  return rval;
}


// --- dictionary accessors ---
static RAIDType GetRAIDType(TagPtr dict)
{
  RAIDType type = kRAIDTypeUseless;
  TagPtr prop;
  char *typeString;

  do {
    prop = GetProperty(dict, kAppleRAIDLevelNameKey);
    if(!prop || prop->type != kTagTypeString)  break;
    typeString = prop->string;
    switch(typeString[0]) {
      case 'M': type = kRAIDTypeMirror; break;
      case 'S': type = kRAIDTypeStripe; break;
      case 'C': type = kRAIDTypeConcat; break;
      default:
printf("raid: didn't like level: %s\n", typeString);
	type = kRAIDTypeUseless;
    }
  } while(0);

  return type;
}

// "whole numbers are the counting numbers plus zero"  - math team coach
// -1 indicates an error
static long long GetWholeNumber(TagPtr dict, char *key)
{
  long long num = -1;
  char *endp;
  TagPtr prop;

  do {
    prop = GetProperty(dict, key);
    if(!prop || prop->type != kTagTypeInteger)  break;
    num = strtouq(prop->string, &endp, 0);   // as long as not > #mems :)
    if(*endp != '\0') {
      num = -1;
    }
  } while(0);

  return num;
}


// -- cruft --
static void problemCode()
{
  RAIDDevice rmem, *tmem = &rmem;
  AppleRAIDHeaderV2 rheader, *theader = &rheader;
  void *fp = &problemCode;

printf("\n ... calling probleCode() ...\n");
  fp += 1;
printf("populating a size member\n");
  theader->size = 123456789;
//printf("trying to copy tmem->size = theader->size");
  tmem->size = theader->size;
printf("dumping size: %x,%x\n", (int)(tmem->size>>32), (int)tmem);
}
