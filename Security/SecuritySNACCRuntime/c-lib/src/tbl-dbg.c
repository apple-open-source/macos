/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


#ifdef TTBL
#include "tbl-dbg.h"

TdeExceptionCode DBGMinCode = TDEINFO;

void DBGOcts PARAMS ((v),
   AsnOcts* v)
{
    int i;
    for (i = 0; i < v->octetLen; i++)
	fprintf (stdout, "%c", isprint(v->octs[i])?v->octs[i]:'.');
}

char*
Class2ClassStr PARAMS ((class),
    int class)
{
    switch (class)
    {
        case UNIV:
            return "UNIV";
            break;

        case APPL:
            return "APPL";
            break;

        case CNTX:
            return "CNTX";
            break;

        case PRIV:
            return "PRIV";
            break;

        default:
            return "UNKNOWN";
            break;
    }
}

char*
Form2FormStr PARAMS ((form),
    BER_FORM form)
{
    switch (form)
    {
        case PRIM:
            return "PRIM";
            break;

        case CONS:
            return "CONS";
            break;

        default:
            return "UNKNOWN";
            break;
    }
}

char*
Code2UnivCodeStr PARAMS ((code),
    BER_UNIV_CODE code)
{
    switch (code)
    {
      case BOOLEAN_TAG_CODE:
        return "BOOLEAN";
        break;

      case INTEGER_TAG_CODE:
        return "INTEGER";
        break;

      case BITSTRING_TAG_CODE:
        return "BIT STRING";
        break;

      case OCTETSTRING_TAG_CODE:
        return "OCTET STRING";
        break;

      case NULLTYPE_TAG_CODE:
        return "NULL";
        break;

      case OID_TAG_CODE:
        return "OBJECT IDENTIFIER";
        break;

      case OD_TAG_CODE:
        return "OD";
        break;

      case EXTERNAL_TAG_CODE:
        return "EXTERNAL";
        break;

      case REAL_TAG_CODE:
        return "REAL";
        break;

      case ENUM_TAG_CODE:
        return "ENUM";
        break;

      case SEQ_TAG_CODE:
        return "SEQUENCE";
        break;

      case SET_TAG_CODE:
        return "SET";
        break;

      case NUMERICSTRING_TAG_CODE:
        return "NUMERICSTRING";
        break;

      case PRINTABLESTRING_TAG_CODE:
        return "PRINTABLESTRING";
        break;

      case TELETEXSTRING_TAG_CODE:
        return "TELETEXSTRING";
        break;

      case VIDEOTEXSTRING_TAG_CODE:
        return "VIDEOTEXSTRING";
        break;

      case IA5STRING_TAG_CODE:
        return "IA5STRING";
        break;

      case UTCTIME_TAG_CODE:
        return "UTCTIME";
        break;

      case GENERALIZEDTIME_TAG_CODE:
        return "GENERALIZEDTIME";
        break;

      case GRAPHICSTRING_TAG_CODE:
        return "GRAPHICSTRING";
        break;

      case VISIBLESTRING_TAG_CODE:
        return "VISIBLESTRING";
        break;

      case GENERALSTRING_TAG_CODE:
        return "GENERALSTRING";
        break;

      default:
        return "UNKNOWN";

    }
} /* Form2FormStr */

#define SOT 0
#define EOL 1
#define EOLINC 2
#define DECSOTEOL 3
#define SOTSPC 4
#define EOLIF 5

void DBGIndent PARAMS ((mode),
    int mode)
{
    static int indent = 0;
    static int withinline = 0;
    int i;
    
    /*DEC*/
    if (mode==DECSOTEOL)
    	indent--;

    /*SPC*/
    if (mode==SOTSPC && withinline)
    	fprintf(stdout," ");

    /*SOT*/
    if ((mode==SOT || mode==DECSOTEOL || mode==SOTSPC) && !withinline)
    	{
    	for (i=0; i<indent; i++)
    	    fprintf(stdout,"   ");
    	withinline = 1;
    	}

    /*IF*/
    if (mode==EOLIF && withinline)
	fprintf(stdout,"\n");

    /*EOL*/
    if (mode==EOL || mode==EOLINC || mode==DECSOTEOL || mode==EOLIF)
    	withinline = 0;
    
    /*INC*/
    if (mode==EOLINC)
    	indent++;
}

