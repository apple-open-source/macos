/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "portable.h"

#include <stdio.h>

#include <ac/socket.h>
#include <ac/string.h>

#include "slap.h"
#include "back-netinfo.h"

static dsfilter *dsfilter_append_berval LDAP_P((dsfilter *dsf, int32_t dsaType, dsdata *key, struct berval *data));
static dsfilter *substrings_assertion_to_dsfilter LDAP_P((BackendDB *be, SubstringsAssertion *sa));
static dsassertion *attribute_description_to_dsassertion LDAP_P((BackendDB *be, AttributeDescription *desc));
static dsfilter *attribute_description_to_dsfilter LDAP_P((BackendDB *be, AttributeDescription *desc));
static dsfilter *attribute_assertion_to_dsfilter LDAP_P((BackendDB *be, AttributeAssertion *ava, ber_tag_t choice));
static dsfilter *computed_filter_to_dsfilter LDAP_P((BackendDB *be, ber_int_t result));
static dsfilter *filter_list_to_dsfilter LDAP_P((BackendDB *be, Filter *filter, ber_tag_t choice));

/*
 * Translate a slapd attribute assertion into a dsassertion.
 */
dsassertion *attribute_assertion_to_dsassertion(
	BackendDB *be,
	AttributeAssertion *ava,
	ber_tag_t choice)
{
	int32_t dsaType;
	dsassertion *dsa;
	dsdata *wrappedKey, *wrappedValue;

	/* DSA_EQUAL */
	switch (choice)
	{
		case LDAP_FILTER_EQUALITY:
			dsaType = DSA_EQUAL;
			break;
		case LDAP_FILTER_GE:
			dsaType = DSA_GREATER_OR_EQUAL;
			break;
		case LDAP_FILTER_LE:
			dsaType = DSA_LESS_OR_EQUAL;
			break;
		case LDAP_FILTER_APPROX:
			dsaType = DSA_APPROX;
			break;
		default:
			return NULL;
	}

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "attribute_assertion_to_dsassertion: "
		"choice %ld assertion type %d\n", choice, dsaType));
#else
	Debug(LDAP_DEBUG_TRACE, "==> attribute_assertion_to_dsassertion choice=%ld dsaType=%d\n",
		choice, dsaType, 0);
#endif

	wrappedKey = dsdata_alloc(0);
	if (wrappedKey == NULL)
		return NULL;

	wrappedKey->type = DataTypeCPtr;
	wrappedKey->retain = 1;
	wrappedKey->data = (void *)ava->aa_desc;

	wrappedValue = dsdata_alloc(0);
	if (wrappedValue == NULL)
	{
		dsdata_release(wrappedKey);
		return NULL;
	}
	wrappedValue->type = DataTypeCPtr;
	wrappedValue->retain = 1;
	wrappedValue->data = (void *)&ava->aa_value;

	dsa = dsassertion_new(dsaType, SELECT_ATTRIBUTE, wrappedKey, wrappedValue);

	dsdata_release(wrappedKey);
	dsdata_release(wrappedValue);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "attribute_assertion_to_dsassertion: done\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "<== attribute_assertion_to_dsassertion\n", 0, 0, 0);
#endif

	return dsa;
}

/*
 * Wrap an attribute assertion dsassertion into a dsfilter.
 */
static dsfilter *attribute_assertion_to_dsfilter(
	BackendDB *be,
	AttributeAssertion *ava,
	ber_tag_t choice)
{
	dsassertion *a;
	dsfilter *f;

	a = attribute_assertion_to_dsassertion(be, ava, choice);
	if (a == NULL)
	{
		return NULL;
	}

	f = dsfilter_new_assert(a);

	dsassertion_release(a);

	return f;
}

/*
 * Add an attribute value assertion to a filter. Used for substring
 * searches.
 */
static dsfilter *dsfilter_append_berval(
	dsfilter *dsf,
	int32_t dsaType,
	dsdata *wrappedKey,
	struct berval *data)
{
	dsfilter *localDsf;
	dsdata *wrappedValue;
	dsassertion *dsa;

	wrappedValue = dsdata_alloc(0);
	if (wrappedValue == NULL)
	{
		dsfilter_release(dsf);
		return NULL;
	}

	wrappedValue->type = DataTypeCPtr;
	wrappedValue->retain = 1;
	wrappedValue->data = (void *)data;

	dsa = dsassertion_new(dsaType, SELECT_ATTRIBUTE, wrappedKey, wrappedValue);
	if (dsa == NULL)
	{
		dsdata_release(wrappedValue);
		dsfilter_release(dsf);
		return NULL;
	}

