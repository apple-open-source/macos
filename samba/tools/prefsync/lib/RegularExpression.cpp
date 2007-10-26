/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
#include "macros.hpp"
#include "common.hpp"
#include <regex.h>

/* All REG_* errors are greater than 0, so this is OK to indicate no match. */
const int RegularExpression::NOMATCH = -1;

RegularExpression::~RegularExpression()
{
    if (this->m_preg) {
	regex_t * preg = (regex_t *)(this->m_preg);

	regfree(preg);
	delete preg;
    }

    if (this->m_errstr) {
	std::free(this->m_errstr);
    }
}

int RegularExpression::compile(const char * pattern)
{
    regex_t * preg = (regex_t *)(this->m_preg);
    int err;

    if (preg) {
	regfree(preg);
	delete preg;
	this->m_preg = NULL;
    }

    preg = new regex_t;
    if (preg == NULL) {
	throw std::runtime_error("out of memory");
    }

    err = regcomp(preg, pattern, REG_EXTENDED | REG_NEWLINE);
    if (err != 0) {
	delete preg;
	return err;
    }

    this->m_preg = preg;
    ASSERT(this->m_preg != NULL);

    return 0;
}

int
RegularExpression::match(const std::string& strval, unsigned count)
{
    int err;
    regex_t *	    preg = (regex_t *)(this->m_preg);
    regmatch_t *    matches = NULL;

    if (count) {
	matches = new regmatch_t[count];
	if (matches == NULL) {
	    throw std::runtime_error("out of memory");
	}
    }

    /* Flush the last match results. */
    this->m_matchlist.clear();

    err = regexec(preg, strval.c_str(), count, matches, 0 /* flags */);
    switch(err) {
	case 0: /* match */

	    for (unsigned i = 0; i < count; ++i) {
		std::string::size_type len;

		if (matches[i].rm_so == -1 || matches[i].rm_eo == -1) {
		    continue;
		}

		DEBUGMSG("matched rm_eo=%d rm_so=%d\n",
		    (int)matches[i].rm_eo, (int)matches[i].rm_so);

		len = matches[i].rm_eo - matches[i].rm_so;
		this->m_matchlist.push_back(
			std::string(strval.c_str() + matches[i].rm_so, len));
	    }

	    DEBUGMSG("made %zd matches\n", this->m_matchlist.size());

	    delete[] matches;
	    return err;

	case REG_NOMATCH: /* no match */
	    delete[] matches;
	    return RegularExpression::NOMATCH;

	default: /* error */
	    delete[] matches;
	    return err;
    }
}

const char * RegularExpression::errstring(int errcode)
{
    size_t errsz;

    if (this->m_errstr) {
	delete[] this->m_errstr;
    }

    ASSERT(this->m_preg != NULL);

    errsz = regerror(errcode, (const regex_t *)(this->m_preg), NULL, 0);
    if (errsz <= 0) {
	return "no error";
    }

    this->m_errstr = new char[errsz];
    if (this->m_errstr == NULL) {
	throw std::runtime_error("out of memory");
    }

    regerror(errcode, (const regex_t *)(this->m_preg), this->m_errstr, errsz);
    return this->m_errstr;
}

/* vim: set cindent ts=8 sts=4 tw=79 : */
