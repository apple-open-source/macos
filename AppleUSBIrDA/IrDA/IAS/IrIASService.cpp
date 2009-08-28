/*
    File:       IrIASService.cpp

    Contains:   Implementation of IrDA's silly name server

*/

#include "IrIASService.h"
#include "CListIterator.h"
#include "CBuffer.h"
#include "IrDALog.h"

// consider splitting these into n-tables, one for each class?

#if (hasTracing > 0 && hasIASServiceTracing > 0)

enum tracecodes {
    kLogServiceNew = 1,
    kLogServiceFree,
    kLogClassNew,
    kLogClassFree,
    kLogAttrNew,
    kLogAttrFree,
    kLogNamedListFree,
    kLogElementNew,
    kLogElementFree
};

static
EventTraceCauseDesc TraceEvents[] = {
    {kLogServiceNew,        "IrService: service new, obj="},
    {kLogServiceFree,       "IrService: service free, obj="},
    {kLogClassNew,          "IrService: class new, obj="},
    {kLogClassFree,         "IrService: class free, obj="},
    {kLogAttrNew,           "IrService: attr new, obj="},
    {kLogAttrFree,          "IrService: attr free, obj="},
    {kLogNamedListFree,     "IrService: named list free, obj="},
    {kLogElementNew,        "IrService: element new, obj="},
    {kLogElementFree,       "IrService: element free, obj="}
};

#define XTRACE(x, y, z) IrDALogAdd( x, y, ((uintptr_t)z & 0xffff), TraceEvents, true )
#else
#define XTRACE(x, y, z) ((void)0)
#endif

#pragma mark 
//============== TIASService ==============

//--------------------------------------------------------------------------------
//      TIASService
//--------------------------------------------------------------------------------
#define super TIASNamedList
OSDefineMetaClassAndStructors(TIASService, TIASNamedList);

/*static*/
TIASService *
TIASService::tIASService(void)
{
    TIASService *obj = new TIASService;
    
    XTRACE(kLogServiceNew, 0, obj);
    
    if (obj && !obj->Init()) {      // this named list has no name
	obj->release();
	obj = nil;
    }
    return obj;
}


//--------------------------------------------------------------------------------
//      free
//--------------------------------------------------------------------------------
void TIASService::free()
{
    long index;
    
    XTRACE(kLogServiceFree, 0, this);

    // Iterate thru the list of classes and delete each one.  Note that I'm not
    // removing them because I'm depending on the CList destructor to do that for me.
    for (index = 0; index < this->GetArraySize(); index++) {
	TIASClass* theClass = (TIASClass*)this->At(index);
	theClass->release();
    }
    
    super::free();

} // TIASService::free


//--------------------------------------------------------------------------------
//      AddIntegerEntry
//--------------------------------------------------------------------------------
IrDAErr TIASService::AddIntegerEntry(const UChar* className, const UChar* attributeName, ULong intValue)
{
    TIASElement* theEntry;
    
    // Create, initialize the new entry
    theEntry = TIASElement::tIASElement(intValue);      // create and initialize
    require(theEntry, Fail_Element_New);

    // Let AddAttributeEntry finish the job (it will free theEntry if there are any errors)
    return AddAttributeEntry(className, attributeName, theEntry);

Fail_Element_New:
    return kIrDAErrNoMemory;

} // TIASService::AddIntegerEntry


//--------------------------------------------------------------------------------
//      AddStringEntry
//--------------------------------------------------------------------------------
IrDAErr TIASService::AddStringEntry(const UChar* className, const UChar* attributeName,
				    const UChar* stringValue, const UChar charSet, const ULong length)
{
    TIASElement *theEntry;

    // Create, initialize the new entry
    theEntry = TIASElement::tIASElement(stringValue, charSet, length);      // create and initialize
    require(theEntry, Fail_Element_New);

    // Let AddAttributeEntry finish the job (it will free theEntry if there are any errors)
    return AddAttributeEntry(className, attributeName, theEntry);

Fail_Element_New:
    return kIrDAErrNoMemory;

} // TIASService::AddStringEntry


