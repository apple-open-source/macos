/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                                   MacsBug_utils.c                                    |
 |                                                                                      |
 |                            MacsBug Utility Plugin Commands                           |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2006                       |
 |                                                                                      |
 *--------------------------------------------------------------------------------------*

 This file contains all of "low level" plugins that may be needed by the commands written
 in gdb command language or other higher level plugins. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <setjmp.h>

#include "MacsBug.h"

/*--------------------------------------------------------------------------------------*/

int branchTaken = 0;				/* !=0 if branch [not] taken in disasm	*/

static char curr_function1[1025] = {0};		/* current function being disassembled	*/
static char curr_function2[1025] = {0};		/* ditto but for the pc area		*/

char *default_help = "For internal use only -- do not use.";

/*--------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------*
 | __asciidump addr n - dump n bytes starting at addr in ASCII |
 *-------------------------------------------------------------*/
 
void __asciidump(char *arg, int from_tty)
{
    int  	  argc, i, k, n, offset, repeated, repcnt;
    GDB_ADDRESS   addr;
    char 	  *argv[5], tmpCmdLine[1024], addrexp[1024];
    unsigned char *c, x, data[65], prev[64];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__asciidump", &argc, argv, 4);
    if (argc != 3)
    	gdb_error("__asciidump called with wrong number of arguments");
    	
    n = gdb_get_int(argv[2]);
    
    for (repcnt = offset = 0; offset < n; offset += 64) {
    	sprintf(addrexp, "(char *)(%s)+%ld", argv[1], offset);
	
    	if (offset + 64 < n) {
    	    addr     = gdb_read_memory(data, addrexp, 64);
    	    repeated = ditto && (memcmp(data, prev, 64) == 0);
    	} else {
    	    addr     = gdb_read_memory(data, addrexp, n - offset);
    	    repeated = 0;
    	}
    	
    	memcpy(prev, data, 64);

    	if (repeated) {
    	    if (repcnt++ == 0)
    	    	gdb_printf(" %.8llX: ''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''\n", (long long)addr);
    	} else {
    	    repcnt = 0;
    	    gdb_printf(" %.8llX: ", (long long)addr);
    	                
            for (k = i = 0; i < 64; ++i) {
            	if (offset + k < n) {
            	    x = data[k];
            	    data[k++] = isprint((int)x) ? (unsigned char)x : (unsigned char)'.';
            	}
            }
            
	    data[k] = '\0';
	    gdb_printf("%s\n", data);
    	}
    }
}

#define __ASCIIDUMP_HELP \
"__ASCIIDUMP addr n -- Dump n bytes starting at addr in ASCII."


/*-------------------------------------------------------------------*
 | __binary value n - display right-most n bits of a value in binary |
 *-------------------------------------------------------------------*/
 