int DBGSimple PARAMS ((tag, v, begin),
    AsnTag tag _AND_
    AsnOcts* v _AND_
    int begin)
{
    BER_CLASS tclass = TAG_ID_CLASS(tag);
    BER_FORM form = TAG_ID_FORM(tag);
    unsigned long int code = tag & 0x1FFFFFFF;
    BER_UNIV_CODE bcode;
    char* codename;
    int i;
    if (tclass==UNIV)
	{
	bcode = code>>24;
	codename = Code2UnivCodeStr(bcode);
	}
    else
	{
	bcode = OCTETSTRING_TAG_CODE;
	codename = "NOT_UNIV";
	}
    if (begin)
    	{
    	DBGIndent(SOTSPC);
	fprintf (stdout, "%s", codename);
	if (TAG_IS_CONS(tag))
	    {
	    if (tclass==UNIV) 
		{
    		fprintf(stdout, " {\n");
		DBGIndent(EOLINC);
		}
    	    }
    	else
    	    {
    	    fprintf(stdout,": ");
    	    PrintAsnOcts(stdout,v,0);
	    fprintf(stdout,"\n");
	    DBGIndent(EOL);
    	    }
	}
    else
    	{
	if (TAG_IS_CONS(tag) && tclass==UNIV)
    	    {
    	    DBGIndent(DECSOTEOL);
    	    fprintf(stdout, "}\n");
    	    }
    	}
    return 0;
}

void DBGNamedValue PARAMS ((tnnl, val, mode),
    TBLNamedNumberList* tnnl _AND_
    AsnInt val _AND_
    int mode)
{
    /* mode 0: Don't print if no named value. postfix print with -- */
    /* mode 0|1: prefix with -- */
    /* mode >1: prefix with , */
    TBLNamedNumber* tnn;
    char* name = NULL;
    FOR_EACH_LIST_ELMT (tnn, tnnl)
	if (tnn->value == val)
	    {
	    name = tnn->name.octs;
	    break;
	    }
    if (!mode && !name)
	return;
    if (mode<=1)
	fprintf(stdout," -- ");
    else
	fprintf(stdout,", ");
    if (name)
	fprintf(stdout,"%s",name);
    fprintf(stdout,"(%d)",val);
    if (!mode)
    	fprintf(stdout," --");
}

void
DBGPrintType PARAMS ((type),
    TBLType* type)
{
    static char* TIN [] = { "BOOLEAN", "INTEGER", "BIT STRING", "OCTET STRING",
    	"NULL", "OBJECT IDENTIFIER", "REAL", "ENUMERATED", "SEQUENCE", "SET", 
    	"SEQUENCE OF", "SET OF", "CHOICE", NULL };
    	
    if (type->typeId == TBL_TYPEREF)
	DBGOcts(&type->content->a.typeRef->typeDefPtr->typeName);
    else
	fprintf(stdout,TIN[type->typeId]);
    if (type->fieldName.octetLen)
    {
	fprintf(stdout," ");
	DBGOcts(&type->fieldName);
    }
}

typedef int (*Proc) PROTO (());