//--------------------------------------------------------------------------------
//      AddNBytesEntry
//--------------------------------------------------------------------------------
IrDAErr TIASService::AddNBytesEntry(const UChar* className, const UChar* attributeName, const UChar*  aFewBytes, ULong length)
{
    TIASElement* theEntry;

    // Create, initialize the new entry
    theEntry = TIASElement::tIASElement(aFewBytes, length);     // create and initialize
    require(theEntry, Fail_Element_New);

    // Let AddAttributeEntry finish the job (it will free theEntry if there are any errors)
    return AddAttributeEntry(className, attributeName, theEntry);

Fail_Element_New:
    return kIrDAErrNoMemory;

} // TIASService::AddNBytesEntry


//--------------------------------------------------------------------------------
//      AddAttributeEntry
//--------------------------------------------------------------------------------
IrDAErr TIASService::AddAttributeEntry(const UChar* className, const UChar* attributeName, TIASElement* theEntry)
{
    IrDAErr result = kIrDAErrNoMemory;
    ULong flags;
    TIASAttribute *theAttr;

    // Make sure the class and attribute exist
    theAttr = AddAttribute(className, attributeName, flags);
    XREQUIRE(theAttr, Fail_ClassAttr_New);

    // Insert the new entry
    result = theAttr->Insert(theEntry);
    XREQUIRENOT(result, Fail_Entry_Insert);

    return noErr;

Fail_Entry_Insert:
    RemoveAttribute(className, attributeName, flags);

Fail_ClassAttr_New:
    theEntry->release();

    return result;

} // TIASService::AddAttributeEntry


//--------------------------------------------------------------------------------
//      AddAttribute
//--------------------------------------------------------------------------------
TIASAttribute* TIASService::AddAttribute(const UChar* className, const UChar* attributeName, ULong& flags)
{
    IrDAErr result;
    TIASClass* theClass;
    TIASAttribute* theAttr;

    theClass = AddClass(className, flags);
    XREQUIRE(theClass, Fail_Class_New);

    // Does the attribute already exist in this class
    theAttr = theClass->FindAttribute(attributeName);

    // If attribute exists, done
    if (theAttr != nil) {
	return theAttr;
    }

    // Create, initialize the new attribute
    theAttr = TIASAttribute::tIASAttribute(attributeName);
    require(theAttr, Fail_Attribute_New);
    
    result = theClass->Insert(theAttr);
    XREQUIRENOT(result, Fail_Attribute_Insert);

    // I created the attribute, let caller know so then can delete it if necessary
    flags |= kIASAddedAttribute;

    return theAttr;

Fail_Attribute_Insert:
    theAttr->release();

Fail_Attribute_New:
    RemoveClass(className, flags);

Fail_Class_New:
    return nil;

} // TIASService::AddAttribute


//--------------------------------------------------------------------------------
//      AddClass
//--------------------------------------------------------------------------------
TIASClass* TIASService::AddClass(const UChar* className, ULong& flags)
{
    IrDAErr result;
    TIASClass *theClass;

    // Init flags
    flags = 0;

    // Does the class already exist in the name service
    theClass = FindClass(className);

    // If class exists, done
    if (theClass != nil) {
	return theClass;
    }

    // Create, initialize the new class
    theClass = TIASClass::tIASClass(className);
    require(theClass, Fail_Class_New);
    
    result = this->Insert(theClass);
    XREQUIRENOT(result, Fail_Class_Insert);

    // I created the class, let caller know so then can delete it if necessary
    flags |= kIASAddedClass;

    return theClass;

Fail_Class_Insert:
    theClass->release();

Fail_Class_New:
    return nil;

} // TIASService::AddClass