void __binary(char *arg, int from_tty)
{
    int  	       argc, n, i;
    unsigned long long value;
    char 	       *argv[5], tmpCmdLine[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__binary", &argc, argv, 4);
    
    if (argc != 3)
    	gdb_error("__binary called with wrong number of arguments");
    	
    value = gdb_get_long_long(argv[1]);
    n     = gdb_get_int(argv[2]);
    
    i = 0;
    while (i++ < n)
    	gdb_printf("%1d", (int)((value >> (n-i)) & 1));
}

#define __BINARY_HELP \
"__BINARY value n -- Display right-most n bits of value in binary.\n" \
"No newline is output after the display."


/*-----------------------------------------------------------------------------*
 | __command_exists cmd - return 1 if the specified cmd exists and 0 otherwise |
 *-----------------------------------------------------------------------------*/
 
static void __command_exists(char *arg, int from_tty)
{
    gdb_set_int("$exists", gdb_is_command_defined(arg));
}

#define __COMMAND_EXISTS_HELP \
"__COMMAND_EXISTS cmd -- See if specified cmd exists.\n"				\
"Set $exists to 1 if the command exists and 0 otherwise."


#ifndef not_ready_for_prime_time_but_keep_enabled_for_private_testing
/*---------------------------------------------------------------------------------------*
 | is_printable_string - see if pointer points to a string with all printable characters |
 *---------------------------------------------------------------------------------------*
 
 Returns 0 if addr does NOT point to a printable string.  Otherwise the length of
 theString is returned along with the string itself in theString (up to maxLen bytes,
 plus 1 for the '\0' at the end).  The string is formatted with quote delimiters and a
 truncation indications ("...") if it needs to be truncated to maxLen.
  
 The strings pointed to by addr can be either C strings or Pascal strings (strings
 preceded by a length byte).  The returned strType indicates which.  It is typed as
 follows:
 											*/
 typedef enum {					/* type status of strings:		*/
     UNKNOWN_STRING_TYPE,			/*   not known, figure it out		*/
     IS_PSTRING,				/*   have a Pascal string		*/
     IS_CSTRING,				/*   have a C string			*/
     IS_RB_PSTRING				/*   guessing it's a RealBasic pstring	*/ 
 } String_Type;
 											/*
 Normally strType is passed set with UNKNOWN_STRING_TYPE and it is returned as IS_PSTRING
 or IS_CSTRING.  But if the caller already knows the expected type (e.g., RealBasic
 string pointers are IS_PSTRING or IS_RB_PSTRING) then strType can be preset to the
 desired type and that is the way the string at addr will be treated.
 
 Pascal strings are only accepted if addr points to either the "__TEXT.__cstring",
 "__DATA.__data", or "__TEXT.__const_text" object file sections in an attempt to 
 minimize accepting printable "garbage".
 
 Note that there can be ambiguity when looking at a string and trying to decide whether
 it is a C string or a Pascal string.  For example, what if the first byte of the string
 happens to be equal to the length of the string following that byte?  To answer that
 this code "guesses" by applying some heuristics.  Of course it is not always correct!
*/

static int is_printable_string(GDB_ADDRESS addr, String_Type *strType, char *theString,
			       int maxLen)
{
    char          c, l, r, quote, *p;
    unsigned char p_stringLen;
    int           i, n, c_stringLen, in_cstring_section;
    GDB_ADDRESS   addr0 = addr;
    
    static char tmpString[1025];
 
    c_stringLen = 0;

    if (*strType == IS_PSTRING || *strType == IS_RB_PSTRING)
    	goto pstring;
    	
    /* Check for a printable C string...						*/
    
    p = theString;
    
    while (c_stringLen < maxLen && gdb_read_memory_from_addr(&c, addr++, 1, 0) && c != '\0') {
	if (!isprint(c) && c != '\n' && c != '\t' && c != '\r' && 
	    !((unsigned char)c >= 0xA8 && (unsigned char)c <= 0xAA /* ¨ © ª */)) {
	    c_stringLen = 0;
	    break;
	}
	++c_stringLen;
	*p++ = c;
    }
    
    *p = '\0';
    
    if (*strType == IS_CSTRING)
    	return (c_stringLen);
    
    if (c_stringLen > 0) {
    	*strType = IS_CSTRING;
    	
    	/* We think it's a good cstring but check for a possible pstring length byte...	*/
    	
    	c = *(unsigned char *)theString;
    	if (c != (c_stringLen - 1) && (c == ' ' || !isspace(c)))
    	    return (c_stringLen);

	/* Possible pstring.  But if the 1st byte is any of the following then assume	*/
	/* we still have a cstring (of course this algorithm is not perfect):		*/
    	
    	/*   Letters and digits.							*/
	
    	/*   isspace() chararacters (like '\n', '\t', '\r').				*/
    	
    	/*   '/' and there are no other '/'s in the string (assumed not a pathname).	*/
    	
    	/*   '(', '<', '[', '{', single and double quotes with their matching right	*/
    	/*   character.  								*/
    	
    	/*   Any other punctuation character is treated as a potential Pascal length	*/
    	/*   byte.									*/ 

    	if (c == ' ' || !isspace(c)) {		/* isspace() ==> could be pascal string	*/
	    if (!ispunct(c))			/* letters and digits ==> C string	*/
		return (c_stringLen);
		
	    switch (c) {			/* handle the punctuation characters...	*/
	    	case '/':			/* '/' ==> C string if more '/'s	*/
	    	    if (strchr(&theString[1], '/') != NULL)
	    	    	return (c_stringLen);
	    	    break;
	    	
	    	case '(': r = ')'; goto match_it;/* '(' ==> C string if matching ')'	*/
	    	case '<': r = '>'; goto match_it;/* '<' ==> C string if matching '>'	*/
	    	case '[': r = ']'; goto match_it;/* '[' ==> C string if matching ']'	*/
	    	case '{': r = '}';		 /* '{' ==> C string if matching '}'	*/
	    	    match_it:			 /* ignore matches within strings	*/
	    	    	for (l = theString[0], n = 1, quote = 0, p = &theString[1]; *p; ++p) {
	    	    	    if (*p == l) {
	    	    	    	if (!quote)
	    	    	    	    ++n;
	    	    	    } else if (*p == r) {
	    	    	    	if (!quote && --n == 0)
	    	    	    	    return (c_stringLen);
	    	    	    } else if (*p == '"')
	    	    	    	quote = (quote == '"') ? 0 : '"';
	    	    	    else if (*p == '\'')
	    	    	    	quote = (quote == '\'') ? 0 : '\'';
	    	    	}
	    	    	break;
	    	
	    	case '"': r = '"'; goto match_quote; /* " ==> C string if matching "	*/
	    	case '\'': r = '\'';		     /* ' ==> C string if matching '	*/
	    	    match_quote:
	    	    	for (l = theString[0], n = 1, p = &theString[1]; *p; ++p) {
	    	    	    if (*p == l)
	    	    	    	++n;
	    	    	    else if (*p == r && --n == 0)
				return (c_stringLen);
	    	    	}
	    	    	break;
	    	    	    
	        default:			/* all other punctuation		*/
	            break;  			/*		==> possible length byte*/
	    }
    	}
    }

    pstring:					/* handle Pascal strings below here	*/
        
    /* Check for printable Pascal string only if it's in the "__TEXT.__cstring",	*/
    /* "__TEXT.__const_text", or "__DATA.__data" sections to limit the potential for	*/
    /* displaying printable garbage.  However, for the possible case of a dynamically	*/
    /* created RealBasic string we allow those anywhere.				*/
    
    if (*strType != IS_RB_PSTRING) {
	in_cstring_section = (gdb_is_addr_in_section(addr0, "__TEXT.__cstring") != NULL);
	
	if (!in_cstring_section &&
	    !gdb_is_addr_in_section(addr0, "__DATA.__data") &&
	    !gdb_is_addr_in_section(addr0, "__TEXT.__const_text")) {
	    return (c_stringLen);
	}
    }
    
    /* Check for a pstring.  If we still don't think it was a Pascal string, then fall	*/
    /* back to treating it as a C string since we can get here either two ways; it 	*/
    /* wasn't a C string either, or the above heuristic thought it could be a pstring.  */
    /* There is the case were we expect it to be a Pascal string because it is from a 	*/
    /* pointer in a __basicstring section.  						*/
    
    addr = addr0;
    p    = theString;
    
    if (!gdb_read_memory_from_addr(&p_stringLen, addr++, 1, 0) || p_stringLen == 0)
    	return (c_stringLen);
            
    for (i = 0; i < p_stringLen; ++i) {
    	if (!gdb_read_memory_from_addr(&c, addr++, 1, 0))
    	    return (c_stringLen);
    	if (!isprint(c) && c != '\n' && c != '\t' && c != '\r' &&
    	    !((unsigned char)c >= 0xA8 && (unsigned char)c <= 0xAA /* ¨ © ª */))
    	    return (c_stringLen);
    	*p++ = c;
    }
    
    *p = '\0';

    /* If we get this far then we think we have a Pascal.  But if c_stringLen > 0 and	*/
    /* the length byte for the Pascal string is '\n', '\t', or '\r' we still can't be	*/
    /* sure we really have a Pascal string.  So apply yet another heuristic.		*/
  
    /* If the string is in the "__TEXT.__cstring" section we'll assume it's a C string.	*/
    /* We're being conservative here (and still possibly wrong) in assuming these 	*/
    /* strings are for print calls.							*/
    
    /* If the string is anywhere else we'll assume it's a Pascal string UNLESS a C 	*/
    /* string immediately follows the and the concatenated length of both strings is 	*/
    /* equal to the original c_stringLen.  We again assume we have a C string that just */
    /* happens to start with a '\n', '\t', or '\r'.					*/
    
    /* We can be wrong with these assumptions.  A C string immediately following a 	*/
    /* Pascal string could be just that.  In that case we will be showing too much.	*/
    /* Also, a Pascal string that just happens to be in the C string section will show	*/
    /* as a C string with the length byte showing as a '\n', '\t', or '\r' character.   */
    /* Of course this isn't as bad as showing too much.  But it's the best we can do.	*/

    if (*strType != IS_PSTRING && *strType != IS_RB_PSTRING && c_stringLen > 0 &&
    	(p_stringLen == '\n' || p_stringLen == '\t' || p_stringLen == '\r')) {
	if (in_cstring_section)
	    return (c_stringLen);
	*strType != UNKNOWN_STRING_TYPE;
	if ((i = is_printable_string(addr, strType, tmpString, 1023)) &&
	    *strType == IS_CSTRING && (p_stringLen + i + 1) == c_stringLen)
	    return (c_stringLen);
    }
    
    *strType = IS_PSTRING;
    return (p_stringLen);
}


/*------------------------------------------------------------------*
 | is_objc_class - see if we have a pointer to a Objective C object |
 *------------------------------------------------------------------*
 
 If addr points to an object then the name of the class to which the object belongs
 is returned in theString (possibly truncated to maxLen characters) and its length
 as the function result.  0 is returned if we don't think the addr points to an
 object.
 
 According to the "Objective-C Runtime Reference" an object (instance of a class)
 looks like the following:
 
   struct objc_object {
       struct objc_class *isa;
       ...variable length data containing instance variable values...
   };
   
 So if addr points to one of these the first field should be a pointer to a class
 definition:
 
   struct objc_class {
       struct objc_class         *isa;
       struct objc_class         *super_class;
       const char                *name;
       long                      version;
       long                      info;
       long                      instance_size;
       struct objc_ivar_list     *ivars;
       struct objc_method_list   **methodLists;
       struct objc_cache         *cache;
       struct objc_protocol_list *protocols;
   };
    
 If we have a printable C string pointed to by the name field then we'll assume addr
 was indeed an instance variable and theString will be returned formatted as follows:
 
   <name: addr>
   
 where, name is the class name and addr the original addr.
 
 Note this is what "_NSPrintForDebugger" and "_CFPrintForDebugger" do by default.  What
 we do here is mainly to test to see if addr is actually a pointer to a possible class
 object.  gdb_show_objc_object() is available to display Objective C class objects. It,
 like gdb's PRINT-OBJECT, actually does call "_NSPrintForDebugger" or
 "_CFPrintForDebugger".
*/

static int is_objc_class(GDB_ADDRESS addr, char *theString, int maxLen)
{
    int           nameLen = 0;
    unsigned long object_isa, class_isa, name;
    String_Type   strType;
    
    if (gdb_read_memory_from_addr(&object_isa, addr, 4, 0) && object_isa != 0)
	if (gdb_read_memory_from_addr(&class_isa, object_isa, 4, 0) && class_isa != 0)
    	    if (gdb_read_memory_from_addr(&name, object_isa + 8, 4, 0) && name != 0) {
    	    	strType = IS_CSTRING;
    	    	nameLen = is_printable_string((GDB_ADDRESS)name, &strType, 
    	    				       theString + 1, maxLen - 14);
    	    	if (nameLen > 0) {
    	    	    *theString = '<';		/* <name: 0x12345678>			*/
    	    	    theString += ++nameLen;
    	    	    *theString++ = ':';
    	    	    *theString++ = ' ';
    	    	    nameLen += 2 + sprintf(theString, "0x%llx>", addr);
    	    	}
    	    }
    	    
    return (nameLen);
}


/*-------------------------------------------------------------------------------*
 | is_const_string_object - see if we have a pointer to a constant string object |
 *-------------------------------------------------------------------------------*
 
 Returns 0 if addr points to an Objective C constant string object.  Otherwise the
 length of the string is returned along with the string itself in theString (up to
 maxLen bytes, plus 1 for the '\0' at the end).
  
 Both C and Pascal (is that possible?) string objects are checked for.  The strType is
 returned indicating which (IS_PSTRING or IS_CSTRING -- see is_printable_string()).
*/

static int is_const_string_object(GDB_ADDRESS addr, String_Type *strType, 
				  char *theString, int maxLen)
{
    int           len, len1;
    unsigned long addr4;

    /* Two forms of string objects are handled:						*/

    /*   typedef struct {                         typedef struct {			*/
    /*       void *isa;                               void *isa;			*/
    /*       char *cstring;                           long encoding;			*/
    /*       long len;                                char *cstring;			*/
    /*   } _NSConstantStringClassReference;           long len;				*/
    /*                                            } __CFConstantStringClassReference;	*/  
    
    /* Check out the isa to see if it's a pointer to a class...				*/
    
    if (is_objc_class(addr, theString, maxLen) == 0)
    	return (0);
    
    /* See if we have the _NSConstantStringClassReference...				*/
    
    if (gdb_read_memory_from_addr(&addr4, addr + 4, 4, 0) &&
    	gdb_read_memory_from_addr(&len1,  addr + 8, 4, 0) &&
    	(gdb_is_addr_in_section((GDB_ADDRESS)addr4, "__TEXT.__cstring") ||
    	 gdb_is_addr_in_section((GDB_ADDRESS)addr4, "__DATA.__data"   ))) {
    	*strType = UNKNOWN_STRING_TYPE;		/* allow C and Pascal strings (?)	*/
	len = is_printable_string((GDB_ADDRESS)addr4, strType, theString, maxLen);
	if ((len + (*strType == IS_PSTRING)) == len1)
	    return (len);
    }
    
    /* See if we have the __CFConstantStringClassReference...				*/

    if (gdb_read_memory_from_addr(&addr4, addr +  8, 4, 0) &&
    	gdb_read_memory_from_addr(&len1,  addr + 12, 4, 0) &&
    	(gdb_is_addr_in_section((GDB_ADDRESS)addr4, "__TEXT.__cstring") ||
    	 gdb_is_addr_in_section((GDB_ADDRESS)addr4, "__DATA.__data"   ))) {
    	*strType = UNKNOWN_STRING_TYPE;		/* allow C and Pascal strings (?)	*/
	len = is_printable_string((GDB_ADDRESS)addr4, strType, theString, maxLen);
	if ((len + (*strType == IS_PSTRING)) == len1)
	    return (len);
    }
    
    return (0);
}


/*------------------------------------------------------------------------------*
 | string_for_ptr - see if there is a string that can be derived from a pointer |
 *------------------------------------------------------------------------------*

 The specified address (addr) is used to see if it points to a string, Objective C
 string object, or RealBasic string.  If so, theString will contain what was found
 (up to maxLen characters) and the string's length as the function result.  Otherwise
 0 is returned.  
 
 If not_guessable_obj is non-zero, and we end up trying to guess at what addr is
 (see code below) then we won't assume it's a Objective C object that can be
 displayed with a gdb PRINT-OBJECT command.  This code can do that but under 
 some conditions it appears that attempting this goes into a loop.  So this
 argument is provided to override the attempt.

 Note, since strings can either be C strings or Pascal strings we suffix "(pstring)"
 at the end of Pascal strings to indicate that.  The algorithm for determining this
 is not perfect so mistakes can be made.
*/

static int string_for_ptr(GDB_ADDRESS addr, char *theString, int maxLen,
			  int not_guessable_obj)
{    
    char 	  *s, *s1, c, c1;
    int           pstring, str_obj, stringLen, totalLen, needQuote, override_quoting,
    		  maxLen_minus_2, b;
    unsigned long addr4;
    String_Type   strType;
    

    #define OBJCSTR_SUFFIX " (obj)"
    #define PSTRING_SUFFIX " (pstr)"
    
    #define SECRET_CODE 424242
    #define PSTRING_SUFFIX2 " (p)"
    
    static const int objcstr_suffixLen  = sizeof(OBJCSTR_SUFFIX)  - 1;
    static const int pstring_suffixLen  = sizeof(PSTRING_SUFFIX)  - 1;
    static const int pstring_suffix2Len = sizeof(PSTRING_SUFFIX2) - 1;
    
    static int  result_string_size = 0;
    static char *result_string     = NULL;
    
    static int env_MACSBUG_COMMENT_INSNS = 0;
    static int gen_MACSBUG_COMMENT_INSNS = -1;
    
    if (gen_MACSBUG_COMMENT_INSNS < macsbug_generation) {
	char *env = getenv("MACSBUG_COMMENT_INSNS");
	env_MACSBUG_COMMENT_INSNS = env ? atoi(env) : 0;
	gen_MACSBUG_COMMENT_INSNS = macsbug_generation;
    }
       
    str_obj = stringLen = needQuote = override_quoting = totalLen = 0;
    strType = UNKNOWN_STRING_TYPE;
    
    if (maxLen > result_string_size) {
    	result_string_size = maxLen + 3;
    	result_string = (char *)gdb_realloc(result_string, result_string_size);
    }
    
    /* Find out which object file section the addr is in and go after the string	*/
    /* accordingly.  This can (will?) fail for dynmaically created data pointers.	*/
    
    if (gdb_is_addr_in_section(addr, "__DATA.__cfstring") ||
        gdb_is_addr_in_section(addr, "__OBJC.__cstring_object")) {
	str_obj = stringLen = is_const_string_object(addr, &strType, result_string, maxLen);
    } else if (gdb_is_addr_in_section(addr, "__OBJC.__class"     ) ||
    	       gdb_is_addr_in_section(addr, "__OBJC.__meta_class")) {
	if (gdb_read_memory_from_addr(&addr4, addr + 8, 4, 0) &&
	    gdb_is_addr_in_section((GDB_ADDRESS)addr4, "__TEXT.__cstring"))
	    	str_obj = stringLen = is_printable_string((GDB_ADDRESS)addr4, &strType,
	    						  result_string, maxLen);
    } else if (gdb_is_addr_in_section(addr, "__OBJC.__message_refs") ||
	       gdb_is_addr_in_section(addr, "__OBJC.__cls_refs"    ) ||
	       gdb_is_addr_in_section(addr, "__OBJC.__cls_meth"    ) ||
	       gdb_is_addr_in_section(addr, "__OBJC.__inst_meth"   )) {
	if (gdb_read_memory_from_addr(&addr4, addr, 4, 0) &&
	    gdb_is_addr_in_section((GDB_ADDRESS)addr4, "__TEXT.__cstring"))
	    	stringLen = is_printable_string((GDB_ADDRESS)addr4, &strType, 
	    					result_string, maxLen);
    } else if (gdb_is_addr_in_section(addr, "__DATA.__const_data")) {
	str_obj = stringLen = is_const_string_object((GDB_ADDRESS)addr4, &strType,
						      result_string, maxLen);
	if (!stringLen) {
	    strType = UNKNOWN_STRING_TYPE;
	    stringLen = is_printable_string(addr, &strType, result_string, maxLen);
	}
    } else if (gdb_is_addr_in_section(addr, "__DATA.__basicstring") &&
	       gdb_read_memory_from_addr(&addr4, addr + 4, 4, 0)) {
	if (gdb_is_addr_in_section((GDB_ADDRESS)addr4, "__TEXT.__cstring")) {
	    strType = IS_PSTRING;
	    stringLen = is_printable_string((GDB_ADDRESS)addr4, &strType, result_string,
	    				    maxLen);
	}
    } else if (!gdb_is_addr_in_section(addr, "__TEXT.__literal4"     )  &&
	       !gdb_is_addr_in_section(addr, "__TEXT.__literal8"     )  &&
	       !gdb_is_addr_in_section(addr, "__TEXT.__literal16"    )  &&
	       !gdb_is_addr_in_section(addr, "__TEXT.__string_object")) 
	stringLen = is_printable_string(addr, &strType, result_string, maxLen);
    
    /* If all else fails (possibly due to dynamically created data), guess!		*/
    
    /* Note, it could be argued "why don't we ALWAYS just guess?".  Well, that might 	*/
    /* work, and we get rid of all the above code.  But the above is for date references*/
    /* to the existing object code sections while this is for stuff presumably created	*/
    /* at runtime.  We can trust the above.  But the stuff here is more problematic.	*/
    
    /* The stuff we guess at here is Objective C object and class references, and 	*/
    /* RealBasic strings.  If those fail to yield something usable we could also try	*/
    /* to see if the addr points to a printable string (or even a pointer to a pointer	*/
    /* to a printable string).  This was considered.  But we end up producing a lot of 	*/
    /* garbage comments just because the pointers may point to a few null terminated 	*/
    /* printable characters.  So for now we don't do this.				*/
    
    /* See if we can do a PRINT-OBJECT addr and use it's output (actually we use our	*/
    /* own version of PO).  Note that classes are displayed using "_NSPrintForDebugger" */
    /* or "_CFPrintForDebugger".  It is possible that given the "right" address you can	*/
    /* cause these routines to loop even though the specified address is possibly	*/
    /* accessible (determined because we can read it).  So we do some additional checks	*/
    /* to see if we think the isa pointer of the object is pointing at a class 		*/
    /* definition (see is_objc_class() for details since that is what we use).		*/
    
    if (stringLen == 0 && !not_guessable_obj) {		/* check for object or class	*/
	stringLen = is_objc_class(addr, result_string, maxLen); /* isa seems ok...	*/
	if (stringLen > 0) {				/* so give po a shot...		*/
	    stringLen = gdb_show_objc_object(addr, result_string, maxLen);
	    if (stringLen > 0 && (strcmp(result_string, "<nil>") == 0 ||
	    			  strcmp(result_string, "<not an object>") == 0))
		stringLen = 0;
	    else {					/* edit the returned string to	*/
		s = (s1 = result_string) - 1;		/* remove '\n's and runs of ' 's*/
		b = 0;
		
		while ((c = *++s) != '\0') {
		    if (c == '\n')
			b = 1;
		    else if (isspace(c)) {
			if (b == 0)
			    *s1++ = ' ';
			b = 1;
		    } else if (isprint(c) || c == '\a' || c == '\f' || c == '\b' || c == '\v') {
			*s1++ = c;
			b = 0;
		    } else {
			stringLen = 0;
			break;
		    }
		}
		
		if (stringLen > 0) {			/* edited string is ready to go */
		    while (isspace(*--s1)) ;		/* remove trailing space(s)	*/
		    *++s1 = '\0';
		    stringLen = s1 - result_string;
		    strType = IS_CSTRING;
		    override_quoting = 1;		/* never quote these things	*/
		}
	    }
	}
    }
    
    /* Check for a RealBasic string.  The addr must point to a Pascal string pointer at	*/
    /* addr+4.										*/
	
    if (stringLen == 0) {				/* check for RealBasic string...*/
	if (gdb_read_memory_from_addr(&addr4, addr + 4, 4, 0)) {
	    strType = IS_RB_PSTRING;
	    stringLen = is_printable_string((GDB_ADDRESS)addr4, &strType, result_string, maxLen);
	}
    }
    
    /* Check for a constant string object pointer.  These should be picked up by the 	*/
    /* first (PO) check above.  But just in case...					*/
    
    if (stringLen == 0)					/* check const string object...	*/
    	str_obj = stringLen = is_const_string_object(addr, &strType, result_string, maxLen);

    /* Finally check for a class description.  Like constant string objects these too	*/
    /* should have been detected by the PO above.  But again, just in case.  We got the	*/
    /* code so we might as well use it!							*/
    
    if (stringLen == 0) {				/* check class description...	*/
	strType = IS_CSTRING;
	stringLen = is_objc_class(addr, result_string, maxLen);
    }
    
    /* If we got a string then copy it to the returned string quoting and truncating as	*/
    /* as necessary...									*/
    
    if (stringLen == 0)
    	return (0);
    	
    s = result_string;
    
    if (!override_quoting)
	if (str_obj || env_MACSBUG_COMMENT_INSNS != SECRET_CODE) {
	    *theString++ = '"';
	    totalLen     = needQuote = 1;
	} else {
	    totalLen = needQuote = (strType == IS_PSTRING || *s == ' ' || *(s + stringLen - 1) == ' ');
	    if (needQuote)
		*theString++ = '\'';
	}
    
    maxLen -= needQuote;
    if (str_obj && env_MACSBUG_COMMENT_INSNS != SECRET_CODE)
	maxLen -= objcstr_suffixLen;
    if (strType == IS_PSTRING)
	maxLen -= (env_MACSBUG_COMMENT_INSNS != SECRET_CODE) ? pstring_suffixLen
							     : pstring_suffix2Len;
    maxLen_minus_2 = maxLen - 2;
    
    while ((c = *s++) != '\0' && totalLen < maxLen) {	/* show \n, \t, \r explicitly...*/
    	switch (c) {
    	    case '\n': c1 = 'n';
	      esc_chr: if (totalLen >= maxLen_minus_2)
			   break;
		       *theString++ = '\\'; *theString++ = c1;
		       totalLen += 2;
		       break;
    	    case '\r': c1 = 'r'; goto esc_chr;
    	    case '\t': c1 = 't'; goto esc_chr;
	    case '\a': c1 = 'a'; goto esc_chr;
	    case '\f': c1 = 'f'; goto esc_chr;
	    case '\b': c1 = 'b'; goto esc_chr;
	    case '\v': c1 = 'v'; goto esc_chr;
	    default: *theString++ = c;
		     ++totalLen;
    	}
    }
    
    if (needQuote) {
	if (str_obj || env_MACSBUG_COMMENT_INSNS != SECRET_CODE) {
	    if (totalLen <= maxLen)
		*theString++ = '"';
	    else
		strcpy(theString - 3, "...\"");
	} else {
	    if (totalLen <= maxLen)
		*theString++ = '\'';
	    else
		strcpy(theString - 3, "...'");
	}
	++totalLen;
    }
    
    if (str_obj && env_MACSBUG_COMMENT_INSNS != SECRET_CODE) {
	strcpy(theString, OBJCSTR_SUFFIX);
	theString += objcstr_suffixLen;
	totalLen  += objcstr_suffixLen;
    }

    if (strType == IS_PSTRING) {
    	if (env_MACSBUG_COMMENT_INSNS != SECRET_CODE) {
	    strcpy(theString, PSTRING_SUFFIX);
	    totalLen += pstring_suffixLen;
	} else {
	    strcpy(theString, PSTRING_SUFFIX2);
	    totalLen += pstring_suffix2Len;
	}
    } else
	*theString = '\0';
    
    return (totalLen);
}


/*----------------------------------------------*
 | string_for_value - convert value to a string |
 *----------------------------------------------*
 
 Set theString (up to maxLen characters but at least a minimum of 11) with the hex
 of the specified value.  If the value is actually a code address to the start of
 a function that is shown too.  If not, and the value can be shown as a printable
 string (ignoring leading zeroes in the value) then that is shown along with the
 hex.  The length of theString is returned as the function result.  0 is returned 
 if no string is generated.
 
 Examples:  0x313233 <some_function_name>      if 0x313233 is a function address
            0x313233 '123'                     if 0x313233 can be shown as chars
            0x313233                           if 0x313233 can't be shown as chars
*/

static int string_for_value(unsigned long long value, char *theString, int maxLen)
{
    int  i, len, totalLen;
    char *p, *p1, *s;
    
    static unsigned long long magic_constants[] = {
    	0x6666666666666667ULL,			/* divide by multiples of 5		*/
    	0x5555555555555556ULL,			/* divide by 3				*/
    	0x6666666655555555ULL			/* divide by -3				*/
    };
    
    if (value == 0)
    	return (0);
     
    /* We won't bother showing the characters if we have a value that (we think) is 	*/
    /* really an address of the START (offset 0) of some known function (in a known 	*/
    /* __TEXT section).  On the other hand if we don't have a symbol for the address,	*/
    /* i.e., we only have a value which is shown as hex, then keep that to show the hex	*/
    /* value for the characters.  The characters won't be shown unless all the bytes of	*/
    /* tha value are printable.								*/
    
    (void)gdb_address_symbol((GDB_ADDRESS)value, 0, theString, maxLen);
    
    if ((p = strchr(theString, ':')) != NULL && strrchr(p, '+') == NULL) {
	if (gdb_is_addr_in_section((GDB_ADDRESS)value, "__TEXT")                  ||
	    gdb_is_addr_in_section((GDB_ADDRESS)value, "__TEXT.__text")           ||
	    gdb_is_addr_in_section((GDB_ADDRESS)value, "__TEXT.__symbol_stub")    ||
	    gdb_is_addr_in_section((GDB_ADDRESS)value, "__TEXT.__symbol_stub1")   ||
	    gdb_is_addr_in_section((GDB_ADDRESS)value, "__TEXT.__picsymbol_stub") ||
	    gdb_is_addr_in_section((GDB_ADDRESS)value, "__TEXT.__picsymbolstub1") ||
	    gdb_is_addr_in_section((GDB_ADDRESS)value, "__TEXT.__StaticInit"))
	return (strlen(theString));
    }
    
    totalLen = p ? p - theString : strlen(theString);
   
    for (i = len = 0, p = (char *)&value + (8 - target_arch); i < target_arch; ++i, ++p)
       if (*p == '\0') {				/* ignore leading 0's		*/
	   if (len > 0)					/* but not 0's after printables	*/
	       return (totalLen);
       } else if (isprint(*p)) {			/* up till printable chars	*/
	   if (++len == 1)				/* remember 1st printable	*/
	       p1 = p;
       } else						/* non-printables, use as is	*/
	   return (totalLen);
    
    /* Eliminate the "magic numbers"...							*/
    
    if (len == target_arch)
       for (i = 0; i < sizeof(magic_constants)/sizeof(unsigned long long); ++i)
	   if (memcmp(p1,  (char *)&magic_constants[i] + (8-target_arch), target_arch) == 0)
	       return (0);
    
    /* Hey, we made it!  Create the string...						*/
    
    theString += totalLen;
    *theString++ = ' ';
    ++totalLen;
	 
    *theString++ = '\'';
    for (i = target_arch - len; i < target_arch; ++i)
	*theString++ = *p1++;
    
    *theString++ = '\'';
    *theString   = '\0';
    
    return (totalLen + len + 2);
}


/*------------------------------------------------------------------*
 | cmp - create a comment representing the comparison of two values |
 *------------------------------------------------------------------*
 
 A string is created in theString representing the (signed if signd is non-zero)
 comparison of the left and right values.  The function returns the length of
 theString.
 
 It is assumed that theString is at least 41 bytes long to hold the max comparison
 of 2 16-byte strings ('...') and a 4-byte relation operator (e.g., " == ").  The
 left AND right values are shown as strings if they are BOTH printable (ignoring
 leading zeros in the values).  The left OR the right is shown as a string if they
 are printable and the other side if the comparison is 0.  Otherwise both are shown
 as hex values.
*/

static int cmp(unsigned long long left, unsigned long long right, int signd, char *theString)
{
    int		       i, j;
    char	       *p, *s1, *s2, rel[6];
    unsigned long long left_right;
    
    for (i = 0, s1 = theString, left_right = left; i < 2; ++i) {
	for (j = 0, p = (char *)&left_right + (8 - target_arch), s2 = s1;
	     j < target_arch; ++j, ++p) {
	    if (*p == '\0') {
		if (s2 > s1) {
		    s2 = s1;
		    break;
		}
	    } else {
		if (s2 == s1)
		    *s2++ = '\'';
		if (isprint(*p))
		    *s2++ = *p;
		else {
		    switch ((char)*p) {
			case '\n': s2 += sprintf(s2, "\\n"); continue;
			case '\r': s2 += sprintf(s2, "\\r"); continue;
			case '\t': s2 += sprintf(s2, "\\t"); continue;
			case '\a': s2 += sprintf(s2, "\\a"); continue;
			case '\f': s2 += sprintf(s2, "\\f"); continue;
			case '\b': s2 += sprintf(s2, "\\b"); continue;
			case '\v': s2 += sprintf(s2, "\\v"); continue;
			default:   s2 = s1;
				   j = target_arch;
				   continue;
		    }
		}
	    }
	}
		
	if (s2 > s1) {
	    if (i == 1 && *theString != '\'' && left != 0)	/* 0x1234 rel 'right'	*/
	    	s1 += sprintf(s1, "0x%llx", right);	
	    else {						/* 'left' rel 'right'	*/
		s1    = s2;					/* 0x0    rel 'right'	*/
		*s1++ = '\'';
		*s1   = '\0';
	    }
	} else if (i == 1 && *theString == '\'' && right != 0) {/* 'left' rel 0x4567	*/
	    s1 = theString + sprintf(theString, "0x%llx%s0x%llx", left, rel, right);
	    break;
	} else							/* 0x1234 rel 0x4567	*/
	    s1 += sprintf(s1, "0x%llx", left_right);		/* 'left' rel 0x0	*/
		    
	if (i == 0) {
	    if (signd) {
		if ((long long)left > (long long)right)
		    strcpy(rel, " > ");
		else if ((long long)left < (long long)right)
		    strcpy(rel, " < ");
		else
		    strcpy(rel, " == ");
	    } else {
		if ((unsigned long long)left > (unsigned long long)right)
		    strcpy(rel, " > ");
		else if ((unsigned long long)left < (unsigned long long)right)
		    strcpy(rel, " < ");
		else
		    strcpy(rel, " == ");
	    }
	
	    s1 += sprintf(s1, "%s", rel);
	    
	    left_right = right;
	}
    }
    
    return (s1 - theString);
}


/*-----------------------------------------------------------------------*
 | no_reg - gdb_get_register() failure handler for try_to_comment_insn() |
 *-----------------------------------------------------------------------*
 
 The way try_to_comment_insn() calls gdb_get_register() is as part of an expression.
 But there is the (remote?) possibility that gdb_get_register() could fail.  So we
 use setjmp/longjmp to handle the failure as an exception.  The expressions calling
 gdb_get_register() call no_reg() if a failure is detected.  no_reg is typed so it
 can be used arithmetically in the expression.  But it will never return.  Instead
 it will longjmp using the following setjmp jmp_buf:
 											*/
 static jmp_buf no_reg_jmpbuf;
											/*
 try_to_comment_insn() sets this jmp_buf up so it can handle detect this exception
 and return 0.
*/

static GDB_ADDRESS no_reg(void)
{
    longjmp(no_reg_jmpbuf, 1);
}


/*-------------------------------------------------------------------------------------*
 | try_to_comment_insn - see if we can add a "constructive" comment to the instruction |
 *-------------------------------------------------------------------------------------*
 
 For the instruction (at the current pc) see if we can use it to derive a possibly useful
 comment to display in its disassembly.  The function returns the length of the the string
 in comment, up to maxLen characters, or 0 if not comment is created.

 Accepted instructions that have a pointer associated with them use that pointer to try
 to see if it is associated with a printable string or string object.  This minimizes some
 of the potential for creating garbage strings, but not always.
 
 See code below for the list of instructions that are handled.
*/

static int try_to_comment_insn(GDB_ADDRESS pc, unsigned long instruction,
			       char *comment, int maxLen)
{
    int           pri, ext, rt, rs, ra, rb, frs, frt, i, len;
    unsigned long ui, ui2, addr4, prev_instr;
    long long     si, d, ds;
    GDB_ADDRESS   ea, addr, value, rsval, raval, rbval, lr, ctr;
    float	  fltval;
    double	  dblval;
    unsigned char c;
    char          rsstr[6], rastr[6], rbstr[6], fsstr[6];
    
    #define FIX_ADDRESS(addr) if (target_arch == 4) addr &= ((GDB_ADDRESS)1 << 32) - 1;
    
    #define SI_MASK 0x0000FFFFU			/* si mask                 (bits 16-31) */
    #define UI_MASK 0x0000FFFFU			/* ui mask     		   (bits 16-31) */
    #define D_MASK  0x0000FFFFU			/* d  mask     		   (bits 16-31) */
    #define DS_MASK 0x0000FFFCU			/* ds mask     		   (bits 16-29) */

    #define BITS(instruction, b1, b2) \
	(((unsigned long)(instruction) >> (31UL-(b2))) & ((1UL << ((b2)-(b1)+1)) - 1UL))
    
    #define SIGN_EXTEND32(x) ((((unsigned long)x) & 0x00008000U)			\
				 ? (((unsigned long)x) | 0xFFFF0000U) 			\
				 : ((unsigned long)x))
    #define SIGN_EXTEND64(x) ((long long)(((unsigned long)x & 0x00008000U) 		\
					    ? (long)(((unsigned long)x) | 0xFFFF0000U) 	\
					    : (long)x))
    
    #define SIGN_EXTEND(x) SIGN_EXTEND64(x)
    
    /* These macros call no_reg() for gdb_get_register() failures (exceptions).  We use	*/
    /* these macro to make the emulation of the instructions more readable.  They 	*/
    /* expect their register numbers be already set in rs, ra, rb, or frs respectively	*/
    /* as required by the macro used.							*/
    
    #define Rs (sprintf(rsstr, "$r%d", rs), gdb_get_register(rsstr, &rsval)  ? rsval : no_reg())
    #define Ra (sprintf(rastr, "$r%d", ra), gdb_get_register(rastr, &raval)  ? raval : no_reg())
    #define Rb (sprintf(rbstr, "$r%d", rb), gdb_get_register(rbstr, &rbval)  ? rbval : no_reg())
    #define Fs (sprintf(fsstr, "$f%d", frs),gdb_get_register(fsstr, &dblval) ? dblval: no_reg())
    
    /* Set up our exception handling to handle possible gdb_get_register() failures   	*/
    /* from the above macros.  This is probably overkill.  See no_reg() for further 	*/
    /* details.										*/
    
    if (setjmp(no_reg_jmpbuf))
    	return (0);
    
    /* Use the primary and extend fields to determine which instructions we have...	*/
    
    switch (pri = BITS(instruction, 0, 5)) {
	case 10:						     /* cmpli bf,l,ra,ui*/
	    ra = BITS(instruction, 11, 15);
	    ui = (instruction & UI_MASK);
	    return (cmp(Ra, ui, 0, comment));
	    
	case 11:						     /* cmpi  bf,l,ra,si*/
	    ra    = BITS(instruction, 11, 15);
	    si    = (instruction & SI_MASK);
	    si    = SIGN_EXTEND(si);
	    return (cmp(Ra, si, 1, comment));
	    
	case 14: 						     /* addi rt,ra,si   */
	    rt = BITS(instruction, 6, 10);
	    ra = BITS(instruction, 11, 15);
	    if (rt == 1 && ra == 1) return (0);
	    si = (instruction & SI_MASK);
	    si = SIGN_EXTEND(si);
	    if ((len = string_for_ptr((value = (ra == 0) ? si : (Ra + si)), comment, maxLen, 1)) > 0)
	    	return (len);
	    return (string_for_value(value, comment, maxLen));
    	
    	case 19:
	    switch (BITS(instruction, 21, 30)) {
	    	case 16:					     /* bclr[l]         */
	    	    if (!gdb_get_register("$lr", &lr))
	    	    	return (0);
	    	    return (strlen(gdb_address_symbol(lr, 0, comment, maxLen)));
	    	
	    	case 528:					     /* bcctr[l]        */
	    	    if (!gdb_get_register("$ctr", &ctr))
	    	    	return (0);
	    	    return (strlen(gdb_address_symbol(ctr, 0, comment, maxLen)));
	    	    
		default:  return (0);
	    }
	    break;
	
	case 24:						     /* ori  ra,rs,ui   */
	    if (instruction == 0x60000000 /*nop*/)
	    	return (0);
	    ui = (instruction & UI_MASK);
	    rs = BITS(instruction, 6, 10);
	    return (string_for_value(Rs | ui, comment, maxLen));
	    
	case 32: /* 4/8 byte loads use effective addr to get a   */  /* lwz  rt,d(ra)   */
	case 33: /* 4/8 byte ptr to the string.			 */  /* lwzu rt,d(ra)   */
	    ra = BITS(instruction, 11, 15);
	    d  = (instruction & D_MASK);
	    d  = SIGN_EXTEND(d);
	    if (ra == 1) {
	    	if (d <= 8) return (0);
	    	/* Assume bl target/lwz r2,20(r1) is a RealBasiic call...		*/
    	    	if (pri == 32 && d == 20 && BITS(instruction, 6, 10) == 2 &&
    	    	    gdb_read_memory_from_addr(&prev_instr, pc-4, 4, 0) &&
    	    	    BITS(prev_instr, 0, 5) == 18 && (prev_instr & 3) == 1)
    	    	    	return (0);
	    }
	    if (!gdb_read_memory_from_addr(&addr4, (ra == 0) ? d : Ra + d, 4, 0))
		return (0);
	    if ((len = string_for_ptr((GDB_ADDRESS)addr4, comment, maxLen, 0)) > 0)
	    	return (len);
    	    return (string_for_value((GDB_ADDRESS)addr4, comment, maxLen));
    	    
	case 34: /* Byte loads are handled differently in that   */  /* lbz  rt,d(ra)   */
	case 35: /* effective addr is used as ptr to the string. */  /* lbzu rt,d(ra)   */
	    ra = BITS(instruction, 11, 15);
	    d  = (instruction & D_MASK);
	    d  = SIGN_EXTEND(d);
	    if ((len = string_for_ptr(addr = (ra == 0) ? d : Ra + d, comment, maxLen, 0)) > 0)
	    	return (len);
	    if (!gdb_read_memory_from_addr(&c, addr, 1, 0))
		return (0);
	    return (string_for_value((GDB_ADDRESS)c, comment, maxLen));
   
	case 38: /* All stores only use the register being   	 */  /* stb   rs,d(ra)  */
	case 39: /* stored (rs) as a ptr to a string.		 */  /* stbu  rs,d(ra)  */
    	    rs = BITS(instruction, 6, 10);
    	    if ((len = string_for_ptr(Rs, comment, maxLen, 0)) > 0)
    	    	return (len);
    	    return (string_for_value((GDB_ADDRESS)(unsigned char)rsval, comment, maxLen));
	
	case 36:						     /* stw   rs,d(ra)  */
	case 37: 						     /* stwu  rs,d(ra)  */
    	    ra = BITS(instruction, 11, 15);
	    d  = (instruction & D_MASK);
	    d  = SIGN_EXTEND(d);
    	    if (ra == 1 && d <= 8)
    	    	return (0);
    	    rs = BITS(instruction, 6, 10);
    	    if ((len = string_for_ptr(Rs, comment, maxLen, 0)) > 0)
    	    	return (len);
    	    return (string_for_value((GDB_ADDRESS)(unsigned long)rsval, comment, maxLen));
     
        case 48: 						     /* lfs  frs,d(ra)  */
	case 49: 						     /* lfsu frt,d(ra)  */
	    ra = BITS(instruction, 11, 15);
	    d  = (instruction & D_MASK);
	    d  = SIGN_EXTEND(d);
	    if (!gdb_read_memory_from_addr(&fltval, (ra == 0) ? d : Ra + d, sizeof(float), 0))
		return (0);
	    return (sprintf(comment, "%.9g", fltval));
	    
        case 50:						     /* lfd  frt,d(ra)  */
	case 51:						     /* lfdu frt,d(ra)  */
	    ra = BITS(instruction, 11, 15);
	    d  = (instruction & D_MASK);
	    d  = SIGN_EXTEND(d);
	    if (!gdb_read_memory_from_addr(&dblval, (ra == 0) ? d : Ra + d, sizeof(double), 0))
		return (0);
	    return (sprintf(comment, "%.17g", dblval));
	
	case 52:						     /* stfs  frs,d(ra) */
	case 53: 						     /* stfsu frs,d(ra) */
    	    frs = BITS(instruction, 6, 10);
    	    return (sprintf(comment, "%.9g", Fs));
    	    
	case 54:						     /* stfd  frs,d(ra) */
	case 55:						     /* stfdu frs,d(ra) */
    	    frs = BITS(instruction, 6, 10);
    	    return (sprintf(comment, "%.17g", Fs));

	case 58:
	    switch (BITS(instruction, 30, 31)) {
		case 0:   					     /* ld   rt,ds(ra)  */ 
		case 1:   					     /* ldu  rt,ds(ra)  */
		    ra = BITS(instruction, 11, 15);
		    ds = (instruction & DS_MASK);
		    ds = SIGN_EXTEND(ds);
		    if (!gdb_read_memory_from_addr(&addr, (ra == 0) ? d : Ra + ds, 8, 0))
			return (0);
		    if ((len = string_for_ptr(addr, comment, maxLen, 0)) > 0)
		    	return (len);
		    return (string_for_value(addr, comment, maxLen));
		
		default:
		    return (0);
	    }
	    break;

	case 62:
	    switch (BITS(instruction, 30, 31)) {
		case 0: 					     /* std  rs,ds(ra)  */
		case 1: 					     /* stdu rs,ds(ra)  */
		    rs = BITS(instruction, 6, 10);
		    if ((len = string_for_ptr(Rs, comment, maxLen, 0)) > 0)
		    	return (len);
		    return (string_for_value(rsval, comment, maxLen));
		
		default:
		    return (0);
	    }
	    break;
	
	case 31:
	    switch (ext = BITS(instruction, 21, 30)) {
	    	case 0:						     /* cmp   bf,l,ra,rb*/
	    	case 32:					     /* cmpl  bf,l,ra,rb*/
		    ra = BITS(instruction, 11, 15);
		    rb = BITS(instruction, 16, 20);
		    return (cmp(Ra, Rb, (ext == 0), comment));
	    	
		case 87:  					     /* lbzx  rt,ra,rb  */
		case 119: 					     /* lbzux rt,ra,rb  */
		    ra = BITS(instruction, 11, 15);
		    rb = BITS(instruction, 16, 20);
		    if ((len = string_for_ptr((addr = (Ra + Rb)), comment, maxLen, 0)) > 0)
		    	return (len);
		    if (!gdb_read_memory_from_addr(&c, addr, 1, 0))
			return (0);
		    return (string_for_value((GDB_ADDRESS)c, comment, maxLen));
				
		case 23:  					     /* lwzx  rt,ra,rb  */
		case 55:  					     /* lwzux rt,ra,rb  */
		    ra = BITS(instruction, 11, 15);
		    rb = BITS(instruction, 16, 20);
		    if (!gdb_read_memory_from_addr(&addr4, Ra + Rb, 4, 0))
			return (0);
		    if ((len = string_for_ptr((GDB_ADDRESS)addr4, comment, maxLen, 0)) > 0)
			return (len);
		    return (string_for_value((GDB_ADDRESS)addr4, comment, maxLen));
		    
		case 21:  					     /* ldx   rt,ra,rb  */
		case 53:  					     /* ldux  rt,ra,rb  */
		    ra = BITS(instruction, 11, 15);
		    rb = BITS(instruction, 16, 20);
		    if (!gdb_read_memory_from_addr(&addr, Ra + Rb, 8, 0))
			return (0);
		    if ((len = string_for_ptr(addr, comment, maxLen, 0)) > 0)
		    	return (len);
		    return (string_for_value(addr, comment, maxLen));
		   
		case 215: 					     /* stbx  rs,ra,rb  */
	    	case 247: 					     /* stbux rs,ra,rb  */
		    rs = BITS(instruction, 6, 10);
		    if ((len = string_for_ptr(Rs, comment, maxLen, 0)) > 0)
			return (len);
		    return (string_for_value((GDB_ADDRESS)(unsigned char)rsval, comment, maxLen));

	    	case 151: 					     /* stwx  rs,ra,rb  */ 
	    	case 183: 					     /* stwux rs,ra,rb  */ 
		case 149: 					     /* stdx  rs,ra,rb  */
	    	case 181: 					     /* stdux rs,ra,rb  */
		    rs = BITS(instruction, 6, 10);
		    if ((len = string_for_ptr(Rs, comment, maxLen, 0)) > 0)
			return (len);
		    return (string_for_value((GDB_ADDRESS)(unsigned long)rsval, comment, maxLen));
	    	
	    	case 266: 					     /* add   rt,ra,rb  */
	    	    ra = BITS(instruction, 11, 15);
		    rb = BITS(instruction, 16, 20);
		    if ((len = string_for_ptr((value = (Ra + Rb)), comment, maxLen, 0)) > 0)
		    	return (len);
		    return (string_for_value(value, comment, maxLen));
		
		case 444: 					     /* or    ra,rs,rb  */
		    rs = BITS(instruction,  6, 10);
		    rb = BITS(instruction, 16, 20);
		    if ((len = string_for_ptr((value = (Rs | Rb)), comment, maxLen, 0)) > 0)
		    	return (len);
		    return (string_for_value(value, comment, maxLen));
	    	
	    	case 535:					     /* lfsx   frt,ra,rb*/
		case 567:					     /* lfsux  frt,ra,rb*/
	    	    ra = BITS(instruction, 11, 15);
		    rb = BITS(instruction, 16, 20);
		    if (!gdb_read_memory_from_addr(&fltval, Ra + Rb, sizeof(float), 0))
			return (0);
		    return (sprintf(comment, "%.9g", fltval));
		
		case 599:					     /* lfdx   frt,ra,rb*/
		case 631:					     /* lfdux  frt,ra,rb /
	    	    ra = BITS(instruction, 11, 15);
		    rb = BITS(instruction, 16, 20);
		    if (!gdb_read_memory_from_addr(&dblval, Ra + Rb, sizeof(double), 0))
			return (0);
		    return (sprintf(comment, "%.17g", dblval));
		
	    	case 663: 					     /* stfsx  frs,ra,rb*/
	    	case 695:					     /* stfsux frs,ra,rb*/
		    frs = BITS(instruction, 6, 10);
		    return (sprintf(comment, "%.9g", Fs));
	    	
	    	case 727: 					     /* stfdx  frs,ra,rb*/
	    	case 759:					     /* stfdux frs,ra,rb*/
		    frs = BITS(instruction, 6, 10);
		    return (sprintf(comment, "%.17g", Fs));
		    
		default:  return (0);
	    }
	    break;
	
	default:
	    return (0);
    }
    	
    return (0);
}
#endif /* not_ready_for_prime_time_but_keep_enabled_for_private_testing */


