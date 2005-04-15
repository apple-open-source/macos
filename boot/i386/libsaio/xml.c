/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 2003 Apple Computer, Inc.  All Rights
 * Reserved.  
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
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

#include "bootstruct.h"
#include "libsaio.h"
#include "sl.h"
#include "xml.h"

struct Module {  
  struct Module *nextModule;
  long          willLoad;
  TagPtr        dict;
  char          *plistAddr;
  long          plistLength;
  char          *driverPath;
};
typedef struct Module Module, *ModulePtr;

struct DriverInfo {
  char *plistAddr;
  long plistLength;
  void *moduleAddr;
  long moduleLength;
};
typedef struct DriverInfo DriverInfo, *DriverInfoPtr;

#define kDriverPackageSignature1 'MKXT'
#define kDriverPackageSignature2 'MOSX'

struct DriversPackage {
  unsigned long signature1;
  unsigned long signature2;
  unsigned long length;
  unsigned long alder32;
  unsigned long version;
  unsigned long numDrivers;
  unsigned long reserved1;
  unsigned long reserved2;
};
typedef struct DriversPackage DriversPackage;

enum {
  kCFBundleType2,
  kCFBundleType3
};


#define USEMALLOC 1
#define DOFREE 1

static long ParseTagList(char *buffer, TagPtr *tag, long type, long empty);
static long ParseTagKey(char *buffer, TagPtr *tag);
static long ParseTagString(char *buffer, TagPtr *tag);
static long ParseTagInteger(char *buffer, TagPtr *tag);
static long ParseTagData(char *buffer, TagPtr *tag);
static long ParseTagDate(char *buffer, TagPtr *tag);
static long ParseTagBoolean(char *buffer, TagPtr *tag, long type);
static long GetNextTag(char *buffer, char **tag, long *start);
static long FixDataMatchingTag(char *buffer, char *tag);
static TagPtr NewTag(void);
static char *NewSymbol(char *string);
#if DOFREE
static void FreeSymbol(char *string);
#endif


//==========================================================================
// XMLGetProperty

TagPtr
XMLGetProperty( TagPtr dict, const char * key )
{
    TagPtr tagList, tag;

    if (dict->type != kTagTypeDict) return 0;
    
    tag = 0;
    tagList = dict->tag;
    while (tagList)
    {
        tag = tagList;
        tagList = tag->tagNext;
        
        if ((tag->type != kTagTypeKey) || (tag->string == 0)) continue;
        
        if (!strcmp(tag->string, key)) return tag->tag;
    }
    
    return 0;
}


//==========================================================================
// XMLParseFile
// Expects to see one dictionary in the XML file.
// Puts the first dictionary it finds in the
// tag pointer and returns 0, or returns -1 if not found.
//
long
XMLParseFile( char * buffer, TagPtr * dict )
{
    long       length, pos;
    TagPtr     tag;
    pos = 0;
  
    while (1)
    {
        length = XMLParseNextTag(buffer + pos, &tag);
        if (length == -1) break;
    
        pos += length;
    
        if (tag == 0) continue;
        if (tag->type == kTagTypeDict) break;
    
        XMLFreeTag(tag);
    }
    if (length < 0) {
        return -1;
    }
    *dict = tag;
    return 0;
}

//==========================================================================
// ParseNextTag