int
DBGType PARAMS ((type, val, begin),
    TBLType* type _AND_
    AVal* val _AND_
    int begin)
{
    static Proc printproc [] = {PrintAsnBool, PrintAsnInt, PrintAsnBits,
    PrintAsnOcts,
            PrintAsnNull, PrintAsnOid, PrintAsnReal, PrintAsnInt, NULL, NULL,
            NULL, NULL, NULL,
            NULL};
    
    if (begin)
    {
	DBGIndent(SOTSPC);
	DBGPrintType(type);
	if (type->typeId >= TBL_SEQUENCE && type->typeId <= TBL_CHOICE)
    	{
    	    fprintf(stdout," {\n");
    	    DBGIndent(EOLINC);
    	}
    }
    else
    {
	if (printproc[type->typeId])
	{
    	    DBGIndent(SOT);
    	    fprintf(stdout,": ");
    	    (*printproc[type->typeId])(stdout,val,0);
	    switch (type->typeId)
	    {
	    case TBL_BITSTRING:
		{
		AsnInt i;
		AsnBits* b = (AsnBits*)val;
		int mode = 1;
		for (i=0; i<b->bitLen;i++)
		    if (GetAsnBit(b,i))
			DBGNamedValue(type->values,i,mode++);
		if (mode>1)
	    	    fprintf(stdout," --");
		}
		break;
	    case TBL_ENUMERATED:
		DBGNamedValue(type->values,*(AsnInt*)val,0);
		break;
	    default:
		break;
	    }
	    fprintf(stdout,"\n");
	    DBGIndent(EOL);
	}
	if (type->typeId >= TBL_SEQUENCE && type->typeId <= TBL_CHOICE)
    	{
    	    DBGIndent(DECSOTEOL);
    	    fprintf(stdout,"}\n");
    	}
    }
    return 0;
}

int DBGExc PARAMS ((code, p1, p2, p3),
    TdeExceptionCode code _AND_
    void* p1 _AND_
    void* p2 _AND_
    void* p3)
{
    if (code<DBGMinCode)
	return 0;
    switch (code) 
    {
    case TDEEOC:
	DBGIndent(SOTSPC);
	fprintf(stdout,"[EOC]");
	break;
    case TDEPEEKTAG:
	DBGIndent(SOTSPC);
	fprintf(stdout,"[%08X]",*(AsnTag*)p1);
	break;
    case TDEPUSHTAG:
	DBGIndent(SOTSPC);
	fprintf(stdout,"[%08X/%d(%d)]",*(AsnTag*)p1,*(int*)p2,*(int*)p3);
	break;
    case TDEUNEXPECTED:
	DBGIndent(EOLIF);
	DBGIndent(SOT);
	fprintf(stdout,"WARNING: Unexpected type encountered");
	if (p2) {
	    fprintf(stdout," when expecting ");
	    DBGPrintType((TBLType*)p2);
	}
	fprintf(stdout," while decoding ");
	DBGPrintType((TBLType*)p1);
	fprintf(stdout,", decoding it in simple mode.\n");
	DBGIndent(EOL);
	break;
    case TDENONOPTIONAL:
	DBGIndent(EOLIF);
	DBGIndent(SOT);
	fprintf(stdout,"WARNING: Non-optional element ");
	DBGPrintType((TBLType*)p2);
	fprintf(stdout," missing in ");
	DBGPrintType((TBLType*)p1);
	fprintf(stdout,".\n");
	DBGIndent(EOL);
	break;
    case TDEMANDATORY:
	DBGIndent(EOLIF);
	DBGIndent(SOT);
	fprintf(stdout,"WARNING: Mandatory elements missing in ");
	DBGPrintType((TBLType*)p1);
	fprintf(stdout,".\n");
	DBGIndent(EOL);
	break;
    case TDECONSTRAINT:
	DBGIndent(EOLIF);
	DBGIndent(SOT);
	fprintf(stdout,"WARNING: Value %d not in SIZE(%d..%d) in ",
		*(AsnInt*)p3,((TBLRange*)p2)->from,((TBLRange*)p2)->to);
	DBGPrintType((TBLType*)p1);
	fprintf(stdout,".\n");
	DBGIndent(EOL);
	break;
    case TDENOMATCH:
	DBGIndent(EOLIF);
	DBGIndent(SOT);
	fprintf(stdout,"WARNING: Tag [%08X] does not match tag [%08X] of type ",
		*(AsnTag*)p3,*(AsnTag*)p2);
	DBGPrintType((TBLType*)p1);
	fprintf(stdout,".\n");
	DBGIndent(EOL);
	break;
    case TDEERROR:
	DBGIndent(EOLIF);
	DBGIndent(SOT);
	fprintf(stdout,"ERROR: %s.\n",(char*)p1);
	DBGIndent(EOL);
	break;
    }
    return 0;
}
#endif