//--------------------------------------------------------------------------------
//      RemoveClass
//--------------------------------------------------------------------------------
IrDAErr TIASService::RemoveClass(const UChar* className, ULong flags)
{
    TIASClass *theClass;

    theClass = FindClass(className);
    if (theClass && (flags & kIASDeleteClass)) {
	this->Remove(theClass);
	theClass->release();
    }
    return noErr;

} // TIASService::RemoveClass


//--------------------------------------------------------------------------------
//      RemoveAttribute
//--------------------------------------------------------------------------------
IrDAErr TIASService::RemoveAttribute(const UChar* className, const UChar* attributeName, ULong flags)
{
    TIASClass* theClass;
    TIASAttribute* theAttr;

    theClass = FindClass(className);
    if (theClass) {
	theAttr = theClass->FindAttribute(attributeName);
	if (theAttr && (flags & kIASDeleteAttribute)) {
	    theClass->Remove(theAttr);
	    theAttr->release();
	}
	if (flags & kIASDeleteClass) {
	    this->Remove(theClass);
	    theClass->release();
	}
    }
    return noErr;

} // TIASService::RemoveAttribute


//--------------------------------------------------------------------------------
//      FindClass
//--------------------------------------------------------------------------------
TIASClass* TIASService::FindClass(const UChar* className)
{
    return (TIASClass*)this->Search(className);

} // TIASService::FindClass


//--------------------------------------------------------------------------------
//      FindAttribute
//--------------------------------------------------------------------------------
TIASAttribute* TIASService::FindAttribute(const UChar* className, const UChar* attributeName)
{
    TIASClass* theClass;
    TIASAttribute* theAttr = nil;

    theClass = FindClass(className);
    if (theClass) {
	theAttr = theClass->FindAttribute(attributeName);
    }
    return theAttr;

} // TIASService::FindAttribute


#pragma mark 
//============== TIASClass ================

#undef super
#define super TIASNamedList
OSDefineMetaClassAndStructors(TIASClass, TIASNamedList);


//--------------------------------------------------------------------------------
//      TIASClass
//--------------------------------------------------------------------------------
/*static*/
TIASClass *
TIASClass::tIASClass(const UChar *name)
{
    TIASClass *obj = new TIASClass;
    
    XTRACE(kLogClassNew, 0, obj);
    
    if (obj && !obj->Init(name)) {
	obj->release();
	obj = nil;
    }
    return obj;
}


//--------------------------------------------------------------------------------
//      free
//--------------------------------------------------------------------------------
void TIASClass::free()
{
    long index;
    
    XTRACE(kLogClassFree, 0, this);

    // Iterate thru the list of attributes and delete each one.  Note that I'm not
    // removing them because I'm depending on the list destructor to do that for me.
    for (index = 0; index < this->GetArraySize(); index++) {
	TIASAttribute* theAttr = (TIASAttribute*)this->At(index);
	theAttr->release();
    }
    
    super::free();

} // TIASClass::free


//--------------------------------------------------------------------------------
//      Insert
//--------------------------------------------------------------------------------
IrDAErr TIASClass::Insert(TIASAttribute* attribute)
{
    return CList::Insert((void*)attribute);

} // TIASClass::Insert


//--------------------------------------------------------------------------------
//      FindAttribute
//--------------------------------------------------------------------------------
TIASAttribute* TIASClass::FindAttribute(const UChar* attributeName)
{
    return (TIASAttribute*)this->Search(attributeName);

} // TIASClass::FindAttribute


#pragma mark 
//============== TIASAttribute ============

#undef super
#define super TIASNamedList
OSDefineMetaClassAndStructors(TIASAttribute, TIASNamedList);


//--------------------------------------------------------------------------------
//      TIASAttribute
//--------------------------------------------------------------------------------
/*static*/
TIASAttribute *
TIASAttribute::tIASAttribute(const UChar *name)
{
    TIASAttribute *obj = new TIASAttribute;
    
    XTRACE(kLogAttrNew, 0, obj);
    
    if (obj && !obj->Init(name)) {
	obj->release();
	obj = nil;
    }
    return obj;
}