long
XMLParseNextTag( char * buffer, TagPtr * tag )
{
	long   length, pos;
	char * tagName;

    length = GetNextTag(buffer, &tagName, 0);
    if (length == -1) return -1;

	pos = length;
    if (!strncmp(tagName, kXMLTagPList, 6))
    {
        length = 0;
    }
    else if (!strcmp(tagName, kXMLTagDict))
    {
        length = ParseTagList(buffer + pos, tag, kTagTypeDict, 0);
    }
    else if (!strcmp(tagName, kXMLTagDict "/"))
    {
        length = ParseTagList(buffer + pos, tag, kTagTypeDict, 1);
    }
    else if (!strcmp(tagName, kXMLTagKey))
    {
        length = ParseTagKey(buffer + pos, tag);
    }
    else if (!strcmp(tagName, kXMLTagString))
    {
        length = ParseTagString(buffer + pos, tag);
    }
    else if (!strcmp(tagName, kXMLTagInteger))
    {
        length = ParseTagInteger(buffer + pos, tag);
    }
    else if (!strcmp(tagName, kXMLTagData))
    {
        length = ParseTagData(buffer + pos, tag);
    }
    else if (!strcmp(tagName, kXMLTagDate))
    {
        length = ParseTagDate(buffer + pos, tag);
    }
    else if (!strcmp(tagName, kXMLTagFalse))
    {
        length = ParseTagBoolean(buffer + pos, tag, kTagTypeFalse);
    }
    else if (!strcmp(tagName, kXMLTagTrue))
    {
        length = ParseTagBoolean(buffer + pos, tag, kTagTypeTrue);
    }
    else if (!strcmp(tagName, kXMLTagArray))
    {
        length = ParseTagList(buffer + pos, tag, kTagTypeArray, 0);
    }
    else if (!strcmp(tagName, kXMLTagArray "/"))
    {
        length = ParseTagList(buffer + pos, tag, kTagTypeArray, 1);
    }
    else
    {
        *tag = 0;
        length = 0;
    }
  
    if (length == -1) return -1;
  
    return pos + length;
}

//==========================================================================
// ParseTagList

static long
ParseTagList( char * buffer, TagPtr * tag, long type, long empty )
{
	long   length, pos;
	TagPtr tagList, tmpTag;
  
    tagList = 0;
    pos = 0;
  
    if (!empty)
    {
        while (1)
        {
            length = XMLParseNextTag(buffer + pos, &tmpTag);
            if (length == -1) break;

            pos += length;
      
            if (tmpTag == 0) break;
            tmpTag->tagNext = tagList;
            tagList = tmpTag;
        }
    
        if (length == -1)
        {
            XMLFreeTag(tagList);
            return -1;
        }
    }
  
    tmpTag = NewTag();
    if (tmpTag == 0)
    {
        XMLFreeTag(tagList);
        return -1;
    }

    tmpTag->type = type;
    tmpTag->string = 0;
    tmpTag->tag = tagList;
    tmpTag->tagNext = 0;
    
    *tag = tmpTag;
    
    return pos;
}

//==========================================================================
// ParseTagKey

static long
ParseTagKey( char * buffer, TagPtr * tag )
{
    long   length, length2;
    char   *string;
    TagPtr tmpTag, subTag;
  
    length = FixDataMatchingTag(buffer, kXMLTagKey);
    if (length == -1) return -1;
  
    length2 = XMLParseNextTag(buffer + length, &subTag);
    if (length2 == -1) return -1;
  
    tmpTag = NewTag();
    if (tmpTag == 0)
    {
        XMLFreeTag(subTag);
        return -1;
    }
  
    string = NewSymbol(buffer);
    if (string == 0)
    {
        XMLFreeTag(subTag);
        XMLFreeTag(tmpTag);
        return -1;
    }
  
    tmpTag->type = kTagTypeKey;
    tmpTag->string = string;
    tmpTag->tag = subTag;
    tmpTag->tagNext = 0;
  
    *tag = tmpTag;
  
    return length + length2;
}

//==========================================================================
// ParseTagString

static long
ParseTagString( char * buffer, TagPtr * tag )
{
    long   length;
    char * string;
    TagPtr tmpTag;
  
    length = FixDataMatchingTag(buffer, kXMLTagString);
    if (length == -1) return -1;
  
    tmpTag = NewTag();
    if (tmpTag == 0) return -1;
  
    string = NewSymbol(buffer);
    if (string == 0)
    {
        XMLFreeTag(tmpTag);
        return -1;
    }
  
    tmpTag->type = kTagTypeString;
    tmpTag->string = string;
    tmpTag->tag = 0;
    tmpTag->tagNext = 0;
  
    *tag = tmpTag;
    return length;
}

