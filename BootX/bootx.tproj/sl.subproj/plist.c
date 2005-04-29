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
 *  plist.c - plist parsing functions
 *
 *  Copyright (c) 2000-2005 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 *  code split out from drivers.c by Soren Spies, 2005
 */

#include <sl.h>

#define kXMLTagPList   "plist"
#define kXMLTagDict    "dict"
#define kXMLTagKey     "key"
#define kXMLTagString  "string"
#define kXMLTagInteger "integer"
#define kXMLTagData    "data"
#define kXMLTagDate    "date"
#define kXMLTagFalse   "false/"
#define kXMLTagTrue    "true/"
#define kXMLTagArray   "array"
// for back-references used by libkern serializer
#define kXMLTagReference "reference"
#define kXMLTagID      "ID="
#define kXMLTagIDREF   "IDREF="

static long ParseNextTag(char *buffer, TagPtr *tag);
static long ParseTagList(char *buffer, TagPtr *tag, long type, long empty);
static long ParseTagKey(char *buffer, TagPtr *tag);
static long ParseTagString(char *buffer, TagPtr *tag);
static long ParseTagInteger(char *buffer, TagPtr *tag);
static long ParseTagData(char *buffer, TagPtr *tag);
static long ParseTagDate(char *buffer, TagPtr *tag);
static long ParseTagBoolean(char *buffer, TagPtr *tag, long type);
static long GetNextTag(char *buffer, char **tag, long *start, long *empty);
static long FixDataMatchingTag(char *buffer, char *tag);
static TagPtr NewTag(void);
static char *NewSymbol(char *string);
static void FreeSymbol(char *string);

#if PLIST_DEBUG
// for debugging parsing failures
static int gTagsParsed;
static char *gLastTag;
#endif

TagPtr GetProperty(TagPtr dict, char *key)
{
  TagPtr tagList, tag;
  
  if (dict->type != kTagTypeDict) return 0;
  
  tag = 0;    // ?
  tagList = dict->tag;
  while (tagList) {
    tag = tagList;
    tagList = tag->tagNext;
    
    if ((tag->type != kTagTypeKey) || (tag->string == 0)) continue;
    
    if (!strcmp(tag->string, key)) {
      return tag->tag;
    }
  }
  
  return 0;
}

// intended to look for two versions of the tag; now just for sizeof
#define MATCHTAG(parsedTag, keyTag) \
    (!strncmp(parsedTag, keyTag, sizeof(keyTag)-1))

// a tag cache for iokit's super-plists (alas, to merge w/"Symbol" cache?)
// we're not going to use the 0th element; it's used by the whole dict anyway
static int lastid;
static int numids = 0;  // skipping 0th
static TagPtr *idtags;

#define INITIALIDS 10
static long InitTagCache()
{
  long rval = -1;

  do {
    lastid = 0;
    numids = INITIALIDS;
    idtags = (TagPtr*)malloc(numids * sizeof(*idtags));
    if (!idtags)  break;
    bzero(idtags, numids * sizeof(*idtags));

    rval = 0;
  } while(0);

  return rval;
}

static void FreeTagCache()
{
  if (idtags)
    free(idtags);
  idtags = NULL;
}

// get the number from (e.g.): ID="3"
static int ExtractID(char *tagName, int tagLen)
{
  char *idptr = tagName + tagLen;
  int rval = 0;

  while(*idptr != '>') {
    idptr++;
    if (MATCHTAG(idptr, kXMLTagID)) {
      rval = strtol(idptr + sizeof(kXMLTagID)-1+1, 0, 0);  // -NUL +"
      break;
    } else if (MATCHTAG(idptr, kXMLTagIDREF)) {
      rval = strtol(idptr + sizeof(kXMLTagIDREF)-1+1, 0, 0);  // -NUL +"
      break;
    }
    // there can be multiple modifiers (integers have 'size' first)
    while(*idptr != ' ' && *idptr != '>')  idptr++;
  }

 return rval;
}

