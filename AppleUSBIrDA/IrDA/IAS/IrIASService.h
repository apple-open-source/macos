/*
    File:       IrIASService.h

    Contains:   Methods for implementing IrIASService

*/


#ifndef __IRIASSERVICE_H
#define __IRIASSERVICE_H

#include "CList.h"

// Constants

enum IASCharacterSetCodes
{
    kIASCharSetAscii        = 0,
    kIASCharSetISO_8859_1   = 1,
    kIASCharSetISO_8859_2   = 2,
    kIASCharSetISO_8859_3   = 3,
    kIASCharSetISO_8859_4   = 4,
    kIASCharSetISO_8859_5   = 5,
    kIASCharSetISO_8859_6   = 6,
    kIASCharSetISO_8859_7   = 7,
    kIASCharSetISO_8859_8   = 8,
    kIASCharSetISO_8859_9   = 9,
    kIASCharSetUniCode      = 0xff
};

enum IASFrameFlags
{
    kIASFrameLstBit     = 0x80,
    kIASFrameAckBit     = 0x40,
    kIASFrameOpCodeMask = 0x3F
};

enum IASOpCodes
{
    kIASOpUnassigned,
    kIASOpGetInfoBaseDetails,
    kIASOpGetObjects,
    kIASOpGetValue,
    kIASOpGetValueByClass,                      // The only supported operation
    kIASOpGetObjectInfo,
    kIASOpGetAttributeNames
};

enum IASReturnCodes
{
    kIASRetOkay,
    kIASRetNoSuchClass,
    kIASRetNoSuchAttribute,
    kIASRetUnsupported = 0xFF
};

enum IASValueTypes
{
    kIASValueMissing,
    kIASValueInteger,
    kIASValueNBytes,
    kIASValueString
};

enum IASCleanupFlags
{
    kIASAddedClass      = 0x01,
    kIASAddedAttribute  = 0x02,
    kIASDeleteClass     = kIASAddedClass,       // In other words, delete it if it was added (for cleanup purposes)
    kIASDeleteAttribute = kIASAddedAttribute    // In other words, delete it if it was added (for cleanup purposes)
};


class CBuffer;

// Classes

// --------------------------------------------------------------------------------------------------------------------
//                      TIASElement
// --------------------------------------------------------------------------------------------------------------------

class TIASElement : public OSObject
{
    OSDeclareDefaultStructors(TIASElement);
    
    public:
	    static TIASElement *tIASElement(ULong theValue);                                        // create with integer value 
	    static TIASElement *tIASElement(const UChar* theBytes, ULong length);                   // create with NBytes value 
	    static TIASElement *tIASElement(const UChar* theString, UChar charSet, ULong length);   // create with string 
	    static TIASElement *tIASElement(CBuffer* buffer);                                       // create from info in cbuffer 
	
	    void                free(void);

	    UByte               GetType();
	    IrDAErr             GetInteger(ULong *theValue);
	    IrDAErr             GetNBytes(UByte **theBytes, ULong *theLength);  // jdg
	    IrDAErr             GetString(UByte **theString, UByte *charSet, ULong *length);

	    void                AddInfoToBuffer(CBuffer* buffer);       // add element info to end of cbuffer

    private:
	    Boolean             init_with_long(ULong theValue);
	    Boolean             init_with_nbytes(const UChar* theBytes, ULong length);
	    Boolean             init_with_string(const UChar* theString, UChar charSet, ULong length);
	    Boolean             init_with_buffer(CBuffer* buffer);
	    
	    Boolean             SetInteger(ULong theValue);
	    Boolean             SetNBytes(const UChar* theBytes, ULong length);
	    Boolean             SetString(const UChar* theString, UChar charSet, ULong length);
	    Boolean             ExtractInfoFromBuffer(CBuffer* buffer);

	    UByte               type;
	    ULong               length;
	    UByte               characterSet;       // used to store the character set of a string
	    ULong               valueOrBytes;       // Used to store an integer value or up to 4 bytes -- in network order
	    UByte*              nameOrBytes;        // Used to store string or more than 4 bytes

};

// --------------------------------------------------------------------------------------------------------------------
//                      TIASNamedList
// --------------------------------------------------------------------------------------------------------------------

class TIASNamedList : public CList
{
	    OSDeclareDefaultStructors(TIASNamedList);
			
	    Boolean     Init(void);                     // make an unnamed, named list (sigh)
	    Boolean     Init(const UChar* name);        // make a copy of the name for this list
	    void        free(void);

	    void        *Search(const UChar* name);     // only valid if this CList contains other TIASNamedList 

    private:

	    // Fieldsä

	    UChar       *fName;
	    int         fNameLen;           // size of fName buffer
};

// --------------------------------------------------------------------------------------------------------------------
//                      TIASAttribute
// --------------------------------------------------------------------------------------------------------------------

// This is a list of TIASElement's

class TIASAttribute : public TIASNamedList
{
	    OSDeclareDefaultStructors(TIASAttribute);
    public:
	    static TIASAttribute * tIASAttribute(const UChar *name);        // create a named attribute
	    static TIASAttribute * tIASAttribute(CBuffer* buffer);          // create an unnamed attribute, fill from cbuffer


	    IrDAErr         Insert(TIASElement* element);
	    void            AddInfoToBuffer(CBuffer* buffer);
    private:
	    void            free(void);
	    Boolean         InitFromBuffer(CBuffer* buffer);

};

// --------------------------------------------------------------------------------------------------------------------
//                      TIASClass
// --------------------------------------------------------------------------------------------------------------------

// This is a list of TIASAttribute's

class TIASClass : public TIASNamedList
{
	    OSDeclareDefaultStructors(TIASClass);
	    
    public:
	    static TIASClass * tIASClass(const UChar *name);            // create with a name
	    
	    IrDAErr             Insert(TIASAttribute* element);
	    TIASAttribute*      FindAttribute(const UChar* attributeName);
    
    private:
	    void        free(void);

};

// --------------------------------------------------------------------------------------------------------------------
//                      TIASService
// --------------------------------------------------------------------------------------------------------------------

// This is a list of TIASClass's

class TIASService : public TIASNamedList
{
	    OSDeclareDefaultStructors(TIASService);
    public:
	    static TIASService * tIASService(void);


	    IrDAErr             AddIntegerEntry(const UChar* className, const UChar* attributeName, ULong intValue);
	    IrDAErr             AddStringEntry(const UChar* className, const UChar* attributeName, const UChar* stringValue, UChar charSet, ULong length);
	    IrDAErr             AddNBytesEntry(const UChar* className, const UChar* attributeName, const UChar* aFewBytes, ULong length);

	    TIASClass*          FindClass(const UChar* className);
	    TIASAttribute*      FindAttribute(const UChar* className, const UChar* attributeName);
	    IrDAErr             RemoveAttribute(const UChar* className, const UChar* attributeName, ULong flags = kIASDeleteClass | kIASDeleteAttribute);

    private:
	    void                free(void);

	    TIASClass*          AddClass(const UChar* className, ULong& flags);
	    TIASAttribute*      AddAttribute(const UChar* className, const UChar* attributeName, ULong& flags);
	    IrDAErr             AddAttributeEntry(const UChar* className, const UChar* attributeName, TIASElement* theEntry);
	    IrDAErr             RemoveClass(const UChar* className, ULong flags = kIASDeleteClass);

	    // Fieldsä

};

inline UByte TIASElement::GetType(void) { return type; };           // jdg


#endif // __IRIASSERVICE_H