	dsdata_release(wrappedValue);

	localDsf = dsfilter_new_assert(dsa);
	if (localDsf == NULL)
	{
		dsassertion_release(dsa);
		dsfilter_release(dsf);
		return NULL;
	}

	dsf = dsfilter_append_filter(dsf, localDsf);

	dsassertion_release(dsa);
	dsfilter_release(localDsf);

	return dsf;
}

static dsfilter *substrings_assertion_to_dsfilter(
	BackendDB *be,
	SubstringsAssertion *sa)
{
	dsfilter *dsf;
	BerVarray bvp;
	dsdata *wrappedKey;

	/* DSA_SUBSTR DSA_PREFIX DSA_SUFFIX */

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ENTRY, "substrings_assertion_to_dsfilter: enter\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "==> substrings_assertion_to_dsfilter\n", 0, 0, 0);
#endif

	wrappedKey = dsdata_alloc(0);
	if (wrappedKey == NULL)
		return NULL;

	wrappedKey->type = DataTypeCPtr;
	wrappedKey->retain = 1;
	wrappedKey->data = (void *)sa->sa_desc;

	dsf = dsfilter_new_composite(DSF_OP_AND);
	if (dsf == NULL)
	{
		dsdata_release(wrappedKey);
		return NULL;
	}

	if (sa->sa_initial.bv_val != NULL)
	{
		dsf = dsfilter_append_berval(dsf, DSA_PREFIX, wrappedKey, &sa->sa_initial);
		if (dsf == NULL)
		{
			dsdata_release(wrappedKey);
			return NULL;
		}
	}

	if (sa->sa_any != NULL)
	{
		for (bvp = sa->sa_any; bvp->bv_val != NULL; bvp++)
		{
			dsf = dsfilter_append_berval(dsf, DSA_SUBSTR, wrappedKey, bvp);
			if (dsf == NULL)
			{
				dsdata_release(wrappedKey);
				return NULL;
			}
		}
	}

	if (sa->sa_final.bv_val != NULL)
	{
		dsf = dsfilter_append_berval(dsf, DSA_SUFFIX, wrappedKey, &sa->sa_final);
		if (dsf == NULL)
		{
			dsdata_release(wrappedKey);
			return NULL;
		}
	}

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "substrings_assertion_to_dsfilter: done\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "<== substrings_assertion_to_dsfilter\n", 0, 0, 0);
#endif

	dsdata_release(wrappedKey);

	return dsf;
}

static dsassertion *attribute_description_to_dsassertion(
	BackendDB *be,
	AttributeDescription *desc)
{
	dsassertion *dsa;
	dsdata *wrappedKey;

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "attribute_description_to_dsassertion: "
		"attribute %s\n", desc->ad_cname.bv_val));
#else
	Debug(LDAP_DEBUG_TRACE, "==> attribute_description_to_dsassertion ad_cname=%s\n",
		desc->ad_cname.bv_val, 0, 0);
#endif

	/* DSA_HAS_KEY */

	/*
	 * All entries have objectClass, structuralObjectClass, and 
	 * distinguishedName.
	 */
	if ((ad_cmp(desc, slap_schema.si_ad_objectClass) == 0) ||
	    (ad_cmp(desc, slap_schema.si_ad_structuralObjectClass) == 0))
	{
		wrappedKey = uint32_to_dsdata(L3True);
		dsa = dsassertion_new(DSA_PRECOMPUTED, 0, wrappedKey, NULL);
	}
	else
	{
		wrappedKey = dsdata_alloc(0);
		if (wrappedKey == NULL)
			return NULL;
		wrappedKey->type = DataTypeCPtr;
		wrappedKey->retain = 1;
		wrappedKey->data = (void *)desc;
		dsa = dsassertion_new(DSA_HAS_KEY, SELECT_ATTRIBUTE, wrappedKey, NULL);
	}

	dsdata_release(wrappedKey);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "attribute_description_to_dsassertion: "
		"assertion type %d\n", dsa->assertion));
#else
	Debug(LDAP_DEBUG_TRACE, "<== attribute_description_to_dsassertion dsaType=%d\n", dsa->assertion, 0, 0);
#endif

	return dsa;
}

static dsfilter *attribute_description_to_dsfilter(
	BackendDB *be,
	AttributeDescription *desc)
{
	dsassertion *a;
	dsfilter *f;

	a = attribute_description_to_dsassertion(be, desc);
	if (a == NULL)
	{
		return NULL;
	}

	f = dsfilter_new_assert(a);
	dsassertion_release(a);

	return f;
}