/*-----------------------------------------------------------------------------------*
 | branch_taken - determine if instr. at PC is conditional branch whether it's taken |
 *-----------------------------------------------------------------------------------*
 
 If the specified instruction is a conditional branch the function returns 1 if the
 branch will be taken and -1 if it won't.  0 is returned if the instruction is not
 a conditional branch.
 
 The algorithm here is based on what is described in the IBM PPC Architecture book.
 To paraphrase what's there in determining whether the branch is taken or not it
 basically says:

   bc[l][a]  (primary = 16)
   bclr[l]   (primary = 19, extend = 16)
     cond_ok = BO[0] || (CR[BI] == BO[1])
     if !BO[2] then CTR = CTR-1
     ctr_ok = BO[2] || (CTR != 0) ^ BO[3]
     if ctr_ok && cond_ok then "will branch"
     else "will not branch"

   bcctr[l]  (primary = 19, extend = 528)
     cond_ok = BO[0] || (CR[BI] == BO[1])
     if cond_ok then "will branch"
     else "will not branch"

 where the notation X[i] is bit i in field X.

 The implementation of this below is "optimized" to read as:
     cond_ok = BO[0] || (CR[BI] == BO[1])
     if (cond_ok && !"bcctr[l]") then cond_ok = BO[2] || (CTR-1 != 0) ^ BO[3]
     if cond_ok then "will branch"
     else "will not branch"
     
 Note, for all these brances, of BO == 0x14 we have a "branch always" which are not
 classed as conditional branches for our purposes.
*/

