/*
 * asn1Templates.c - Common ASN1 templates for use with libNSSDer.
 */

#include <secasn1.h>

/*
 * Generic templates for individual/simple items and pointers to
 * and sets of same.
 *
 * If you need to add a new one, please note the following:
 *	 - For each new basic type you should add *four* templates:
 *	one plain, one PointerTo, one SequenceOf and one SetOf.
 *	 - If the new type can be constructed (meaning, it is a
 *	*string* type according to BER/DER rules), then you should
 *	or-in SEC_ASN1_MAY_STREAM to the type in the basic template.
 *	See the definition of the OctetString template for an example.
 *	 - It may not be obvious, but these are in *alphabetical*
 *	order based on the SEC_ASN1_XXX name; so put new ones in
 *	the appropriate place.
 */

const SEC_ASN1Template SEC_AnyTemplate[] = {
    { SEC_ASN1_ANY | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem) }
};

const SEC_ASN1Template SEC_PointerToAnyTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_AnyTemplate }
};

const SEC_ASN1Template SEC_SequenceOfAnyTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_AnyTemplate }
};

const SEC_ASN1Template SEC_SetOfAnyTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_AnyTemplate }
};

const SEC_ASN1Template SEC_BitStringTemplate[] = {
    { SEC_ASN1_BIT_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem) }
};

const SEC_ASN1Template SEC_PointerToBitStringTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_BitStringTemplate }
};

const SEC_ASN1Template SEC_SequenceOfBitStringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_BitStringTemplate }
};

const SEC_ASN1Template SEC_SetOfBitStringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_BitStringTemplate }
};

const SEC_ASN1Template SEC_BMPStringTemplate[] = {
    { SEC_ASN1_BMP_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem) }
};

const SEC_ASN1Template SEC_PointerToBMPStringTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_BMPStringTemplate }
};

const SEC_ASN1Template SEC_SequenceOfBMPStringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_BMPStringTemplate }
};

const SEC_ASN1Template SEC_SetOfBMPStringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_BMPStringTemplate }
};

const SEC_ASN1Template SEC_BooleanTemplate[] = {
    { SEC_ASN1_BOOLEAN, 0, NULL, sizeof(SECItem) }
};

const SEC_ASN1Template SEC_PointerToBooleanTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_BooleanTemplate }
};

const SEC_ASN1Template SEC_SequenceOfBooleanTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_BooleanTemplate }
};

const SEC_ASN1Template SEC_SetOfBooleanTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_BooleanTemplate }
};

const SEC_ASN1Template SEC_EnumeratedTemplate[] = {
    { SEC_ASN1_ENUMERATED, 0, NULL, sizeof(SECItem) }
};

const SEC_ASN1Template SEC_PointerToEnumeratedTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_EnumeratedTemplate }
};

const SEC_ASN1Template SEC_SequenceOfEnumeratedTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_EnumeratedTemplate }
};

const SEC_ASN1Template SEC_SetOfEnumeratedTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_EnumeratedTemplate }
};

const SEC_ASN1Template SEC_GeneralizedTimeTemplate[] = {
    { SEC_ASN1_GENERALIZED_TIME | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem)}
};

const SEC_ASN1Template SEC_PointerToGeneralizedTimeTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_GeneralizedTimeTemplate }
};

const SEC_ASN1Template SEC_SequenceOfGeneralizedTimeTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_GeneralizedTimeTemplate }
};

const SEC_ASN1Template SEC_SetOfGeneralizedTimeTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_GeneralizedTimeTemplate }
};

const SEC_ASN1Template SEC_IA5StringTemplate[] = {
    { SEC_ASN1_IA5_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem) }
};

const SEC_ASN1Template SEC_PointerToIA5StringTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_IA5StringTemplate }
};

const SEC_ASN1Template SEC_SequenceOfIA5StringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_IA5StringTemplate }
};

const SEC_ASN1Template SEC_SetOfIA5StringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_IA5StringTemplate }
};

const SEC_ASN1Template SEC_IntegerTemplate[] = {
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT, 0, NULL, sizeof(SECItem) }
};

const SEC_ASN1Template SEC_UnsignedIntegerTemplate[] = {
    { SEC_ASN1_INTEGER, 0, NULL, sizeof(SECItem) }
};

const SEC_ASN1Template SEC_PointerToIntegerTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_IntegerTemplate }
};

const SEC_ASN1Template SEC_SequenceOfIntegerTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_IntegerTemplate }
};

const SEC_ASN1Template SEC_SetOfIntegerTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_IntegerTemplate }
};

const SEC_ASN1Template SEC_NullTemplate[] = {
    { SEC_ASN1_NULL, 0, NULL, sizeof(SECItem) }
};

const SEC_ASN1Template SEC_PointerToNullTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_NullTemplate }
};

const SEC_ASN1Template SEC_SequenceOfNullTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_NullTemplate }
};

const SEC_ASN1Template SEC_SetOfNullTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_NullTemplate }
};