/*static*/
TIASAttribute *
TIASAttribute::tIASAttribute(CBuffer *buffer)
{
    TIASAttribute *obj = new TIASAttribute;

    XTRACE(kLogAttrNew, 0, obj);

    if (obj && !obj->InitFromBuffer(buffer)) {
	obj->release();
	obj = nil;
    }
    return obj;
}


//--------------------------------------------------------------------------------
//      free
//--------------------------------------------------------------------------------
void TIASAttribute::free()
{
    long index;
    
    XTRACE(kLogAttrFree, 0, this);

    // Iterate thru the list of elements and delete each one.  Note that I'm not
    // removing them because I'm depending on the list destructor to do that for me.
    for (index = 0; index < this->GetArraySize(); index++) {
	TIASElement* theElement = (TIASElement*)this->At(index);
	theElement->release();
    }
    super::free();

} // TIASAttribute::free


//--------------------------------------------------------------------------------
//      Insert
//--------------------------------------------------------------------------------
IrDAErr TIASAttribute::Insert(TIASElement* element)
{
    return CList::Insert((void*)element);

} // TIASAttribute::Insert


//--------------------------------------------------------------------------------
//      AddInfoToBuffer
//--------------------------------------------------------------------------------
void TIASAttribute::AddInfoToBuffer(CBuffer* buffer)
{
    long index;
    ULong arraySize = this->GetArraySize();

    // Write out the number of elements (as a 16 byte quantity)
    buffer->Put((int)((arraySize >> 8) & 0xFF));    // Hi byte of short
    buffer->Put((int)((arraySize >> 0) & 0xFF));    // Lo byte of short

    // No iterate thru the elements and let each of them add themselves to the buffer
    for (index = 0; index < this->GetArraySize(); index++) {
	TIASElement* theElement = (TIASElement*)this->At(index);
	theElement->AddInfoToBuffer(buffer);
    }

} // TIASAttribute::AddInfoToBuffer


//--------------------------------------------------------------------------------
//      InitFromBuffer
//--------------------------------------------------------------------------------
Boolean TIASAttribute::InitFromBuffer(CBuffer* buffer)
{
    ULong entryIndex;
    ULong listLength;
    UByte listLenBuf[2];    // Defined as a Big Endian UShort by protocol
    TIASElement* element;

    if (!super::Init()) return false;
    
    // All (successful) replies have a list length field
    if (buffer->Getn(listLenBuf, sizeof(listLenBuf)) != sizeof(listLenBuf)) {
	return false;
    }
    listLength = (ULong)(listLenBuf[0] * 256) + (ULong)listLenBuf[1];

    // Get each attribute entry and add it to the attribute
    for (entryIndex = 0; entryIndex < listLength; entryIndex++) {
	IrDAErr result;
	
	// Create an attribute entry from the buffer info
	element = TIASElement::tIASElement(buffer);
	require(element, Fail);
	
	// Add the attr entry to the attribute (list)
	result = this->Insert(element);
	if (result != noErr) {
	    element->release();
	    return false;
	}
    }

    return true;

Fail:
    return false;

} // TIASAttribute::ExtractInfoFromBuffer


#pragma mark 
//============== TIASNamedList ============

#undef super
#define super CList
OSDefineMetaClassAndStructors(TIASNamedList, CList);


// never created directly, no factories here!

//--------------------------------------------------------------------------------
//      free
//--------------------------------------------------------------------------------
void TIASNamedList::free()
{
    XTRACE(kLogNamedListFree, 0, this);
    
    if (fName != nil) {
	IOFree(fName, fNameLen);
	fName = nil;
    }

    super::free();
    
} // TIASNamedList::free


//--------------------------------------------------------------------------------
//      Init
//--------------------------------------------------------------------------------
Boolean TIASNamedList::Init(const UChar *theName)
{
    fName = nil;
    
    if (!super::init()) return false;
    
    fNameLen = strlen((const char*)theName) + 1;
    if (fNameLen) {
	fName = (UChar*)IOMalloc(strlen((const char*)theName) + 1);
	require(fName, Fail);
    }
    strlcpy((char*)fName, (const char*)theName, fNameLen);
    return true;
    
Fail:
    return false;
} // TIASNamedList::Init