static TagPtr TagFromRef(char *tagName, int tagLen)
{
  int refidx = ExtractID(tagName, tagLen);
  TagPtr rval = NULL;

  if (refidx <= lastid)
    rval = idtags[refidx];

  return rval;
}

static long SaveTagRef(TagPtr tag, int tagid)
{

  // bumped any time we skip an unsupported tag
  if (tagid != ++lastid) {
    if (tagid > lastid) {
    lastid = tagid;
    }
    else {
      printf("invalid plist: tagid (%d) < lastid (%d)??\n", tagid, lastid);
      return -1;
    }
  }

  // upsize idtags if needed
  if (numids <= lastid) {
    while(numids <= lastid)
      numids *= 2;
    idtags = (TagPtr*)realloc(idtags, numids * sizeof(*idtags));
    if (!idtags)  return -1;
  }

  // and record for later
  idtags[lastid] = tag;

  return 0;
}

long ParseXML(char *buffer, TagPtr *dict)
{
  long           length, pos;
  TagPtr         moduleDict;
  
#if PLIST_DEBUG
  gTagsParsed = 0;
  gLastTag = NULL;
#endif
  if (InitTagCache())  return -1;
  pos = 0;
  while (1) {
    moduleDict = (TagPtr)-1;	// have to detect changes to by-ref parameter
    length = ParseNextTag(buffer + pos, &moduleDict);
    if (length == -1) break;
    pos += length;
    
    if (moduleDict == 0) continue;
    
    // did we actually create anything?
    if (moduleDict != (TagPtr)-1) {
      if (moduleDict->type == kTagTypeDict) break;
      if (moduleDict->type == kTagTypeArray) break;

      FreeTag(moduleDict);
    }
  }

  *dict = moduleDict;
  
#if PLIST_DEBUG
  if (length == -1)
    printf("ParseXML gagged (-1) after %s (%d tags); buf+pos: %s\n",
	gLastTag,gTagsParsed,buffer+pos);
#endif 

  // for tidyness even though kext parsing resets all of malloc
  FreeTagCache();

  // return 0 for no error
  return (length != -1) ? 0 : -1;
}

#define PARSESTASHINGTAG(tagBuf, keyTag, parseFunc) do { \
    TagPtr extantTag = TagFromRef(tagBuf, sizeof(keyTag)-1); \
    if (extantTag) { \
      *tag = extantTag; \
      length = 0; \
    } else { \
      int tagid = ExtractID(tagName, sizeof(keyTag)-1); \
      length = parseFunc(buffer + pos, tag); \
      if (tagid && length != -1) \
	if (-1 == SaveTagRef(*tag, tagid)) \
	  return -1; \
    } \
    } while(0)

static long ParseNextTag(char *buffer, TagPtr *tag)
{
  long length, pos, empty = 0;
  char *tagName;
  TagPtr refTag;
  
  length = GetNextTag(buffer, &tagName, 0, &empty);
  if (length == -1) return -1;
#if PLIST_DEBUG
  gLastTag = tagName;
  gTagsParsed++;
#endif
  
  pos = length;
  if (MATCHTAG(tagName, kXMLTagPList)) {
    length = 0;  // just a header; nothing to parse
	// return-via-reference tag should be left alone
  } else if (MATCHTAG(tagName, kXMLTagDict)) {
    length = ParseTagList(buffer + pos, tag, kTagTypeDict, empty);
  } else if (!strcmp(tagName, kXMLTagKey)) {
    length = ParseTagKey(buffer + pos, tag);
  } else if (MATCHTAG(tagName, kXMLTagReference) && 
	(refTag = TagFromRef(tagName, sizeof(kXMLTagReference)-1))) {
      *tag = refTag;
      length = 0;
  } else if (MATCHTAG(tagName, kXMLTagString)) {
    PARSESTASHINGTAG(tagName, kXMLTagString, ParseTagString);
  } else if (MATCHTAG(tagName, kXMLTagInteger)) {
    PARSESTASHINGTAG(tagName, kXMLTagInteger, ParseTagInteger);
  } else if (!strcmp(tagName, kXMLTagData)) {
    length = ParseTagData(buffer + pos, tag);
  } else if (!strcmp(tagName, kXMLTagDate)) {
    length = ParseTagDate(buffer + pos, tag);
  } else if (!strcmp(tagName, kXMLTagFalse)) {
    length = ParseTagBoolean(buffer + pos, tag, kTagTypeFalse);
  } else if (!strcmp(tagName, kXMLTagTrue)) {
    length = ParseTagBoolean(buffer + pos, tag, kTagTypeTrue);
  } else if (MATCHTAG(tagName, kXMLTagArray)) {
    length = ParseTagList(buffer + pos, tag, kTagTypeArray, empty);
  } else {
    // it wasn't parsed so we consumed no additional characters
    length = 0;
    if (tagName[0] == '/')  // was it an end tag (indicated w/*tag = 0)
      *tag = 0;
    else {
//printf("ignored plist tag: %s (*tag: %x)\n", tagName, *tag);
      *tag = (TagPtr)-1;  // we're *not* returning a tag
    }
  }
  
  if (length == -1) return -1;
  
  return pos + length;
}