const SEC_ASN1Template SEC_ObjectIDTemplate[] = {
    { SEC_ASN1_OBJECT_ID, 0, NULL, sizeof(SECItem) }
};

const SEC_ASN1Template SEC_PointerToObjectIDTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_ObjectIDTemplate }
};

const SEC_ASN1Template SEC_SequenceOfObjectIDTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_ObjectIDTemplate }
};

const SEC_ASN1Template SEC_SetOfObjectIDTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_ObjectIDTemplate }
};

const SEC_ASN1Template SEC_OctetStringTemplate[] = {
    { SEC_ASN1_OCTET_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem) }
};

const SEC_ASN1Template SEC_PointerToOctetStringTemplate[] = {
    { SEC_ASN1_POINTER | SEC_ASN1_MAY_STREAM, 0, SEC_OctetStringTemplate }
};

const SEC_ASN1Template SEC_SequenceOfOctetStringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_OctetStringTemplate }
};

const SEC_ASN1Template SEC_SetOfOctetStringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_OctetStringTemplate }
};

const SEC_ASN1Template SEC_PrintableStringTemplate[] = {
    { SEC_ASN1_PRINTABLE_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem)}
};

const SEC_ASN1Template SEC_PointerToPrintableStringTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_PrintableStringTemplate }
};

const SEC_ASN1Template SEC_SequenceOfPrintableStringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_PrintableStringTemplate }
};

const SEC_ASN1Template SEC_SetOfPrintableStringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_PrintableStringTemplate }
};

#ifdef	__APPLE__
const SEC_ASN1Template SEC_TeletexStringTemplate[] = {
    { SEC_ASN1_TELETEX_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem)}
};

const SEC_ASN1Template SEC_PointerToTeletexStringTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_TeletexStringTemplate }
};

const SEC_ASN1Template SEC_SequenceOfTeletexStringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_TeletexStringTemplate }
};

const SEC_ASN1Template SEC_SetOfTeletexStringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_TeletexStringTemplate }
};
#endif	/* __APPLE__ */

const SEC_ASN1Template SEC_T61StringTemplate[] = {
    { SEC_ASN1_T61_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem) }
};

const SEC_ASN1Template SEC_PointerToT61StringTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_T61StringTemplate }
};

const SEC_ASN1Template SEC_SequenceOfT61StringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_T61StringTemplate }
};

const SEC_ASN1Template SEC_SetOfT61StringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_T61StringTemplate }
};

const SEC_ASN1Template SEC_UniversalStringTemplate[] = {
    { SEC_ASN1_UNIVERSAL_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem)}
};

const SEC_ASN1Template SEC_PointerToUniversalStringTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_UniversalStringTemplate }
};

const SEC_ASN1Template SEC_SequenceOfUniversalStringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_UniversalStringTemplate }
};

const SEC_ASN1Template SEC_SetOfUniversalStringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_UniversalStringTemplate }
};

const SEC_ASN1Template SEC_UTCTimeTemplate[] = {
    { SEC_ASN1_UTC_TIME | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem) }
};

const SEC_ASN1Template SEC_PointerToUTCTimeTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_UTCTimeTemplate }
};

const SEC_ASN1Template SEC_SequenceOfUTCTimeTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_UTCTimeTemplate }
};

const SEC_ASN1Template SEC_SetOfUTCTimeTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_UTCTimeTemplate }
};

const SEC_ASN1Template SEC_UTF8StringTemplate[] = {
    { SEC_ASN1_UTF8_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem)}
};

const SEC_ASN1Template SEC_PointerToUTF8StringTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_UTF8StringTemplate }
};

const SEC_ASN1Template SEC_SequenceOfUTF8StringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_UTF8StringTemplate }
};

const SEC_ASN1Template SEC_SetOfUTF8StringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_UTF8StringTemplate }
};

const SEC_ASN1Template SEC_VisibleStringTemplate[] = {
    { SEC_ASN1_VISIBLE_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem) }
};

const SEC_ASN1Template SEC_PointerToVisibleStringTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_VisibleStringTemplate }
};

const SEC_ASN1Template SEC_SequenceOfVisibleStringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_VisibleStringTemplate }
};

const SEC_ASN1Template SEC_SetOfVisibleStringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_VisibleStringTemplate }
};


/*
 * Template for skipping a subitem.
 *
 * Note that it only makes sense to use this for decoding (when you want
 * to decode something where you are only interested in one or two of
 * the fields); you cannot encode a SKIP!
 */
const SEC_ASN1Template SEC_SkipTemplate[] = {
    { SEC_ASN1_SKIP }
};


/* These functions simply return the address of the above-declared templates.
** This is necessary for Windows DLLs.  Sigh.
**
** FIXME - may not be needed for OS X. TBD.
*/
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_AnyTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_BMPStringTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_BooleanTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_BitStringTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_IA5StringTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_GeneralizedTimeTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_IntegerTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_NullTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_ObjectIDTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_OctetStringTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_PointerToAnyTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_PointerToOctetStringTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_SetOfAnyTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_UTCTimeTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_UTF8StringTemplate)