static int branch_taken(unsigned long instruction)
{
    int 	  pri, ext, cond_ok;
    GDB_ADDRESS   ctr;
    union {					/* gdb_get_register() could return a	*/
    	unsigned long long cr_err;		/* error code as a long long.		*/
    	unsigned long cr;			/* but the cr is always only 32-bits	*/
    } cr;
        
    pri = (instruction >> 26);			/* get primary opcode			*/
    if (pri == 19) {				/* bclr[l] or bcctr[l] ?		*/
    	ext = (instruction >> 1) & 0x3FFUL;	/* ...get extension			*/
    	if (ext != 16 && ext != 528)		/* ...not bclr[l] or bcctr[l]		*/
    	    return (0);
    } else if (pri == 16)			/* bc[l][a]				*/
    	ext = 0;
    else					/* not bc[l][a], bclr[l], or bcctr[l]	*/
    	return (0);
    
    if (((instruction >> 21) & 0x14) == 0x14)	/* ignore "branch always" branches	*/
    	return (0);
    	
    /* At this point we know we have a bc[l][a], bclr[l], or bcctr[l].   All three of	*/
    /* these do one common test: cond_ok = BO[0] || (CR[BI] == BO[1])			*/

    cond_ok = (instruction & 0x02000000); 	/* BO[0]				*/
    if (!cond_ok) {				/* !BO[0], try CR[BI] == BO[1]		*/
    	gdb_get_register("$cr", &cr.cr);
	cond_ok = (((cr.cr >> (31-((instruction>>16) & 0x1F))) & 1) == (((instruction & 0x01000000) >> 24) & 1));
    }
    
    /* bc[l][a] and bclr[l] also need to check to ctr...				*/
     
    if (cond_ok && ext != 528) {		/* cond_ok = BO[2] || (CTR-1!=0)^BO[3]	*/
    	cond_ok = (instruction & 0x00800000);	/* BO[2]				*/
    	if (!cond_ok) {				/* !BO[2], try (CTR-1 != 0) ^ BO[3]	*/
    	    gdb_get_register("$ctr", &ctr);
    	    cond_ok = (ctr-1 != 0) ^ ((instruction & 0x00400000) >> 22);
    	}
    }
    
    return (cond_ok ? 1 : -1);			/* return +1 if branch taken		*/
}