static long ParseTagList(char *buffer, TagPtr *tag, long type, long empty)
{
  long   length, pos;
  TagPtr tagList, tmpTag = (TagPtr)-1;
  
  tagList = 0;
  pos = 0;
  
  if (!empty) {
    while (1) {
      tmpTag = (TagPtr)-1;
      length = ParseNextTag(buffer + pos, &tmpTag);
      if (length == -1) break;
      pos += length;

      // detect end of list
      if (tmpTag == 0) break;

      // if we made a new tag, insert into list
      if (tmpTag != (TagPtr)-1) {
        tmpTag->tagNext = tagList;
        tagList = tmpTag;
      }
    }
    
    if (length == -1) {
      FreeTag(tagList);
      return -1;
    }
  }
  
  tmpTag = NewTag();
  if (tmpTag == 0) {
    FreeTag(tagList);
    return -1;
  }
  
  tmpTag->type = type;
  tmpTag->string = 0;
  tmpTag->tag = tagList;
  tmpTag->tagNext = 0;
  
  *tag = tmpTag;
  
  return pos;
}


static long ParseTagKey(char *buffer, TagPtr *tag)
{
  long   length, length2;
  char   *string;
  TagPtr tmpTag, subTag = (TagPtr)-1;  // eliminate possible stale tag
  
  length = FixDataMatchingTag(buffer, kXMLTagKey);
  if (length == -1) return -1;
  
  length2 = ParseNextTag(buffer + length, &subTag);
  if (length2 == -1) return -1;

  // XXXX revisit 4063982 if FreeTag becomes real
  if(subTag == (TagPtr)-1)
    subTag = NULL;
  
  tmpTag = NewTag();
  if (tmpTag == 0) {
    FreeTag(subTag);
    return -1;
  }
  
  string = NewSymbol(buffer);
  if (string == 0) {
    FreeTag(subTag);
    FreeTag(tmpTag);
    return -1;
  }
  
  tmpTag->type = kTagTypeKey;
  tmpTag->string = string;
  tmpTag->tag = subTag;
  tmpTag->tagNext = 0;
  
  *tag = tmpTag;
  
  return length + length2;
}


static long ParseTagString(char *buffer, TagPtr *tag)
{
  long   length;
  char   *string;
  TagPtr tmpTag;
  
  length = FixDataMatchingTag(buffer, kXMLTagString);
  if (length == -1) return -1;
  
  tmpTag = NewTag();
  if (tmpTag == 0) return -1;
  
  string = NewSymbol(buffer);
  if (string == 0) {
    FreeTag(tmpTag);
    return -1;
  }
  
  tmpTag->type = kTagTypeString;
  tmpTag->string = string;
  tmpTag->tag = 0;
  tmpTag->tagNext = 0;
  
  *tag = tmpTag;
  
  return length;
}