static dsfilter *computed_filter_to_dsfilter(
	BackendDB *be,
	ber_int_t result)
{
	dsdata *key;
	Logic3 r;
	dsassertion *a;
	dsfilter *f;

	switch (result)
	{
		case LDAP_COMPARE_FALSE:
			r = L3False;
			break;
		case LDAP_COMPARE_TRUE:
			r = L3True;
			break;
		case SLAPD_COMPARE_UNDEFINED:
		default:
			r = L3Undefined;
			break;
	}
	
	key = uint32_to_dsdata(r);
	a = dsassertion_new(DSA_PRECOMPUTED, 0, key, NULL);
	if (a == NULL)
	{
		dsdata_release(key);
		return NULL;
	}

	dsdata_release(key);

	f = dsfilter_new_assert(a);
	dsassertion_release(a);

	return f;
}

static dsfilter *filter_list_to_dsfilter(
	BackendDB *be,
	Filter *filter,
	ber_tag_t choice)
{
	dsfilter *dsf;
	u_int32_t op;
	Filter *fp;

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "filter_list_to_dsfilter: choice %ld\n", choice));
#else
	Debug(LDAP_DEBUG_FILTER, "==> filter_list_to_dsfilter choice=%ld\n", choice, 0, 0);
#endif

	switch (choice)
	{
		case LDAP_FILTER_AND:
			op = DSF_OP_AND;
			break;
		case LDAP_FILTER_OR:
			op = DSF_OP_OR;
			break;
		case LDAP_FILTER_NOT:
			op = DSF_OP_NOT;
			break;
		default:
			return NULL;
	}

	dsf = dsfilter_new_composite(op);
	if (dsf == NULL)
	{
		return NULL;
	}

	for (fp = filter; fp != NULL; fp = fp->f_next)
	{
		dsfilter *localDsf;

		localDsf = filter_to_dsfilter(be, fp);
		dsf = dsfilter_append_filter(dsf, localDsf);
		dsfilter_release(localDsf);
	}

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "filter_list_to_dsfilter: done\n"));
#else
	Debug(LDAP_DEBUG_FILTER, "<== filter_list_to_dsfilter\n", 0, 0, 0);
#endif

	return dsf;
}

dsfilter *filter_to_dsfilter(
	BackendDB *be,
	Filter *filter)
{
	dsfilter *f = NULL;

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "filter_to_dsfilter: choice %ld\n",
		filter->f_choice));
#else
	Debug(LDAP_DEBUG_FILTER, "==> filter_to_dsfilter choice=%ld\n", filter->f_choice, 0, 0);
#endif

	switch (filter->f_choice)
	{
		case LDAP_FILTER_SUBSTRINGS:
			f = substrings_assertion_to_dsfilter(be, filter->f_sub);
			break;
		case LDAP_FILTER_EQUALITY:
		case LDAP_FILTER_GE:
		case LDAP_FILTER_LE:
		case LDAP_FILTER_APPROX:
			f = attribute_assertion_to_dsfilter(be, filter->f_ava, filter->f_choice);
			break;
		case LDAP_FILTER_PRESENT:
			f = attribute_description_to_dsfilter(be, filter->f_desc);
			break;
		case LDAP_FILTER_AND:
		case LDAP_FILTER_OR:
		case LDAP_FILTER_NOT:
			f = filter_list_to_dsfilter(be, filter->f_list, filter->f_choice);
			break;
		case SLAPD_FILTER_COMPUTED:
			f = computed_filter_to_dsfilter(be, filter->f_result);
			break;
		case LDAP_FILTER_EXT:
		default:
			/* Don't support extensible matching rules. */
			f = NULL;
			break;
	}

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "filter_to_dsfilter: filter %p", f));
#else
	Debug(LDAP_DEBUG_FILTER, "<== filter_to_dsfilter f=%p\n", f, 0, 0);
#endif

	return f;
}

/*
 * Evaluate a dsassertion with respect to attribute mapping.
 */