//
// init w/out a name, seems kinda silly, but here we are
//
Boolean TIASNamedList::Init(void)
{
    fName = nil;
    return super::init();
}

//--------------------------------------------------------------------------------
//      Search
//--------------------------------------------------------------------------------
void* TIASNamedList::Search(const UChar* matchName)
{
    CListIterator *iter = CListIterator::cListIterator(this);
    TIASNamedList* item;
    void* result = nil;
    //int review_consider_putting_in_dynamic_cast;    // to make sure list items are named lists

    for (item = (TIASNamedList*)iter->FirstItem(); iter->More(); item = (TIASNamedList*)iter->NextItem()) {
	if (strcmp((const char*)(item->fName), (const char*)matchName) == 0) {
	    result = (void*)item;
	    break;
	}
    }
    iter->release();

    return result;

} // TIASNamedList::Search


#pragma mark 
//============== TIASElement ==============

#undef super
#define super OSObject
OSDefineMetaClassAndStructors(TIASElement, OSObject);

//--------------------------------------------------------------------------------
//      TIASElement
//--------------------------------------------------------------------------------
/*static*/
TIASElement * TIASElement::tIASElement(ULong theValue)
{
    TIASElement *obj = new TIASElement;
    
    XTRACE(kLogElementNew, 0, obj);
    
    if (obj && !obj->init_with_long(theValue)) {
	obj->release();
	obj = nil;
    }
    return obj;
}

/*static*/
TIASElement * TIASElement::tIASElement(const UChar* theBytes, ULong length)
{
    TIASElement *obj = new TIASElement;
    
    XTRACE(kLogElementNew, 0, obj);
    
    if (obj && !obj->init_with_nbytes(theBytes, length)) {
	obj->release();
	obj = nil;
    }
    return obj;
}

/*static*/
TIASElement * TIASElement::tIASElement(const UChar* theString, UChar charSet, ULong length)
{
    TIASElement *obj = new TIASElement;
    
    XTRACE(kLogElementNew, 0, obj);
    
    if (obj && !obj->init_with_string(theString, charSet, length)) {
	obj->release();
	obj = nil;
    }
    return obj;
}

/*static*/
TIASElement * TIASElement::tIASElement(CBuffer* buffer)
{
    TIASElement *obj = new TIASElement;
    
    XTRACE(kLogElementNew, 0, obj);
    
    if (obj && !obj->init_with_buffer(buffer)) {
	obj->release();
	obj = nil;
    }
    return obj;
}



//--------------------------------------------------------------------------------
//      free
//--------------------------------------------------------------------------------
void TIASElement::free()
{
    int len;
    
    
    XTRACE(kLogElementFree, 0, this);
    
    if (nameOrBytes && (nameOrBytes != (UByte*)&valueOrBytes)) {        // if we allocated memory
	len = length;
	if (type == kIASValueString)        // if a unicode string, then allocated memory is length+2
	    len += 2;
	IOFree(nameOrBytes, len);
	nameOrBytes = nil;
    }
    
    super::free();
}


//// Inits
Boolean TIASElement::init_with_long(ULong theValue)
{
    type = kIASValueMissing;
    length = 0;
    valueOrBytes = 0;
    nameOrBytes = nil;
    characterSet = kIASCharSetAscii;

    if (!super::init()) return false;
    
    return SetInteger(theValue);
}

Boolean TIASElement::init_with_nbytes(const UChar* theBytes, ULong length)
{
    type = kIASValueMissing;
    length = 0;
    valueOrBytes = 0;
    nameOrBytes = nil;
    characterSet = kIASCharSetAscii;

    if (!super::init()) return false;
    
    return SetNBytes(theBytes, length);
}

