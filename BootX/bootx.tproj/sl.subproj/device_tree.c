/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 *  device_tree.c - Functions for flattening the Device Tree.
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <sl.h>
#include <device_tree.h>


static long FlatenNode(CICell ph, long nodeAddr, long *nodeSize);
static long FlatenProps(CICell ph, long propAddr, long *propSize,
			long *numProps);

// Public Functions

long FlattenDeviceTree(void)
{
  CICell RootPH;
  long   ret;
  
  gDeviceTreeAddr = AllocateKernelMemory(0);
  
  RootPH = Peer(0);
  if (RootPH == kCIError) return -1;
  
  ret = FlatenNode(RootPH, gDeviceTreeAddr, &gDeviceTreeSize);
  
  AllocateKernelMemory(gDeviceTreeSize);
  
  return ret;
}


CICell SearchForNode(CICell ph, long top, char *prop, char *value)
{
  CICell curChild, result;
  
  if (ph == 0) ph = Peer(0);
  
  if (top == 0) {
    // Look for it in the current node.
    if (GetProp(ph, prop, gTempStr, 4095) != -1) {
      if (strcmp(value, gTempStr) == 0) {
	return ph;
      }
    }
  }
  
  // Look for it in the children.
  curChild = Child(ph);
  
  while (curChild != 0) {
    result = SearchForNode(curChild, 0, prop, value);
    if (result != 0) return result;
    curChild = Peer(curChild);
  }
  
  if (top != 0) {
    while (ph != 0) {
      curChild = Peer(ph);
      while (curChild != 0) {
	result = SearchForNode(curChild, 0, prop, value);
	if (result != 0) return result;
	curChild = Peer(curChild);
      }
      
      ph = Parent(ph);
    }
  }
  
  return 0;
}


CICell SearchForNodeMatching(CICell ph, long top, char *value)
{
  CICell curChild, result;
  
  if (ph == 0) ph = Peer(0);
  
  if (top == 0) {
    // Look for it in the current node.
    if (MatchThis(ph, value) == 0) return ph;
  }
  
  // Look for it in the children.
  curChild = Child(ph);
  
  while (curChild != 0) {
    result = SearchForNodeMatching(curChild, 0, value);
    if (result != 0) return result;
    curChild = Peer(curChild);
  }
  
  if (top != 0) {
    while (ph != 0) {
      curChild = Peer(ph);
      while (curChild != 0) {
	result = SearchForNodeMatching(curChild, 0, value);
	if (result != 0) return result;
	curChild = Peer(curChild);
      }
      
      ph = Parent(ph);
    }
  }
  
  return 0;
}

// Private Functions

long FlatenNode(CICell ph, long nodeAddr, long *nodeSize)
{
  DTNodePtr node;
  CICell    childPH;
  long      curAddr, ret;
  long      propSize, numProps, childSize, numChildren;

  node = (DTNodePtr)nodeAddr;
  curAddr = nodeAddr + sizeof(DTNode);
  numProps = 0;
  numChildren = 0;
  
  ret = FlatenProps(ph, curAddr, &propSize, &numProps);
  if (ret != 0) return ret;

  curAddr += propSize;
  node->nProperties = numProps;

  childPH = Child(ph);
  if (childPH == kCIError) return -1;

  while (childPH != 0) {
    ret = FlatenNode(childPH, curAddr, &childSize);
    curAddr += childSize;
    numChildren++;

    childPH = Peer(childPH);
    if (childPH == -1) return -1;
  }

  node->nChildren = numChildren;
  *nodeSize = curAddr - nodeAddr;
  
  return 0;
}


long FlatenProps(CICell ph, long propAddr, long *propSize, long *numProps)
{
  DTPropertyPtr prop;
  long          ret, cnt, curAddr, valueAddr, valueSize, nProps;
  char          *prevName;
  
  curAddr = propAddr;
  prevName = "";
  nProps = 0;
  
  // make the first property the phandle
  prop = (DTPropertyPtr)curAddr;
  valueAddr = curAddr + sizeof(DTProperty);
  strcpy(prop->name, "AAPL,phandle");
  *((long *)valueAddr) = ph;
  prop->length = 4;
  curAddr = valueAddr + 4;
  nProps++;
  
  // Make the AAPL,unit-string property.
  ret = PackageToPath(ph, gTempStr, 4095);
  if (ret > 0) {
    cnt = ret - 1;
    while (cnt && (gTempStr[cnt - 1] != '@') &&
	   (gTempStr[cnt - 1] != '/')) cnt--;
    
    if (gTempStr[cnt - 1] == '@') {
      prop = (DTPropertyPtr)curAddr;
      valueAddr = curAddr + sizeof(DTProperty);
      strcpy(prop->name, "AAPL,unit-string");
      strcpy((char *)valueAddr, &gTempStr[cnt]);
      prop->length = ret - cnt;
      curAddr = valueAddr + ((prop->length + 3) & ~3);
      nProps++;
    }
  }
  
  while (1) {
    prop = (DTPropertyPtr)curAddr;
    valueAddr = curAddr + sizeof(DTProperty);
    
    ret = NextProp(ph, prevName, prop->name);
    if (ret == -1) return -1;
    if (ret == 0) break;
    
    valueSize = GetProp(ph, prop->name, (char *)valueAddr,
			kPropValueMaxLength);
    if (valueSize == -1) return -1;
    prop->length = valueSize;
    
    // Save the address of the value if this is
    // the memory map property for the device tree.
    if ((ph == gMemoryMapPH) && !strcmp(prop->name, "DeviceTree")) {
      gDeviceTreeMMTmp = (long *)valueAddr;
    }
    
    nProps++;
    curAddr = valueAddr + ((valueSize + 3) & ~3);
    prevName = prop->name;
  };
  
  *numProps = nProps;
  *propSize = curAddr - propAddr;
  
  return 0;
}