Logic3 wrapped_assertion_test(dsassertion *t, dsrecord *r, void *private)
{
	BackendDB *be = (BackendDB *)private;
	dsassertion *t2; /* unmapped assertion */
	Logic3 eval;
	dsdata *value;
	AttributeDescription *ad;
	struct berval *bv;
	struct atmap map;
	u_int32_t super;
	
	/* Not a "special" wrapped assertion. */
	if (t->key->type != DataTypeCPtr)
		return dsassertion_test(t, r);

	/* Extract AttributeDescription and berval */
	ad = (AttributeDescription *)t->key->data;
	if (t->value != NULL)
	{
		assert(t->value->type == DataTypeCPtr);
		bv = (struct berval *)t->value->data;
		assert(bv->bv_val != NULL);
	}
	else
	{
		bv = NULL;
	}

	super = SUPER(r);

	/* Unwrap/map key. */
	if (schemamap_x500_to_ni_at(be, super, ad, &map) != DSStatusOK)
	{
		return L3Undefined;
	}

	eval = L3Undefined;

	/* Test mapped or dynamic attributes. */
	if (t->assertion == DSA_EQUAL && bv != NULL)
	{
		if ((ad_cmp(ad, slap_schema.si_ad_objectClass) == 0) &&
		    (dsrecord_attribute_index(r, map.ni_key, map.selector) == IndexNull) &&
		    (schemamap_check_oc(be, super, bv) != 0))
		{
			eval = L3True;
		}
		else if ((ad_cmp(ad, slap_schema.si_ad_structuralObjectClass) == 0) &&
		    (dsrecord_attribute_index(r, map.ni_key, map.selector) == IndexNull) &&
		    (schemamap_check_structural_oc(be, super, bv)) != 0)
		{
			eval = L3True;
		}
		else if (ad_cmp(ad, netinfo_back_ad_dSID) == 0)
		{
			eval = (atol(bv->bv_val) == r->dsid) ? L3True : L3False;
		}
		else if (ad_cmp(ad, netinfo_back_ad_nIVersionNumber) == 0)
		{
			eval = (atol(bv->bv_val) == r->vers) ? L3True : L3False;
		}
		else if (ad_cmp(ad, netinfo_back_ad_nISerialNumber) == 0)
		{
			eval = (atol(bv->bv_val) == r->serial) ? L3True : L3False;
		}
	}

	if (eval == L3Undefined)
	{
		if (bv != NULL)
		{
			if ((map.x500ToNiTransform)(be, &value, bv, map.type, map.x500ToNiArg) != DSStatusOK)
			{
				schemamap_atmap_release(&map);
				return L3Undefined;
			}
		}
		else
		{
			value = NULL;
		}
	
		t2 = dsassertion_new(t->assertion, map.selector, map.ni_key, value);
		eval = dsassertion_test(t2, r);
		dsassertion_release(t2);
	
		if (value != NULL &&
		    value->type == DataTypeDirectoryID &&
		    eval != L3True)
		{
			/* check also for non-DSID type. */
			dsdata *value2;
	
			value2 = berval_to_dsdata(bv, DataTypeCaseUTF8Str);
			t2 = dsassertion_new(t->assertion, map.selector, map.ni_key, value2);
			eval = dsassertion_test(t2, r);
			dsassertion_release(t2);
			dsdata_release(value2);
		}
	
		if (value != NULL)
			dsdata_release(value);
	}
	
	schemamap_atmap_release(&map);

	return eval;
}

Logic3 wrapped_filter_test(dsfilter *f, dsrecord *r, void *private)
{
	u_int32_t i, hasUndef;
	Logic3 x;

	if (f == NULL) return L3Undefined;
	if (r == NULL) return L3Undefined;

	if (f->op == DSF_OP_ASSERT)
		return wrapped_assertion_test(f->assert, r, private);

	switch (f->op)
	{
		case DSF_OP_AND:
			hasUndef = 0;
			for (i = 0; i < f->count; i++)
			{
				x = wrapped_filter_test(f->filter[i], r, private);
				if (x == L3False) return L3False;
				if (x == L3Undefined) hasUndef = 1;
			}
			if (hasUndef == 0) return L3True;
			return L3Undefined;

		case DSF_OP_OR:
			hasUndef = 0;
			for (i = 0; i < f->count; i++)
			{
				x = wrapped_filter_test(f->filter[i], r, private);
				if (x == L3True) return L3True;
				if (x == L3Undefined) hasUndef = 1;
			}
			if (hasUndef == 0) return L3False;
			return L3Undefined;
			
		case DSF_OP_NOT:
			if (f->count == 0) return L3Undefined;
			x = wrapped_filter_test(f->filter[0], r, private);
			if (x == L3True) return L3False;
			if (x == L3False) return L3True;
			return L3Undefined;
	}

	return L3Undefined;
}