Boolean TIASElement::init_with_string(const UChar* theString, UChar charSet, ULong length)
{
    type = kIASValueMissing;
    length = 0;
    valueOrBytes = 0;
    nameOrBytes = nil;
    characterSet = kIASCharSetAscii;

    if (!super::init()) return false;
    
    return SetString(theString, charSet, length);
}

Boolean TIASElement::init_with_buffer(CBuffer* buffer)
{
    type = kIASValueMissing;
    length = 0;
    valueOrBytes = 0;
    nameOrBytes = nil;
    characterSet = kIASCharSetAscii;

    if (!super::init()) return false;
    
    return ExtractInfoFromBuffer(buffer);
}



//--------------------------------------------------------------------------------
//      SetInteger
//--------------------------------------------------------------------------------
Boolean TIASElement::SetInteger(ULong theValue)
{
    type = kIASValueInteger;
    length = 4;
    valueOrBytes = htonl(theValue);
    nameOrBytes = (UByte*)&valueOrBytes;
    return true;
} // TIASElement::SetInteger


//--------------------------------------------------------------------------------
//      SetNBytes
//--------------------------------------------------------------------------------
Boolean TIASElement::SetNBytes(const UChar * theBytes, ULong theBytesLength)
{
    type = kIASValueNBytes;
    length = theBytesLength;
    nameOrBytes = (UByte*)IOMalloc(length);
    require(nameOrBytes, Fail);
    BlockMove(theBytes, nameOrBytes, length);
    return true;
    
Fail:
    return false;
} // TIASElement::SetNBytes


//--------------------------------------------------------------------------------
//      SetString
//--------------------------------------------------------------------------------
Boolean TIASElement::SetString(const UChar* theString, const UChar charSet, const ULong len)
{
    type = kIASValueString;
    if (charSet != kIASCharSetUniCode)          // if not unicode, use strlen to compute length
	length = (ULong)strlen((const char*)theString);
    else
	length = len;       // if unicode, use supplied length
	
    valueOrBytes = 0;
    characterSet = charSet;
    
    // allocate room for string and two nulls (unicode or C) at end
    nameOrBytes = (UByte*)IOMalloc((unsigned int)(length+2));
    require(nameOrBytes, Fail);

    //strcpy((char*)nameOrBytes, (const char*)theString);
    BlockMove(theString, nameOrBytes, length);      // copy the string
    // would normally just have one null at the end of a C string, but
    // unicode "end of string" appears to want two nulls, since it's
    // a 16-bit encoding.  So always append two nulls.
    nameOrBytes[length] = 0;
    nameOrBytes[length+1] = 0;
    return true;
    
Fail:
    return false;

} // TIASElement::SetString


//--------------------------------------------------------------------------------
//      GetInteger
//--------------------------------------------------------------------------------
IrDAErr TIASElement::GetInteger(ULong  *theValue)
{
    if (type != kIASValueInteger) {
	return kIrDAErrGeneric; // ***FIXME: Better error return
    }
    *theValue = ntohl(valueOrBytes);
    return noErr;

} // TIASElement::GetInteger


//--------------------------------------------------------------------------------
//      GetNBytes
//--------------------------------------------------------------------------------
IrDAErr TIASElement::GetNBytes(UByte **theBytes, ULong *theLength)
{
    if (type != kIASValueNBytes) {
	return kIrDAErrGeneric; // ***FIXME: Better error return
    }
    *theBytes = nameOrBytes;        // return pointer directly to our buffer
    *theLength = length;
    return noErr;

} // TIASElement::GetNBytes

//--------------------------------------------------------------------------------
//      GetString
//--------------------------------------------------------------------------------
IrDAErr TIASElement::GetString(UByte **theString, UByte *charSet, ULong *len)
{
    if (type != kIASValueString) {
	return kIrDAErrGeneric;         // ***FIXME: Better error return
    }
    *theString = nameOrBytes;       // return pointer to our copy!
    
    if (charSet != nil)             // if caller is asking for character set
	*charSet = characterSet;    // return it too
    
    if (len != nil)                 // if caller supplied a length buffer
	*len = length;              // return the length too (needed for unicode)
	
    return noErr;                   // hope the client doesn't clobber me :-)

} // TIASElement::GetString