/*------------------------------------------------------*
 | format_disasm_line - format the gdb disassembly line |
 *------------------------------------------------------*
 
 This is the redirected stream output filter function for disassembly lines.  The src
 line is a gdb disassembly line expected to be in the following format:

   0xaaaa <function[+dddd][ at file:line]>:   opcode   operand [<comment>]
   0xaaaa <function[+dddd][ in file]>:        opcode   operand [<comment>]
    
 Where 0xaaaa is the address and dddd is the offset in the specified function.  The offset
 is suppressed if +dddd is zero.  The " at filename" or " in filename" is suppressed
 unless SET print symbol-filename is enabled.  Comments are the same format as function
 names and thus enclosed in angle brackets.
 
 The data passed to this function is a communication link between here and __disasm()
 or display_pc_area() which specified this function as the output filter.  The data is
 actually actually a DisasmData struct as defined in MacsBug.h.
 
 The line is reformatted to the way we want it (see code below) and written to the stream
 specified in the DisasmData's stream value.
*/

char *format_disasm_line(FILE *f, char *src, void *data)
{
    DisasmData    *disasm_info = (DisasmData *)data;
    char 	  c, c2, *p1, *p2, *eol, *src0 = src, *failure_msg;
    int	 	  n, n_addr, n_offset, n_opcode, n_operand, n_comment, brack, paren, angle,
    		  len, wrap_col, wrap, addr_width, introduced_comment, show_it;
    unsigned long instruction;
    char 	  address[20], function[1024], offset[8], opcode[12], operand[1025],
    		  comment[1025], verify[20];
    
    #define FORMATTED_SIZE 3000
    static char   formatted[FORMATTED_SIZE+3];	/* format line in here			*/
    
    if (!src)					/* null means flush, no meaning here	*/
    	return (NULL);

    /* Extract the address (note this will include the leading "0x")...			*/
    
    n_addr = 0;
    while (*src && *src != ':' && !isspace(*src) && n_addr < 18)
	address[n_addr++] = *src++;
    address[n_addr] = '\0';
    
    /* As an additional check verify that the address is the one we are expecting...	*/
    
    sprintf(verify, "0x%llx", (unsigned long long)disasm_info->addr);
    if (strcmp(verify, address) != 0) {
    	failure_msg = "MacsBug reformat error extracting address";
    format_failure:					/* come here for format failures*/
    	len = strlen(src0);
    	if (disasm_info->max_width > 0 && len > disasm_info->max_width)
    	    len = disasm_info->max_width;
    	if (len > FORMATTED_SIZE)
    	    len = FORMATTED_SIZE;
    	memcpy(formatted, src0, len);
    	formatted[len] = '\0';
    	if ((disasm_info->flags & NO_NEWLINE) != 0) {
    	    if (formatted[len-1] == '\n')
    	    	formatted[len-1] = '\0';
    	} else if (formatted[len-1] != '\n') {
    	    formatted[len]   = '\n';
    	    formatted[len+1] = '\0';
    	}
    	gdb_fprintf(disasm_info->stream, COLOR_RED "### %s (raw disassembly line follows)..."
    				 	 COLOR_OFF "\n%s", failure_msg, formatted);
    	return (NULL);
    }
    
    /* Extract function name and offset from "<function[+dddd][ in|at file:line]>: "...	*/
        
    /* The function name could be a Objective C message and thus enclosed in brackets 	*/
    /* which may contain embedded spaces (e.g., "[msg p1]").  The function may also be	*
    /* an unmangled C++ name, possibly a template instance, which means there are	*/
    /* possibly nested angle brackets or even unpaired angle brackes for operator> and	*/
    /* operator<.  RealBasic names too can have angle brackets. Hopefully none of these	*/
    /* have the delimiting sequence ": " (actually that space is a tab).  So that is	*/
    /* what is looked for once the left-most '<' is found.				*/
    
    function[0]          = '\0';
    offset[n_offset = 0] = '\0';
    
    while (*src && isspace(*src))		/* skip white space up to '<'		*/
    	++src;
    
    if (*src && *src == '<') {			/* if '<' extract function and offset...*/
    	p1 = src;				/* ...search for matching '>' delimiter	*/
	while (*++src && !(*src == '>' && *(src+1) == ':' && isspace(*(src+2)))) ;
	
	/* Found the ": ".  Scan backwards (to the left) looking for the possible '+' 	*/
	/* indicating the offset.  Everything before that (or everything if the '+' is	*/
	/* not found) is taken as the function name.  Everything after the '+' up to	*/
	/* the '>', " at ", or " in " is taken as the offset.  Of course if some idiot	*/
	/* embeds " at ", " in ", '+', or '>'s in their filename we'll fail in here.  	*/
	/* Too damn bad!								*/
	
	if (*src == '>') {			/* ...if ">:" found, scan backwards...	*/
	    p2 = src;
	    while (--p2 > p1 && *p2 != '+') ;	/* ...search left for '+' before offset	*/
	    n = p2 - ++p1;			/* ...length of function name		*/
	    	    
	    if (n > 0) {			/* ...extract function and offset...	*/
 	        if (n > 1023) n = 1023;
 	        memcpy(function, p1, n);	/* ...save function name		*/
 	        function[n] = '\0';
 	    	
		p1 = p2;			/* ...find out where offset ends	*/
 	        while (*++p2 && *p2 != '>' && *p2 != ' ') ;/* "+dddd[ at|in filename]>"	*/
		n_offset = p2 - p1;		/* ...save offset			*/
		if (n_offset > 7) n_offset = 7;
		memcpy(offset, p1, n_offset);
		offset[n_offset] = '\0';
	    } else {				/* ...if there wasn't any offset...	*/
		p2 = src - 3;			/* ...search back for "in|at filename"	*/
		while (--p2 > p1 && strncmp(p2, " at ", 4) != 0 &&
				    strncmp(p2, " in ", 4) != 0) ;
		n = (p2 <= p1) ? src - p1 : p2 - p1;

	        if (n > 1023) n = 1023;
 	        memcpy(function, p1, n);	/* ...save function name		*/
 	        function[n] = '\0';
 	        
 	        strcpy(offset, "+0");		/* ...fake the offset			*/
 	        n_offset = 2;
	    }
	    	    
	    ++src;				/* point at the required ':'		*/
	}
    }
    
    /* At this point we should be looking at the expected ':' at the end of the 	*/
    /* function name info.  If there we know the opcode and operand (and maybe a	*/
    /* comment) follow...								*/
    
    if (*src != ':') {
    	failure_msg = "MacsBug reformat error searching for ':'";
    	goto format_failure;
    }
    
    /* Extract the opcode...								*/
    
    while (*++src && isspace(*src)) ;		/* skip white space after ':'		*/
    
    n_opcode = 0;				/* opcode				*/
    while (*src && !isspace(*src))
    	if (n_opcode < 11)
    	    opcode[n_opcode++] = *src++;
    opcode[n_opcode] = '\0';
    
    /* Extract the operand and comment.  These are done together.  The operand is	*/
    /* assumed to contain no white space. So everything after it (ignoring white space) */
    /* is taken as the comment if it is enclosed in angle brackets.  The comment is	*/
    /* extracted by scanning left from the end of the line for the ending '>' and then	*/
    /* scanning right after the operand for the starting '<'.  Although it shouldn't 	*/
    /* happen, if the delimiting angle brackets are not found whatever is there is 	*/
    /* included in the operand.								*/
    
    if (*src)
    	while (*++src && isspace(*src)) ;	/* skip white space after opcode	*/
    
    if (*src) {					/* note, comment assumed enclosed in <>	*/
    	p1 = src;				/* p1 now points to start of operand	*/
    	while (*++src && !isspace(*src)) ;	/* look for end of operand		*/
    	n_operand = src - p1;			/* operand length (unless comment error)*/
	
	if (*src) {
	    while (*++src && isspace(*src)) ;	/* skip white space after operand	*/
	    
	    if (*src) {				/* comment				*/
		p2 = src + strlen(src);		/* scan left for ending character	*/
		while (--p2 > src && isspace(*p2)) ; /* gets rid of trailing space too	*/
		
		if (*src == '<' && *p2 == '>') {/* have expected comment delimiters...	*/
		    n_comment = p2 - src + 1;	/* ...extract the comment		*/
		    if (n_comment > 1023) n_comment = 1023;
		    memcpy(comment, src, n_comment);
		    comment[n_comment] = '\0';
		} else {			/* huh? (this should never happen)	*/
		    n_operand = p2 - src;	/* but include whatever follows operand	*/
		    n_comment = 0;		/* if we should get into here		*/
		}
	    } else				/* no comment				*/
		n_comment = 0;
 	}
 	
	if (n_operand > 1023) n_operand = 1023;	/* operand				*/
	memcpy(operand, p1, n_operand);
	operand[n_operand] = '\0';
    } else					/* no operand or comment		*/
    	n_operand = n_comment = 0;
    
    #if 0
    gdb_printf("address  = \"%s\"\n", address);
    gdb_printf("function = \"%s\"\n", function);
    gdb_printf("offset   = \"%s\"\n", offset);
    gdb_printf("opcode   = \"%s\"\n", opcode);
    gdb_printf("operand  = \"%s\"\n", operand);
    gdb_printf("comment  = \"%s\"\n", comment);
    #endif
    
    /* Get instruction at the address.  We already have the address as a hex string.	*/
    
    gdb_read_memory(&instruction, address, 4);
    
    /* Format the disassembly line using the following layout...			*/
    
    /*            1         2         3         4         5         6			*/
    /*  01234567890123456789012345678901234567890123456789012345678901234567890		*/
    /*    ^       ^         ^        ^^^          ^            ^			*/
    /*   function									*/
    /*    +dddddd aaaaaaaa  xxxxxxxx   opcode     operand...   comment...		*/
    /*    ^       ^         ^        .*^          ^            ^			*/
    
    /* where, +dddddd is offset, aaaaaaaa is address, xxxxxxxx is instruction.	The 	*/
    /* address is always shown as a minimum of 8 digits but it could be larger in 64-	*/
    /* bit mode, in which case, the instruction, opcode, and operand are all shifted	*/
    /* right proportionality.								*/
    
    wrap_col = (target_arch == 4) ? 12 : 20;
    if (disasm_info->flags & WRAP_TO_SIDEBAR)
	get_screen_size(&max_rows, &max_cols);
    
    memset(formatted, ' ', 60);
    
    /* Display the function name before the disassembly line if the current function	*/
    /* name is different from the one used for the previous line or we are asked to 	*/
    /* show it for the first line of the pc-area.  There can be interference between	*/
    /* the current function name in the pc-area and the history area.  Specifically	*/
    /* when we get near the end of the current function and the next one starts to 	*/
    /* appear in the pc-area.  When doing SO or SI that doesn't show in the history 	*/
    /* area.  So we need to keep two distinct current function names; one for each 	*/
    /* area.  That way the line for the function being shown in the pc-area doesn't	*/
    /* get confused with the one being shown in the history ara.			*/
    
    if (disasm_info->flags & DISASM_PC_AREA) {
    	show_it = strcmp(function, curr_function2) != 0 ||
    		  (disasm_info->flags & ALWAYS_SHOW_NAME);
    	if (show_it)
    	    strcpy(curr_function2, function);
    } else if (strcmp(function, curr_function1) != 0) {
    	show_it = 1;
    	strcpy(curr_function1, function);
    } else
    	show_it = 0;
    
    if (show_it) {
    	if (disasm_info->flags & WRAP_TO_SIDEBAR)
    	    function[max_cols-wrap_col] = '\0';
    	if (0 && !macsbug_screen && isatty(STDOUT_FILENO))
    	    gdb_fprintf(disasm_info->stream, " " COLOR_BLUE "%s" COLOR_OFF "\n", function);
    	else if (disasm_info->max_width > 0 && strlen(function) > disasm_info->max_width)
    	    gdb_fprintf(disasm_info->stream, " %.*s\n", disasm_info->max_width, function);
    	else
    	    gdb_fprintf(disasm_info->stream, " %s\n", function);
    }
    
    memcpy(formatted + 2 + (7-n_offset), offset, n_offset);	/* +dddddd		*/
    
    p1 = formatted + 10;					/* aaaaaaaa[...]	*/
    addr_width = (n_addr < 10) ? 8 : n_addr - 2;
    memset(p1, '0', addr_width);
    memcpy(p1 + addr_width + 2 - n_addr, address + 2, n_addr - 2);
    
    eol = formatted + 12 + addr_width;				/* xxxxxxxx		*/
    eol += sprintf(eol, "%.8lx", instruction);
    *eol = ' ';
    
    eol = formatted + 23 + addr_width;				/* opcode		*/
    memcpy(eol, opcode, n_opcode);
    eol += n_opcode;
    *eol = ' ';
    
    if (n_operand) {						/* operand		*/
    	eol = formatted + 34 + addr_width;
    	eol = memcpy(eol, operand, n_operand);
    	eol += n_operand;
    	*eol = ' ';
    }
    
    if (find_breakpt(disasm_info->addr) >= 0)			/* flag breakpoints	*/
	formatted[21 + addr_width] = '.';
    
    introduced_comment = 0;
    
    if (disasm_info->addr == disasm_info->pc) {			/* special pc handling	*/
	if (disasm_info->flags & FLAG_PC)		 	/* flag if requested	*/
	    formatted[22 + addr_width] = '*';
	n = branch_taken(instruction);				/* cvt branch info	*/
	if (n == 1)
	    disasm_info->flags |= BRANCH_TAKEN;
	else if (n == -1)
	    disasm_info->flags |= BRANCH_NOT_TAKEN;
	
	/* If we are allowed to show Objective C selectors (show_selectors) and this	*/
	/* instruction is a call to one of the objc message dispatch functions, try to 	*/
	/* replace the dispatch function name in the comment with the selector name.	*/
	
	if (show_selectors && n_comment && (p1 = strstr(comment, "objc_msgSend")) != NULL) {
	    GDB_ADDRESS sel_name_addr, ok = 1;
	    char        *reg, *prefix, c1;
	    
	    p1 += sizeof("objc_msgSend") - 1;
	    if (*p1 == '>' || strncmp(p1, "_rtp>", 5) == 0) {/* objc_msgSend[_rtp]	*/
		reg    = "$r4";
		prefix = "";
	    } else if (strncmp(p1, "Super>", 6) == 0) {      /* objc_msgSendSuper	*/
		reg    = "$r4";
		prefix = "super ";
	    } else if (strncmp(p1,"Super_stret>", 12) == 0) {/* objc_msgSendSuper_stret	*/
		reg    = "$r5";
		prefix = "(stret) super ";
	    } else if (strncmp(p1, "_stret>", 7) == 0) {     /* objc_msgSend_stret	*/
		reg    = "$r5";
		prefix = "(stret) ";
	    } else
		ok = 0;
	    
	    if (ok && gdb_get_register(reg, &sel_name_addr) &&
	    	gdb_read_memory_from_addr(&c1, sel_name_addr++, 1, 0)) {
		n_comment = sprintf(comment, "%s", prefix);
		p1 = comment + n_comment;
		*p1 = c1;
		
		while (ok && *p1++ && ++n_comment < 1023)
		    ok = gdb_read_memory_from_addr(p1, sel_name_addr++, 1, 0);
		
		*--p1 = '\0';
	    }
	} else if (n_comment > 0) {
	    comment[n_comment-1] = '\0';
	    n_comment = gdb_demangled_symbol(comment+1, comment+1, 1022, 0) + 2;
	    comment[n_comment-1] = '>';
	    comment[n_comment]   = '\0';
	}
	
	#ifndef not_ready_for_prime_time_but_keep_enabled_for_private_testing
	/* If we still don't have a comment then try to comment selected instructions	*/
	/* with the information associated with the source or target (depending on the	*/
	/* instruction).								*/

	/* THIS CODE IS STILL EXPERIMENTAL!						*/
	
	if (n_comment == 0 && comment_insns) {
	   n_comment = try_to_comment_insn(disasm_info->pc, instruction, comment, 1023);
	   introduced_comment = (n_comment > 0) && (disasm_info->comm_max > 0);
	   
	   /* Note, originally it was thought we could save the comment so that if the	*/
	   /* pc didn't change we could avoid redoing all the original work all over	*/
	   /* again when SO or SI commands are done since the line at the pc moves "up 	*/
	   /* into" the history area.  Seemed like a good idea at the time until the	*/
	   /* case came up where the a patch is made to something involved with the	*/
	   /* display possibly invalidating it.						*/
	   
	   /* Also note, introduced_comment is a switch we set here to unconditionally	*/
	   /* truncate these comments to the output width.  We may be trying to display	*/
	   /* the value of some Objective C object.  And these could get very large. So	*/
	   /* lets not get carried away here trying to be cute.  Just show a little of	*/
	   /* the value so as to not clutter up the disassembly.  This should be enough	*/
	   /* to suggest what the value is.  If the user really wants the whole thing 	*/
	   /* s/he can do the PO command as would have been done without all this stuff.*/
	}
	#endif
    }
    
    if (n_comment) {						/* comment		*/
	p1 = formatted + 47 + addr_width;
	eol = (p1 > eol) ? p1 : eol + 1;
	strcpy(eol, comment);
	eol += n_comment;
	if (disasm_info->flags & DISASM_PC_AREA)
	    force_pc_area_update();
    }
    
    /* Truncate line if necessary.  disasm_info->max_width > 0 implies absolute 	*/
    /* truncation to that length.  introduced_comment implies truncate line only if we	*/
    /* introduced our own comment and disasm_info->comm_max > 0.  If both occur then	*/
    /* use the smaller of the two widths.  And of course if neither are required then	*/
    /* use all we got.									*/
    
    len = eol - formatted;					/* don't count '\n'	*/
    
    if (disasm_info->max_width > 0 && introduced_comment) {	/* have both widths	*/
	if (disasm_info->max_width <= disasm_info->comm_max) {	/* use smaller value	*/
	    if (len > disasm_info->max_width)
		eol = formatted + disasm_info->max_width;
	} else {
	    if (len > disasm_info->comm_max)
		eol = formatted + disasm_info->comm_max;
	}
    } else if (disasm_info->max_width > 0) {			/* have on max_width	*/
	if (len > disasm_info->max_width)
	    eol = formatted + disasm_info->max_width;
    } else if (introduced_comment) {				/* have only comm_max	*/
	if (len > disasm_info->comm_max)
	    eol = formatted + disasm_info->comm_max;
    }
     
    if (((disasm_info->flags & NO_NEWLINE) == 0))
    	*eol++ = '\n';
    *eol = '\0';
   
    /* Writing to the macsbug screen takes care of line wrapping for us.  But if we are	*/
    /* not writing to the macsbug screen and we are going to display the sidebar we 	*/
    /* need to handle the wrapping here ourselves.  Otherwise the sidebar would cut off	*/
    /* the right side of the line.							*/
    
    if (disasm_info->flags & WRAP_TO_SIDEBAR) {
    	len = eol - formatted;
    	if ((len - 1) < (max_cols - wrap_col))
    	    gdb_fprintf(disasm_info->stream, "%s", formatted);
    	else {
    	    wrap = max_cols - (wrap_col + 1);
    	    p1 = formatted;
    	    while (len >= wrap) {
    	    	c  = *(p1+wrap);
    	    	c2 = *(p1+wrap+1);
    	    	*(p1+wrap)   = '\n';
    	    	*(p1+wrap+1) = '\0';
    	    	gdb_fputs(p1, disasm_info->stream);
    	    	len -= wrap;
    	    	p1 += wrap;
    	    	*p1 = c;
    	    	*(p1+1) = c2;
    	    }
    	    if (len > 0)
    	        gdb_fputs(p1, disasm_info->stream);
    	}
    } else
    	gdb_fprintf(disasm_info->stream, "%s", formatted);
    
    disasm_info->addr += 4;
    
    return (NULL);
}