static long ParseTagInteger(char *buffer, TagPtr *tag)
{
  long   length;
  char   *intString;
  TagPtr tmpTag;
  
  length = FixDataMatchingTag(buffer, kXMLTagInteger);
  if (length == -1) return -1;
  
  tmpTag = NewTag();
  if (tmpTag == 0) return -1;
  
  intString = NewSymbol(buffer);
  if (intString == 0) {
    FreeTag(tmpTag);
    return -1;
  }
  
  tmpTag->type = kTagTypeInteger;
  tmpTag->string = intString;
  tmpTag->tag = 0;
  tmpTag->tagNext = 0;
  
  *tag = tmpTag;
  
  return length;
}


static long ParseTagData(char *buffer, TagPtr *tag)
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


static long ParseTagDate(char *buffer, TagPtr *tag)
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


static long ParseTagBoolean(char *buffer, TagPtr *tag, long type)
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


static long GetNextTag(char *buffer, char **tag, long *start, long *empty)
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
  if (empty && cnt2 > 1)
    *empty = buffer[cnt2-1] == '/';
  
  // Fix the tag data.
  *tag = buffer + cnt + 1;
  buffer[cnt2] = '\0';
  if (start) *start = cnt;
  
  return cnt2 + 1;
}


static long FixDataMatchingTag(char *buffer, char *tag)
{
  long length, start, stop;
  char *endTag;
  
  start = 0;
  while (1) {
    length = GetNextTag(buffer + start, &endTag, &stop, NULL);
    if (length == -1) return -1;
    
    if ((*endTag == '/') && !strcmp(endTag + 1, tag)) break;
    start += length;
  }
  
  buffer[start + stop] = '\0';
  
  return start + length;
}


#define kTagsPerBlock (0x1000)

static TagPtr gTagsFree;

