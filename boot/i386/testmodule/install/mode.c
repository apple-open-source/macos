/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#import "libsa.h"
#import "libsaio.h"
#import "install.h"

void install_enterInstallMode(void)
{
    int answer, count, size;
    char *LanguageTable, *val;
    char *LoadableFamilies;
    
    /* Load language choice table */
    loadConfigFile("/usr/standalone/i386/Language.table",
	&LanguageTable, 1);
    
    /* Get language choices. */
    if (LanguageTable &&
	getValueForStringTableKey(
	    LanguageTable, "Languages", &val, &count))
    {
	char *string, *languages[16], *language_strings[16];
	int nlang = 0;
    
	while (string = (char *)newStringFromList(&val, &count)) {
	    languages[nlang] = string;
	    language_strings[nlang++] = (char *)
		newStringForStringTableKey(
		    LanguageTable, string);
	}
    
	for(answer = -1; answer == -1;) {
	    install_clearScreen();
	    answer = install_choose(language_strings, nlang,
		1, nlang);
	}
	setLanguage(languages[answer - 1]);
	while (nlang)
	    zfree(language_strings[nlang--]);
	/* Don't bother freeing the language names. */
	
	zfree(LanguageTable);
    } else {
	printf("Could not load language choice file; "
	    "defaulting to English.\n");
	setLanguage("English");
    }
    
    /* Now save the language in the "Language" key,
	* which has cleverly been made large for us.
	*/
    if (getValueForKey("Language", &val, &size)) {
	char *Language = getLanguage();
	if ((strlen(Language)+2) < size) {
	    strcpy(val, Language);
	    val += strlen(Language);
	    *val++ = '\"';
	    *val++ = ';';
	    *val++ = '\n';
	    size -= strlen(Language) + 1;
	    while (size--)
		*val++ = ' ';
	}
    }
    
    /* Now limit the families we look for when considering
	* drivers during installation.
	*/
    if (getValueForKey("Installation Driver Families",
					    &val, &size)) {
	LoadableFamilies = zalloc(size+1);
	strncpy(LoadableFamilies,val,size);
    }
}