//==========================================================================
// ParseTagInteger

static long
ParseTagInteger( char * buffer, TagPtr * tag )
{
    long   length, integer;
    TagPtr tmpTag;
    
    length = FixDataMatchingTag(buffer, kXMLTagInteger);
    if (length == -1) return -1;
    
    tmpTag = NewTag();
    if (tmpTag == 0) return -1;
    
    integer = 0;
    
    tmpTag->type = kTagTypeInteger;
    tmpTag->string = (char *)integer;
    tmpTag->tag = 0;
    tmpTag->tagNext = 0;
    
    *tag = tmpTag;
    
    return length;
}

//==========================================================================
// ParseTagData

static long
ParseTagData( char * buffer, TagPtr * tag )
{
    long   length;
    TagPtr tmpTag;
    
    length = FixDataMatchingTag(buffer, kXMLTagData);
    if (length == -1) return -1;
    
    tmpTag = NewTag();
    if (tmpTag == 0) return -1;
    
    tmpTag->type = kTagTypeData;
    tmpTag->string = 0;
    tmpTag->tag = 0;
    tmpTag->tagNext = 0;
    
    *tag = tmpTag;
    
    return length;
}

//==========================================================================
// ParseTagDate

static long
ParseTagDate( char * buffer, TagPtr * tag )
{
    long   length;
    TagPtr tmpTag;
    
    length = FixDataMatchingTag(buffer, kXMLTagDate);
    if (length == -1) return -1;
    
    tmpTag = NewTag();
    if (tmpTag == 0) return -1;
    
    tmpTag->type = kTagTypeDate;
    tmpTag->string = 0;
    tmpTag->tag = 0;
    tmpTag->tagNext = 0;
    
    *tag = tmpTag;
    
    return length;
}

//==========================================================================
// ParseTagBoolean

static long
ParseTagBoolean( char * buffer, TagPtr * tag, long type )
{
    TagPtr tmpTag;
    
    tmpTag = NewTag();
    if (tmpTag == 0) return -1;
    
    tmpTag->type = type;
    tmpTag->string = 0;
    tmpTag->tag = 0;
    tmpTag->tagNext = 0;
    
    *tag = tmpTag;
    
    return 0;
}

//==========================================================================
// GetNextTag

static long
GetNextTag( char * buffer, char ** tag, long * start )
{
    long cnt, cnt2;

    if (tag == 0) return -1;
    
    // Find the start of the tag.
    cnt = 0;
    while ((buffer[cnt] != '\0') && (buffer[cnt] != '<')) cnt++;
    if (buffer[cnt] == '\0') return -1;
    
    // Find the end of the tag.
    cnt2 = cnt + 1;
    while ((buffer[cnt2] != '\0') && (buffer[cnt2] != '>')) cnt2++;
    if (buffer[cnt2] == '\0') return -1;

    // Fix the tag data.
    *tag = buffer + cnt + 1;
    buffer[cnt2] = '\0';
    if (start) *start = cnt;
    
    return cnt2 + 1;
}

//==========================================================================
// FixDataMatchingTag
// Modifies 'buffer' to add a '\0' at the end of the tag matching 'tag'.
// Returns the length of the data found, counting the end tag,
// or -1 if the end tag was not found.

static long
FixDataMatchingTag( char * buffer, char * tag )
{
    long   length, start, stop;
    char * endTag;
    
    start = 0;
    while (1)
    {
        length = GetNextTag(buffer + start, &endTag, &stop);
        if (length == -1) return -1;
        
        if ((*endTag == '/') && !strcmp(endTag + 1, tag)) break;
        start += length;
    }
    
    buffer[start + stop] = '\0';
    
    return start + length;
}

//==========================================================================
// NewTag

#define kTagsPerBlock (0x1000)

static TagPtr gTagsFree;