static TagPtr NewTag(void)
{
  long   cnt;
  TagPtr tag;
  
  if (gTagsFree == 0) {
    tag = (TagPtr)AllocateBootXMemory(kTagsPerBlock * sizeof(Tag));
    if (tag == 0) return 0;
    
    // Initalize the new tags.
    for (cnt = 0; cnt < kTagsPerBlock; cnt++) {
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


// currently a no-op
void FreeTag(TagPtr tag)
{
  return;  // XXXX revisit callers, particularly ParseTagKey (4063982)
  if (tag == 0) return;
  
  if (tag->string) FreeSymbol(tag->string);
  
  FreeTag(tag->tag);
  FreeTag(tag->tagNext);
  
  // Clear and free the tag.
  tag->type = kTagTypeNone;
  tag->string = 0;
  tag->tag = 0;
  tag->tagNext = gTagsFree;
  gTagsFree = tag;
}


struct Symbol {
  long          refCount;
  struct Symbol *next;
  char          string[1];
};
typedef struct Symbol Symbol, *SymbolPtr;

static SymbolPtr FindSymbol(char *string, SymbolPtr *prevSymbol);

static SymbolPtr gSymbolsHead;


static char *NewSymbol(char *string)
{
  SymbolPtr symbol;
  
  // Look for string in the list of symbols.
  symbol = FindSymbol(string, 0);
  
  // Add the new symbol.
  if (symbol == 0) {
    symbol = AllocateBootXMemory(sizeof(Symbol) + strlen(string));
    if (symbol == 0) return 0;
    
    // Set the symbol's data.
    symbol->refCount = 0;
    strcpy(symbol->string, string);
    
    // Add the symbol to the list.
    symbol->next = gSymbolsHead;
    gSymbolsHead = symbol;
  }
  
  // Update the refCount and return the string.
  symbol->refCount++;
  return symbol->string;
}


// currently a no-op
static void FreeSymbol(char *string)
{ 
#if 0
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
#endif
}


static SymbolPtr FindSymbol(char *string, SymbolPtr *prevSymbol)
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

#if PLIST_DEBUG
static void DumpTagDict(TagPtr tag, long depth);
static void DumpTagKey(TagPtr tag, long depth);
static void DumpTagString(TagPtr tag, long depth);
static void DumpTagInteger(TagPtr tag, long depth);
static void DumpTagData(TagPtr tag, long depth);
static void DumpTagDate(TagPtr tag, long depth);
static void DumpTagBoolean(TagPtr tag, long depth);
static void DumpTagArray(TagPtr tag, long depth);
static void DumpSpaces(long depth);

void DumpTag(TagPtr tag, long depth)
{
  if (tag == 0) return;
  
  switch (tag->type) {
  case kTagTypeDict :
    DumpTagDict(tag, depth);
    break;
    
  case kTagTypeKey :
    DumpTagKey(tag, depth);
    break;
    
  case kTagTypeString :
    DumpTagString(tag, depth);
    break;
    
  case kTagTypeInteger :
    DumpTagInteger(tag, depth);
    break;
    
  case kTagTypeData :
    DumpTagData(tag, depth);
    break;
    
  case kTagTypeDate :
    DumpTagDate(tag, depth);
    break;
    
  case kTagTypeFalse :
  case kTagTypeTrue :
    DumpTagBoolean(tag, depth);
    break;
    
  case kTagTypeArray :
    DumpTagArray(tag, depth);
    break;
    
  default :
    break;
  }
}


static void DumpTagDict(TagPtr tag, long depth)
{
  TagPtr tagList;
  
  if (tag->tag == 0) {
    DumpSpaces(depth);
    printf("<%s/>\n", kXMLTagDict);
  } else {
    DumpSpaces(depth);
    printf("<%s>\n", kXMLTagDict);
    
    tagList = tag->tag;
    while (tagList) {
      DumpTag(tagList, depth + 1);
      tagList = tagList->tagNext;
    }
    
    DumpSpaces(depth);
    printf("</%s>\n", kXMLTagDict);
  }
}


static void DumpTagKey(TagPtr tag, long depth)
{
  DumpSpaces(depth);
  printf("<%s>%s</%s>\n", kXMLTagKey, tag->string, kXMLTagKey);
  
  DumpTag(tag->tag, depth);
}


static void DumpTagString(TagPtr tag, long depth)
{
  DumpSpaces(depth);
  printf("<%s>%s</%s>\n", kXMLTagString, tag->string, kXMLTagString);
}


/* integers used to live as char*s but we need 64 bit ints */
static void DumpTagInteger(TagPtr tag, long depth)
{
  DumpSpaces(depth);
  printf("<%s>%s</%s>\n", kXMLTagInteger, tag->string, kXMLTagInteger);
}


static void DumpTagData(TagPtr tag, long depth)
{
  DumpSpaces(depth);
  printf("<%s>%x</%s>\n", kXMLTagData, tag->string, kXMLTagData);
}


static void DumpTagDate(TagPtr tag, long depth)
{
  DumpSpaces(depth);
  printf("<%s>%x</%s>\n", kXMLTagDate, tag->string, kXMLTagDate);
}


static void DumpTagBoolean(TagPtr tag, long depth)
{
  DumpSpaces(depth);
  printf("<%s>\n", (tag->type == kTagTypeTrue) ? kXMLTagTrue : kXMLTagFalse);
}


static void DumpTagArray(TagPtr tag, long depth)
{
  TagPtr tagList;
  
  if (tag->tag == 0) {
    DumpSpaces(depth);
    printf("<%s/>\n", kXMLTagArray);
  } else {
    DumpSpaces(depth);
    printf("<%s>\n", kXMLTagArray);
    
    tagList = tag->tag;
    while (tagList) {
      DumpTag(tagList, depth + 1);
      tagList = tagList->tagNext;
    }
    
    DumpSpaces(depth);
    printf("</%s>\n", kXMLTagArray);
  }
}


static void DumpSpaces(long depth)
{
  long cnt;
  
  for (cnt = 0; cnt < (depth * 4); cnt++) putchar(' ');
}
#endif