//--------------------------------------------------------------------------------
//      AddInfoToBuffer
//--------------------------------------------------------------------------------
void TIASElement::AddInfoToBuffer(CBuffer* buffer)
{
    UByte header[5];
    UByte* pHdr = &header[0];

    // Fill in the object id - not really exists, faking it if I can get away with it
    *pHdr++ = 0;
    *pHdr++ = 0;

    // Fill in the type
    *pHdr++ = type;

    XASSERT(length < 256);

    // Fill in ascii char set/lengths
    if (type == kIASValueNBytes) {
	*pHdr++ = 0;                // Hi byte of length
	*pHdr++ = (UByte)length;    // Lo byte of length
    }
    else if (type == kIASValueString) {
	*pHdr++ = characterSet;     // Character set (defaults to ascii)
	*pHdr++ = (UByte)length;    // Length of string
    }

    // Put out the header
    buffer->Putn(header, pHdr - header);

    // Put out the integer/string/octet sequence
    buffer->Putn(nameOrBytes, length);

} // TIASElement::AddInfoToBuffer


//--------------------------------------------------------------------------------
//      ExtractInfoFromBuffer
//--------------------------------------------------------------------------------
Boolean TIASElement::ExtractInfoFromBuffer(CBuffer* buffer)
{
    UByte entryHeader[3];   // UShort object id followed by attr value type id
    UByte lengthInfo[2];    // high length byte, then low length byte
    UByte charSet;          // character set
    ULong length;   
    ULong intValue;
    Boolean rc;
    unsigned char buf[1024];    // max nBytes is 1024 (no null at end)


    // Each entry must have at least a 2-byte object id and an attr value type id byte
    if (buffer->Getn(entryHeader, sizeof(entryHeader)) != sizeof(entryHeader)) {
	return false;
    }

    // Determine how many bytes to input and "set" the appropriate type/value
    switch(entryHeader[2] /*type*/ ) {
	case kIASValueMissing:
	    // This is the default value of an attr element - all done
	    rc = true;
	    break;

	case kIASValueInteger:
	    if (buffer->Getn((UByte*)&intValue, sizeof(intValue)) != sizeof(intValue)) {
		return kIrDAErrGeneric; // FIXME: Return better error code
	    }
	    intValue = ntohl(intValue);	// convert to host before saving
	    rc = SetInteger(intValue);
	    break;

	case kIASValueNBytes:
	    // first get the 16-bit length
	    if (buffer->Getn(lengthInfo, sizeof(lengthInfo)) != sizeof(lengthInfo)) {
		return false;
	    }
	    length = lengthInfo[0] << 8 | lengthInfo[1];    // could read directly into a short
	    check(length <= 1024);          // according to spec
	    check(length <= sizeof(buf));   // sanity
	    if ((UInt32)buffer->Getn(buf, length) != length)
		return false;
	    rc = SetNBytes(buf, length);
	    break;

	case kIASValueString:
	    // first get the character set code, length bytes
	    if (buffer->Getn(lengthInfo, sizeof(lengthInfo)) != sizeof(lengthInfo)) {
		return kIrDAErrGeneric; // FIXME: Return better error code
	    }
	    charSet = lengthInfo[0];    // 1st byte is the character set
	    length = lengthInfo[1];     // 2nd byte is the length
	    if ((UInt32)buffer->Getn(buf, length) != length)
		    return false;
	    buf[length] = 0;                    // turn into C string before calling SetString
						// since length parm is ignored unless unicode charset
	    rc = SetString(buf, charSet, length);   // copy it into the attribute
	    break;

	default:
	    // Unknown type
	    //DebugPrintf("extract info from buffer type %d", entryHeader[2]);  // jdg
	    rc = false;
	    break;
    }

    return rc;

} // TIASElement::ExtractInfoFromBuffer