/*--------------------------------------------------------------------------------------*
 | __disasm addr n [noPC] - disassemble n lines from addr (optionally suppress pc flag) |
 *--------------------------------------------------------------------------------------*
 
 If noPC is specified then suppress flagging the PC line.
*/
 
void __disasm(char *arg, int from_tty)
{
    int  	argc, n, nopc = 0, show_sidebar;
    GDB_ADDRESS addr, limit;
    DisasmData	disasm_info;
    GDB_FILE	*redirect_stdout, *prev_stdout;
    char 	*argv[6], tmpCmdLine[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__disasm", &argc, argv, 5);
    
    nopc = (argc == 4) ? (gdb_get_int(argv[3]) != 0) : 0;
    
    if (argc < 3)
    	gdb_error("usage: __disasm addr n [noPC] (wrong number of arguments)");
    
    /* Sometimes gdb will print an extra line at the start of the first x/i done in a	*/
    /* session.  For example, for Cocoa inferiors, it will print something like,	*/
    
    /*      Current language:  auto; currently objective-c				*/
    
    /* This will confuse the disassembly reformatting.  We can fake gdb out, however,	*/
    /* by giving it "x/0i 0".  It won't do anything if gdb has nothing additional to	*/
    /* say.  But if it does, the additional stuff is all it will say.  This will print	*/
    /* to the current output stream which is where we want it to go.			*/
    
    gdb_execute_command("x/0i 0");
    
    addr  = gdb_get_address(argv[1]);
    limit = addr + 4 * gdb_get_long(argv[2]);
   
    /* All disassembly output is filtered through format_disasm_line() so that we may	*/
    /* reformat the lines the way we want them...					*/
    
    redirect_stdout = gdb_open_output(stdout, format_disasm_line, &disasm_info);
    prev_stdout     = gdb_redirect_output(redirect_stdout);
    
    show_sidebar = (gdb_target_running() && !macsbug_screen && sidebar_state && isatty(STDOUT_FILENO));
    
    if (!gdb_get_register("$pc", &disasm_info.pc))
    	disasm_info.pc = gdb_get_int("$pc");
    
    if (!wrap_lines && !macsbug_screen) {	/* truncate non-macsbug screen lines?	*/
	get_screen_size(&max_rows, &max_cols);
	disasm_info.max_width = max_cols - 1 - (show_sidebar ? ((target_arch == 4) ? 12 : 20) : 0);
    	disasm_info.comm_max  = disasm_info.max_width;
    } else {
        disasm_info.max_width = -1;
        disasm_info.comm_max = max_cols;
        if (macsbug_screen)
            disasm_info.comm_max -= ((target_arch == 4) ? 12 : 20) + 2;
        else					/* always truncate introduced comments	*/
            disasm_info.comm_max -= (show_sidebar ? ((target_arch == 4) ? 12 : 20) : 0);
    }
    
    disasm_info.flags	  = (nopc ? 0 : FLAG_PC) | (show_sidebar ? WRAP_TO_SIDEBAR : 0);
    disasm_info.stream    = prev_stdout;
    
    disasm_info.addr = addr;
    gdb_execute_command("x/%ldi 0x%llx", (long)(limit-addr)/4, (long long)addr);
    
    gdb_close_output(redirect_stdout);
    
    /* The "branch taken" info is handled by display_pc_area() when the MacsBug screen	*/
    /* is on...										*/
    
    if (disasm_info.flags & BRANCH_TAKEN) {
    	gdb_set_int("$branch_taken", branchTaken = 1);
    	if (!macsbug_screen)
    	    gdb_printf("Will branch\n");
    } else if (disasm_info.flags & BRANCH_NOT_TAKEN) {
    	gdb_set_int("$branch_taken", branchTaken = -1);
    	if (!macsbug_screen)
    	    gdb_printf("Will not branch\n");
    } else
    	gdb_set_int("$branch_taken", branchTaken = 0);
    
    if (show_sidebar)
    	__display_side_bar("right", 0);
}

#define __DISASM_HELP \
"__DISASM addr n [noPC] -- Disassemble n lines from addr (suppress pc flag).\n"	\
"The default is to always flag the PC line with a '*' unless noPC is specified\n"	\
"and is non-zero.  If the PC line is a conditional branch then an additional\n"		\
"line is output indicating whether the branch will be taken and $branch_taken\n"	\
"is set to 1 if the branch will br taken, -1 if it won't be taken.  In all\n"		\
"other cases $branch_taken is set to 0.\n"						\
"\n"											\
"As a byproduct of calling __DISASM the registers are displayed in a side bar\n"	\
"on the right side of the screen IF the MacsBug screen is NOT being used.\n"		\
"\n"											\
"Enabled breakpoints are always flagged with a '.'.\n"					\
"\n"											\
"Note that this command cannot be used unless the target program is currently\n"	\
"running.  Use __is_running to determine if the program is running."


/*----------------------------------------*
 | __error msg - display an error message |
 *----------------------------------------*
 
 The message is reported as an error and the command terminated.
*/

static void __error(char *arg, int from_tty)
{
    gdb_error("%s", arg);
}

#define __ERROR_HELP "__ERROR string -- Abort command with an error message."


/*-------------------------------------------------------*
 | __getenv name - access the named environment variable |
 *-------------------------------------------------------*
 
 Sets convenience variable "$name" with the string represening the environment variable
 or a null string if undefined.
*/

static void __getenv(char *arg, int from_tty)
{
    int       numeric;
    long long lvalue;
    char      *p, *value, name[1024];
    
    if (!arg || !*arg)
    	gdb_error("__getenv expects a environemnt variable name");
    
    name[0] = '$';
    strcpy(&name[1], arg);
	
    value = getenv(arg);
    
    numeric = (value && *value);
    if (numeric) {
    	lvalue = strtoll(value, &p, 0);
	numeric = (p > value);
	if (numeric) {
	    while (*p && *p == ' ' || *p == '\t')
	    	++p;
	    numeric = (*p == '\0');
	}
    }
    
    if (numeric)
    	gdb_set_long_long(name, lvalue);
    else if (!gdb_target_running())
    	gdb_set_int(name, (value != NULL));
    else
	gdb_set_string(name, value ? value : "");
    
    gdb_set_int("$__undefenv__", value == NULL);
}

#define __GETENV_HELP \
"__GETENV name -- Get the value of an environment variable.\n"				\
"Sets $name with value of environment variable \"name\".  If the\n"			\
"environment variable does not exist $name is set to a null string\n"			\
"and $__undefenv__ set to 1.\n"								\
"\n"											\
"If the environment variable is numeric then the type of $name is\n"			\
"an integer that contains the environment variable's value.  If it\n"			\
"isn't numeric, $name is the environment variable's string value.\n"			\
"\n"											\
"Note, due to the way gdb stores string convenience variables, $name\n"			\
"CANNOT be a string unless the target program is running!  Thus when\n"			\
"the target is NOT running, and the environment variable value is\n"			\
"not numeric, $name is set to 1 if the variable exists and 0 if it\n"			\
"doesn't exist.  $__undefenv__ is still set accordingly."


/*-------------------------------------------------------------------*
 | __hexdump [addr [n]] - dump n bytes starting at addr (default pc) |
 *-------------------------------------------------------------------*
 
 The bytes are shown as hexdump_width bytes per line in groups of hexdump_width.
*/
 
void __hexdump(char *arg, int from_tty)
{
    int  	  argc, i, j, k, n, offset, repeated, repcnt, extra_space_ok, show_line, width;
    GDB_ADDRESS   addr;
    char 	  *start, *argv[5], tmpCmdLine[1024], addrexp[1024];
    unsigned char *c, x, data[1024], prev[1024], charline[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__hexdump", &argc, argv, 4);
    
    if (argc == 1) {
    	start = "$pc";
    	n     = hexdump_width;
    } else if (argc == 2) {
    	start = argv[1];
    	n     = hexdump_width;
    } else if (argc == 3) {
    	start = argv[1];
    	n     = gdb_get_int(argv[2]);
    } else
    	gdb_error("__hexdump called with wrong number of arguments.");
    
    width = hexdump_width < n ? hexdump_width : n;
    extra_space_ok = (n % 4) == 0 &&
    		     ((hexdump_group % 4) == 0 || hexdump_group == 2) &&
    		     hexdump_group < n;
    
    for (repcnt = offset = 0; offset < n; offset += width) {
    	sprintf(addrexp, "(char *)(%s)+%ld", start, offset);
	
    	if (offset + width < n) {
    	    addr     = gdb_read_memory(data, addrexp, width);
    	    repeated = ditto && (memcmp(data, prev, width) == 0);
    	} else {
    	    addr     = gdb_read_memory(data, addrexp, n - offset);
    	    repeated = 0;
    	}
    	
    	memcpy(prev, data, width);
	
    	if (repeated)
	    show_line = (repcnt++ == 0);
    	else {
	    repcnt = 0;
    	    show_line = 1;
    	}
    	
    	if (show_line) {
    	    gdb_printf(" %.8llX: ", (long long)addr);
    	    
	    for (i = j = 0, c = charline; i < width; ++i) {
		if (i > 0) {
		    if (extra_space_ok && (addr % 8) == 0 && (i % 8) == 0)
			gdb_printf(" ");
		    if (++j >= hexdump_group) {
			gdb_printf(" ");
			j = 0;
		    }
		}
		
		if (offset + i < n) {
		    if (repcnt != 1) {
		      x = data[i];
		      gdb_printf("%1X%1X", (x>>4) & 0x0F, x & 0x0F);
		      *c++ = isprint((int)x) ? (unsigned char)x : (unsigned char)'.';
		    } else {
		      gdb_printf("''");
		      *c++ = (unsigned char)'\'';
		    }
		} else
		    gdb_printf("  ");
	    }
            
	    *c = '\0';
	    gdb_printf("  %s\n", charline);
    	}
    }
}

#define __HEXDUMP_HELP \
"__HEXDUMP [addr [n]] -- Dump n bytes starting at addr (default pc).\n"			\
"\n"											\
"The default number of bytes dumped is 16 in groups of 4 bytes.  Both\n"		\
"the default width and grouping may be set by the SET mb-hexdump-width\n"		\
"and SET mb-macsbug-group commands respectively."


/*-------------------------------------------------------------------*
 | __is_running [msg] - test to see if target is currently being run |
 *-------------------------------------------------------------------*
 
 If the target is being run set $__running__ to 1.  Otherwise set $__running__ to 0.
 If the message is omitted just return so that the caller can test $__running__.  If a
 message is specified report it as an error and abort the command.
*/
  
void __is_running(char *arg, int from_tty)
{
    int running = gdb_target_running();
    
    gdb_set_int("$__running__", running);
    
    if (!running && arg && *arg)
    	 gdb_error("%s", arg);
}

#define __IS_RUNNING_HELP \
"__IS_RUNNING [msg] -- Test to see if target is currently being run.\n"			\
"Sets $__running__ to 1 if the target is running and 0 otherwise.\n"			\
"\n"											\
"If a msg is supplied and the target is not running then the command\n"			\
"is aborted with the error message just as if __error msg was issued."


/*--------------------------------------------------*
 | __is_string arg - determine if arg is a "string" |
 *--------------------------------------------------*
 
 Returns $string set to the a pointer to the string if arg is a string and $string
 is set to 0 if the arg is not a string.
*/

static void __is_string(char *arg, int from_tty)
{
    char *p, str[1024];
    
    if (gdb_is_string(arg)) {
    	gdb_get_string(arg, str, 1023);
    	p = (char *)gdb_malloc(strlen(str) + 1);
    	strcpy(p, str);
    	gdb_set_string("$string", p);
    } else
    	gdb_set_int("$string", 0);
}

#define __IS_STRING_HELP \
"__IS_STRING arg -- Tries to determine if arg is a double quoted \"string\".\n"		\
"Sets $string to 1 if the arg is a string and 0 otherwise."


/*--------------------------------------------------*
 | __lower_case word - convert a word to lower case |
 *--------------------------------------------------*

 Returns $lower_case as a string containing the lower cased copy of the key word. Also
 assumes we're currently executing or string operations cause errors.
*/

static void __lower_case(char *arg, int from_tty)
{
    char c, *p, *w, str[1024];
    
    if (*arg != '"') {
    	sprintf(str, "$lower_case = (char *)\"%s\"", arg);
    	gdb_eval(str);
    	p = arg = gdb_get_string("$lower_case", str, 1023);
    } else
    	gdb_set_string("$lower_case", arg);
	
    p = arg = gdb_get_string("$lower_case", str, 1023);
    
    while ((c = *p) != 0)
	*p++ = tolower(c);
     
    w = (char *)gdb_malloc(strlen(arg) + 1);
    strcpy(w, arg);
    gdb_set_string("$lower_case", w);
}

#define __LOWER_CASE_HELP \
"__LOWER_CASE word -- Convert a word or string to lower case.\n"			\
"Sets $lower_case to the lower case copy of the word."


/*-----------------------------------------------------*
 | __memcpy addr src n - copy n bytes from src to addr |
 *-----------------------------------------------------*
 
 The addr is always an address but the src can either be a double quoted "string" or
 a expression representing an integer value.  For moving integer values n must be 1,
 2, or 4.  For strings n can be any length.
*/

static void __memcpy(char *arg, int from_tty)
{
    int  argc, n;
    long v;
    char *argv[6], *src, str[1024], tmpCmdLine[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__memcpy", &argc, argv, 5);
    
    if (argc != 4)
    	gdb_error("__memcpy called with wrong number of arguments");
    
    n = gdb_get_int(argv[3]);
    
    if (gdb_is_string(argv[2]))
    	src = gdb_get_string(argv[2], str, 1023);
    else {
    	if (n != 1 && n != 2 && n != 4)
    	    gdb_error("__memcpy of a int value with size not 1, 2, or 4.");
    	v = gdb_get_long(argv[2]);
    	src = (char *)&v + (4 - n);
    }
        
    gdb_write_memory(argv[1], src, n);
}

#define __MEMCPY_HELP \
"__MEMCPY addr src n -- Copy n bytes from src to addr."


/*-----------------------------------------------------------------*
 | printf_stdout_filter - stdout output filter for __printf_stderr |
 *-----------------------------------------------------------------*
 
 The __printf_stderr command uses this filter to redirect stdout output from a printf
 command to stderr.
*/

static char *printf_stdout_filter(FILE *f, char *line, void *data)
{
    if (line)
    	gdb_fprintf(*(GDB_FILE **)data, "%s", line);
	
    return (NULL);
}


/*--------------------------------------------------------*
 | __printf_stderr fmt,args... - printf command to stderr |
 *--------------------------------------------------------*
 
 The standard printf gdb command always goes to stdout.  The __printf_stderr is identical
 except that the output goes to stderr.
*/

static void __printf_stderr(char *arg, int from_tty)
{
    GDB_FILE *prev_stderr;
    GDB_FILE *redirect_stdout = gdb_open_output(stdout, printf_stdout_filter, &prev_stderr);
    GDB_FILE *redirect_stderr = gdb_open_output(stderr, printf_stdout_filter, &prev_stderr);
    
    prev_stderr = gdb_redirect_output(redirect_stderr);
    gdb_redirect_output(redirect_stdout);
    
    gdb_printf_command(arg, from_tty);
    
    gdb_close_output(redirect_stderr);
    gdb_close_output(redirect_stdout);
}

#define __PRINTF_STDERR_HELP \
"__PRINTF_STDERR \"gdb printf args\" -- printf to stderr.\n" \
"Gdb's PRINTF always goes to stdout.  __PRINTF_STDERR is identical except that the\n"	   \
"output goes to stderr."


/*-----------------------------------------------*
 | filter_char - convert a character to a string |
 *-----------------------------------------------*
 
 Internal routine to convert a character, c, to a string.  If the character needs to be
 escaped it is returned as the string "\c".  If the character is outside the range 0x20
 to 0x7F then it is returned as a pointer to a static string formatted as an escaped
 octal ("\ooo") with xterm controls surrounding it to display it in bold.
 
 Note that if isString is non-zero then the character is in the context of a double-
 quoted "string".  In that case a double quote is escaped.  Conversely, if isString is
 0 then we assume we're in the context of a single quoted 'string'.  Then single quotes
 are escaped.
*/

char *filter_char(int c, int isString, char *buffer)
{
    c &= 0xFF;
    
    switch (c) {
    	case '\n': return ("\\n");
	case '\r': return ("\\r");
	case '\t': return ("\\t");
	case '\a': return ("\\a");
	case '\f': return ("\\f");
	case '\b': return ("\\b");
	case '\v': return ("\\v");
	case '\'': return (isString ? "'" : "\\'");
	case '"' : return (isString ? "\\\"" : "\"");
	default  : if (c < 0x20 || c >= 0x7F)
			//sprintf(buffer, COLOR_BOLD "\\%03o" COLOR_OFF, c);
			sprintf(buffer, "\\%03o", c);
		   else {
		   	buffer[0] = c;
		   	buffer[1] = 0;
		   }
		   return (buffer);
    }
}


/*-----------------------------------------------------------*
 | __print_char c [s] - print a (possibly escaped) character |
 *-----------------------------------------------------------*

 If s (actually any 2nd argument) is specified then this character is in the context
 of a "string" as opposed to a 'string' (i.e., the surrounding quotes on the string)
 so that a single quote does not need to be escaped.  On the other hand if this is the
 context of a double-quoted string then double quotes need to be escaped.
*/

static void __print_char(char *arg, int from_tty)
{
    int  argc;
    char *argv[5], buf[20], tmpCmdLine[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__print_char", &argc, argv, 4);
    if (argc != 2 && argc != 3)
    	gdb_error("__print_charx called with wrong number of arguments");
        
    gdb_printf("%s", filter_char(gdb_get_int(argv[1]), argc == 3, buf));
}

#define __PRINT_CHAR_HELP \
"__print_char c [s] -- Print a (possibly escaped) character.\n"				\
"Just echos the c to stdout unless it's to be represented as a escaped\n"		\
"character (e.g., \\n) or octal (bolded) if not one of the standard escapes,\n"		\
"or quotes.\n"										\
"\n"											\
"If s (actually any 2nd argument) is specified then the character is in\n"		\
"the context of a \"string\" as opposed to a 'string' (i.e., the surrounding\n"		\
"quotes on the string) so that a single quote does not need to be escaped.\n"		\
"On the other hand if this is the  context of a double-quoted string then\n"		\
"double quotes need to be escaped."


/*-----------------------------------------*
 | __print_1 x - print one character ('x') |
 *-----------------------------------------*/
 
void __print_1(char *arg, int from_tty)
{
    char buf[20];
    
    gdb_printf("'%s'", filter_char(gdb_get_int(arg), 0, buf));
}

#define __PRINT_1_HELP \
"__PRINT_1 x -- Print 1 character surrounded with single quotes ('x').\n"		\
"Special characters are shown escaped or bold octal."


/*-----------------------------------------*
 | __print_2 - print two characters ('xy') |
 *-----------------------------------------*/

void __print_2(char *arg, int from_tty)
{   
    char buf1[20], buf2[20];
    unsigned long x2 = gdb_get_int(arg);

    gdb_printf("'%s%s'", filter_char(x2 >> 8, 0, buf1),
    			 filter_char(x2     , 0, buf2));
}

#define __PRINT_2_HELP \
"__PRINT_2 xy -- Print 1 characters surrounded with single quotes ('xy').\n"		\
"Special characters are shown escaped or bold octal."


/*--------------------------------------------*
 | __print_4 - print four characters ('wxyz') |
 *--------------------------------------------*/

void __print_4(char *arg, int from_tty)
{
   char buf1[20], buf2[20], buf3[20], buf4[20];
   unsigned long x4 = gdb_get_long(arg);
   
   gdb_printf("'%s%s%s%s'", filter_char(x4 >> 24, 0, buf1),
 			    filter_char(x4 >> 16, 0, buf2),
 			    filter_char(x4 >>  8, 0, buf3),
 			    filter_char(x4      , 0, buf4));
}

#define __PRINT_4_HELP \
"__PRINT_4 wxyz -- Print 4 characters surrounded with single quotes ('wxyz').\n"		\
"Special characters are shown escaped or bold octal."


/*-------------------------------------------------*
 | __print_8 - print eight characters ('abcdwxyz') |
 *-------------------------------------------------*/

void __print_8(char *arg, int from_tty)
{
   char buf1[20], buf2[20], buf3[20], buf4[20],  buf5[20], buf6[20], buf7[20], buf8[20];
   unsigned long long x8 = gdb_get_long_long(arg);
   
   gdb_printf("'%s%s%s%s%s%s%s%s'", filter_char(x8 >> 56, 0, buf1),
   				    filter_char(x8 >> 48, 0, buf2),
   				    filter_char(x8 >> 40, 0, buf3),
   				    filter_char(x8 >> 32, 0, buf4),
   				    filter_char(x8 >> 24, 0, buf5),
				    filter_char(x8 >> 16, 0, buf6),
				    filter_char(x8 >>  8, 0, buf7),
				    filter_char(x8      , 0, buf8));
}

#define __PRINT_8_HELP \
"__PRINT_8 abcdwxyz -- Print 48 characters surrounded with single quotes ('abcdwxyz').\n"		\
"Special characters are shown escaped or bold octal."


/*-------------------------------------------------------------------------*
 | __reset_current_function - reset the current function for disassemblies |
 *-------------------------------------------------------------------------*/
 
void __reset_current_function(char *arg, int from_tty)
{
    curr_function1[0] = '\0';
    curr_function2[0] = '\0';
}

#define __RESET_CURRENT_FUNCTION_HELP \
"__RESET_CURRENT_FUNCTION -- Make __DISASM forget which function it's in\n"		\
"The __DISASM will not display the function name unless it changes since the\n"		\
"since last time it disassembled a line.  The __reset_current_function forces it\n"	\
"to forget so that the next __DISASM disassembly line will be preceded by\n"		\
"the function name to which it belongs."


/*-------------------------------------------------------------------------------*
 | __setenv name [value] - set environment variable name to value (or delete it) |
 *-------------------------------------------------------------------------------*/

static void __setenv(char *arg, int from_tty)
{
    int  argc;
    char *argv[5], tmpCmdLine[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__setenv", &argc, argv, 4);
    
    if (argc == 3)
    	(void)setenv(argv[1], argv[2], 1);
    else if (argc == 2)
    	unsetenv(argv[1]);
    else
    	gdb_error("__setenv called with wrong number of arguments");
}

#define __SETENV_HELP \
"__SETENV name [value] -- Set environment variable name to value (or delete it).\n"	\
"Note, setting environment variables from gdb has NO EFFECT on your shell's\n"		\
"environment variables other than locally changing their values while gdb is\n"		\
"loaded."


/*----------------------------------------------------------*
 | __strcmp str1 str2 - compare string compare for equality |
 *----------------------------------------------------------*
 
 Sets $strcmp to 1 if str1 == str2 and 0 if str1 != str2.
*/

static void __strcmp(char *arg, int from_tty)
{
    int  argc;
    char *argv[5], str1[1024], str2[1024], tmpCmdLine[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__strcmp", &argc, argv, 4);
    
    if (argc != 3)
    	gdb_error("__strcmp called with wrong number of arguments");
    
    gdb_get_string(argv[1], str1, 1023);
    gdb_get_string(argv[2], str2, 1023);
    	
    gdb_set_int("$strcmp", strcmp(str1, str2) == 0);
}

#define __STRCMP_HELP \
"__STRCMP str1 str2 -- Compare string compare for equality.\n"				\
"Sets $strcmp to 1 if they are equal and 0 if not equal."				\


/*--------------------------------------------------------------------*
 | __strcmpl str1 str2 - case-independent string compare for equality |
 *--------------------------------------------------------------------*
 
 Sets $strcmpl to 1 if str1 == str2 and 0 if str1 != str2.
*/

static void __strcmpl(char *arg, int from_tty)
{
    int  argc, i;
    char *argv[5], str1[1024], str2[1024], tmpCmdLine[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__strcmpl", &argc, argv, 4);
        
    if (argc != 3)
    	gdb_error("__strcmpl called with wrong number of arguments");
    	
    gdb_get_string(argv[1], str1, 1023);
    gdb_get_string(argv[2], str2, 1023);
        
    gdb_set_int("$strcmpl", gdb_strcmpl(str1, str2) == 1);
}

#define __STRCMPL_HELP \
"__STRCMPL str1 str2 -- Compare string compare for equality (ignoring case).\n"		\
"Sets $strcmpl to 1 if they are equal and 0 if not equal."


/*----------------------------------------------------------------------------*
 | __strncmp str1 str2 n - compare string compare for equality (n characters) |
 *----------------------------------------------------------------------------*
 
 Sets $strncmp to 1 if str1 == str2 and 0 if str1 != str2.
*/

static void __strncmp(char *arg, int from_tty)
{
    int  argc, n;
    char *argv[6], str1[1024], str2[1024], tmpCmdLine[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__strncmp", &argc, argv, 5);
    
    if (argc != 4)
    	gdb_error("__strncmp called with wrong number of arguments");
    
    gdb_get_string(argv[1], str1, 1023);
    gdb_get_string(argv[2], str2, 1023);
    n = gdb_get_int(argv[3]);
    
    gdb_set_int("$strncmp", strncmp(str1, str2, n) == 0);
}

#define __STRNCMP_HELP \
"__STRNCMP str1 str2 n -- Compare string compare for equality (n characters).\n"	\
"Sets $strncmp to 1 if they are equal and 0 if not equal."


/*------------------------------------------------*
 | __strlen str - return length of str in $strlen |
 *------------------------------------------------*/

static void __strlen(char *arg, int from_tty)
{
    char *p, str[1024];
    
    p = gdb_get_string(arg, str, 1023);
    gdb_set_int("$strlen", strlen(p));
}

#define __STRLEN_HELP \
"__STRLEN string -- Get the length of the string.\n" \
"Sets $strlen with the length of the string."


/*-------------------------------------------------------------------------------------*
 | __window_size - set $window_rows and $window_cols to the current window screen size |
 *-------------------------------------------------------------------------------------*/
 
void __window_size(char *arg, int from_tty)
{
    get_screen_size(&max_rows, &max_cols);
    
    gdb_set_int("$window_rows", max_rows);
    gdb_set_int("$window_cols", max_cols);
}

#define __WINDOW_SIZE_HELP \
"__WINDOW_SIZE -- Get the current window screen size.\n" \
"Sets $window_rows and $window_cols to current window size."

/*--------------------------------------------------------------------------------------*/

#include "MacsBug_testing.i"

/*--------------------------------------------------------------------------------------*/

/*-----------------------------------------------------*
 | init_macsbug_utils - initialize the utility plugins |
 *-----------------------------------------------------*/

void init_macsbug_utils(void)
{
    MACSBUG_USEFUL_COMMAND(__asciidump,      	     __ASCIIDUMP_HELP);
    MACSBUG_USEFUL_COMMAND(__binary,		     __BINARY_HELP);
    MACSBUG_USEFUL_COMMAND(__command_exists, 	     __COMMAND_EXISTS_HELP);
    MACSBUG_USEFUL_COMMAND(__disasm,		     __DISASM_HELP);
    MACSBUG_USEFUL_COMMAND(__error,		     __ERROR_HELP);
    MACSBUG_USEFUL_COMMAND(__getenv,	     	     __GETENV_HELP);
    MACSBUG_USEFUL_COMMAND(__hexdump,		     __HEXDUMP_HELP);
    MACSBUG_USEFUL_COMMAND(__is_running,     	     __IS_RUNNING_HELP);
    MACSBUG_USEFUL_COMMAND(__is_string,      	     __IS_STRING_HELP);
    MACSBUG_USEFUL_COMMAND(__lower_case,     	     __LOWER_CASE_HELP);
    MACSBUG_USEFUL_COMMAND(__memcpy,		     __MEMCPY_HELP);
    MACSBUG_USEFUL_COMMAND(__printf_stderr,  	     __PRINTF_STDERR_HELP);
    MACSBUG_USEFUL_COMMAND(__print_char,     	     __PRINT_CHAR_HELP);
    MACSBUG_USEFUL_COMMAND(__print_1,		     __PRINT_1_HELP);
    MACSBUG_USEFUL_COMMAND(__print_2,		     __PRINT_2_HELP);
    MACSBUG_USEFUL_COMMAND(__print_4,		     __PRINT_4_HELP);
    MACSBUG_USEFUL_COMMAND(__print_8,		     __PRINT_8_HELP);
    MACSBUG_USEFUL_COMMAND(__reset_current_function, __RESET_CURRENT_FUNCTION_HELP);
    MACSBUG_USEFUL_COMMAND(__setenv, 	     	     __SETENV_HELP);
    MACSBUG_USEFUL_COMMAND(__strcmp, 	     	     __STRCMP_HELP);
    MACSBUG_USEFUL_COMMAND(__strcmpl,		     __STRCMPL_HELP);
    MACSBUG_USEFUL_COMMAND(__strncmp,		     __STRNCMP_HELP);
    MACSBUG_USEFUL_COMMAND(__strlen,		     __STRLEN_HELP);
    MACSBUG_USEFUL_COMMAND(__window_size,    	     __WINDOW_SIZE_HELP);
   
    add_testing_commands(); 		/* for trying out new things in testing.c	*/
}