static TagPtr
NewTag( void )
{
	long   cnt;
	TagPtr tag;
  
    if (gTagsFree == 0)
    {
#if USEMALLOC
        tag = (TagPtr)malloc(kTagsPerBlock * sizeof(Tag));
#else
        tag = (TagPtr)AllocateBootXMemory(kTagsPerBlock * sizeof(Tag));
#endif
        if (tag == 0) return 0;
        
        // Initalize the new tags.
        for (cnt = 0; cnt < kTagsPerBlock; cnt++)
        {
            tag[cnt].type = kTagTypeNone;
            tag[cnt].string = 0;
            tag[cnt].tag = 0;
            tag[cnt].tagNext = tag + cnt + 1;
        }
        tag[kTagsPerBlock - 1].tagNext = 0;

        gTagsFree = tag;
    }

    tag = gTagsFree;
    gTagsFree = tag->tagNext;
    
    return tag;
}

//==========================================================================
// XMLFreeTag

void
XMLFreeTag( TagPtr tag )
{
#if DOFREE
    if (tag == 0) return;
  
    if (tag->string) FreeSymbol(tag->string);
  
    XMLFreeTag(tag->tag);
    XMLFreeTag(tag->tagNext);
  
    // Clear and free the tag.
    tag->type = kTagTypeNone;
    tag->string = 0;
    tag->tag = 0;
    tag->tagNext = gTagsFree;
    gTagsFree = tag;
#else
    return;
#endif
}

//==========================================================================
// Symbol object.

struct Symbol
{
  long          refCount;
  struct Symbol *next;
  char          string[];
};
typedef struct Symbol Symbol, *SymbolPtr;

static SymbolPtr FindSymbol(char * string, SymbolPtr * prevSymbol);

static SymbolPtr gSymbolsHead;

//==========================================================================
// NewSymbol

static char *
NewSymbol( char * string )
{
static SymbolPtr lastGuy = 0;
	SymbolPtr symbol;
  
    // Look for string in the list of symbols.
    symbol = FindSymbol(string, 0);
  
    // Add the new symbol.
    if (symbol == 0)
    {
#if USEMALLOC
        symbol = (SymbolPtr)malloc(sizeof(Symbol) + 1 + strlen(string));
#else
        symbol = (SymbolPtr)AllocateBootXMemory(sizeof(Symbol) + 1 + strlen(string));
#endif
        if (symbol == 0) //return 0;
            stop("NULL symbol!");
    
        // Set the symbol's data.
        symbol->refCount = 0;
        strcpy(symbol->string, string);
    
        // Add the symbol to the list.
        symbol->next = gSymbolsHead;
        gSymbolsHead = symbol;
    }
  
    // Update the refCount and return the string.
    symbol->refCount++;

 if (lastGuy && lastGuy->next != 0) stop("last guy not last!");
    return symbol->string;
}

//==========================================================================
// FreeSymbol

#if DOFREE
static void
FreeSymbol( char * string )
{ 
    SymbolPtr symbol, prev;
    
    // Look for string in the list of symbols.
    symbol = FindSymbol(string, &prev);
    if (symbol == 0) return;
    
    // Update the refCount.
    symbol->refCount--;
    
    if (symbol->refCount != 0) return;
    
    // Remove the symbol from the list.
    if (prev != 0) prev->next = symbol->next;
    else gSymbolsHead = symbol->next;
    
    // Free the symbol's memory.
    free(symbol);
}
#endif

//==========================================================================
// FindSymbol

static SymbolPtr
FindSymbol( char * string, SymbolPtr * prevSymbol )
{
    SymbolPtr symbol, prev;
  
    symbol = gSymbolsHead;
    prev = 0;
  
    while (symbol != 0) {
        if (!strcmp(symbol->string, string)) break;
    
        prev = symbol;
        symbol = symbol->next;
    }
  
    if ((symbol != 0) && (prevSymbol != 0)) *prevSymbol = prev;
  
    return symbol;
}
