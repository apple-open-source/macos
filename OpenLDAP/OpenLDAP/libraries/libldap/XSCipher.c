//
//  XSCipher.c
//  CoreDaemon
//
//  Created by Mike Abbott on 4/24/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

/*
 * Slow algorithms are used throughout.  Could benefit from some lookup
 * tables or binary searches if performance proves to be a problem.
 */

#include <string.h>
#include <stdlib.h>

#include "XSCipher.h"

/* easier to include this than to make Xcode build it */
#include "XSCipherData.c"

static const struct xsCipher *xsCipherLookup(SSLCipherSuite cipher)
{
	for (size_t i = 0; i < xsCiphersCount; i++)
		if (xsCiphers[i].cipher == cipher)
			return &xsCiphers[i];

	/* also check demoted ciphers, for logging, because e.g. SSLGetSupportedCiphers() can return them */
	for (size_t i = 0; i < xsCiphersCount_Demoted; i++)
		if (xsCiphers_Demoted[i].cipher == cipher)
			return &xsCiphers_Demoted[i];

	return NULL;
}


const char *XSCipherToName(SSLCipherSuite cipher)
{
	const struct xsCipher *ci = xsCipherLookup(cipher);
	if (ci != NULL)
		return ci->name;
	return NULL;
}

SSLCipherSuite XSCipherFromName(const char *name)
{
	for (size_t i = 0; i < xsCiphersCount; i++)
		if (strcmp(xsCiphers[i].name, name) == 0)
			return xsCiphers[i].cipher;

	/* do not check demoted ciphers because they must not be used */

	return SSL_NO_SUCH_CIPHERSUITE;
}

struct xsCipherContext {
	const struct xsCipher *ciphers;
	size_t ciphersCount;

	const struct xsCipherSet *sets;
	size_t setsCount;

	SSLCipherSuite *ciphersToReturn;
	size_t ciphersToReturnCount;

	SSLCipherSuite *ciphersKilled;
	size_t ciphersKilledCount;
};

static void xsCipherContextReset(struct xsCipherContext *ctx)
{
	if (ctx->ciphersToReturn != NULL)
		free(ctx->ciphersToReturn);
	if (ctx->ciphersKilled != NULL)
		free(ctx->ciphersKilled);
	memset(ctx, 0, sizeof *ctx);
}

static int xsCipherArrayContains(SSLCipherSuite cipher, const SSLCipherSuite *ciphers, size_t ciphersCount)
{
	for (size_t i = 0; i < ciphersCount; i++)
		if (ciphers[i] == cipher)
			return 1;
	return 0;
}

static int xsCipherArrayAppend(SSLCipherSuite cipher, SSLCipherSuite **ciphers, size_t *ciphersCount, size_t ciphersMax)
{
	if (*ciphers == NULL) {
		*ciphers = (SSLCipherSuite *) malloc(ciphersMax * sizeof **ciphers);
		if (*ciphers == NULL)
			return 0;
	}
	if (*ciphersCount >= ciphersMax)	/* shouldn't happen but be safe */
		return 0;

	(*ciphers)[(*ciphersCount)++] = cipher;
	return 1;
}

static int xsCipherArrayRemove(SSLCipherSuite cipher, SSLCipherSuite *ciphers, size_t *ciphersCount)
{
	int removed = 0;

	size_t i = 0;
	while (i < *ciphersCount) {
		if (ciphers[i] == cipher) {
			--(*ciphersCount);
			memmove(&ciphers[i], &ciphers[i + 1], (*ciphersCount - i) * sizeof *ciphers);
			++removed;
		} else
			++i;
	}

	return removed;
}

static void xsCipherArrayIntersect(const SSLCipherSuite *from, size_t fromCount, SSLCipherSuite *into, size_t *intoCount)
{
	size_t i = 0;
	while (i < *intoCount) {
		if (!xsCipherArrayContains(into[i], from, fromCount)) {
			--(*intoCount);
			memmove(&into[i], &into[i + 1], (*intoCount - i) * sizeof *into);
		} else
			++i;
	}
}

static int xsCipherStrength(SSLCipherSuite cipher)
{
	/* xsCipherClass_ALL[] lists all the ciphers, sorted strongest->weakest */
	for (size_t i = 0; i < xsCipherClassCount_ALL; i++)
		if (xsCipherClass_ALL[i] == cipher)
			return (int) i;
	return -1;
}

static int xsCipherStrengthCompare(const void *a, const void *b)
{
	SSLCipherSuite cipher_a = *(const SSLCipherSuite *) a;
	SSLCipherSuite cipher_b = *(const SSLCipherSuite *) b;

	/* sort strongest first */
	return xsCipherStrength(cipher_a) - xsCipherStrength(cipher_b);
}

static void xsCipherArraySort(SSLCipherSuite *ciphers, size_t count, int (*cmp)(const void *, const void *))
{
	qsort(ciphers, count, sizeof *ciphers, cmp);
}

static int xsCipherHandle(SSLCipherSuite cipher, char action, struct xsCipherContext *ctx)
{
	if (action == '!') {
		/* kill the cipher */
		if (!xsCipherArrayContains(cipher, ctx->ciphersKilled, ctx->ciphersKilledCount)) {
			if (!xsCipherArrayAppend(cipher, &ctx->ciphersKilled, &ctx->ciphersKilledCount, ctx->ciphersCount))
				return 0;
		}

		/* and remove it from the array */
		xsCipherArrayRemove(cipher, ctx->ciphersToReturn, &ctx->ciphersToReturnCount);
	} else if (action == '+') {
		/* move the cipher to the end of the array if present and not killed */
		if (xsCipherArrayRemove(cipher, ctx->ciphersToReturn, &ctx->ciphersToReturnCount) &&
			!xsCipherArrayContains(cipher, ctx->ciphersKilled, ctx->ciphersKilledCount)) {
			if (!xsCipherArrayAppend(cipher, &ctx->ciphersToReturn, &ctx->ciphersToReturnCount, ctx->ciphersCount))
				return 0;
		}
	} else if (action == '-') {
		/* remove the cipher from the array */
		xsCipherArrayRemove(cipher, ctx->ciphersToReturn, &ctx->ciphersToReturnCount);
	} else {
		/* append the cipher to the array if not already present and not killed */
		if (!xsCipherArrayContains(cipher, ctx->ciphersToReturn, ctx->ciphersToReturnCount) &&
			!xsCipherArrayContains(cipher, ctx->ciphersKilled, ctx->ciphersKilledCount)) {
			if (!xsCipherArrayAppend(cipher, &ctx->ciphersToReturn, &ctx->ciphersToReturnCount, ctx->ciphersCount))
				return 0;
		}
	}

	return 1;
}

SSLCipherSuite *XSCipherSpecParse(const char *spec, size_t *count)
{
	if (count == NULL)
		return NULL;
	*count = 0;
	if (spec == NULL)
		return NULL;

	struct xsCipherContext ctx;
	memset(&ctx, 0, sizeof ctx);
	int ok = 1;

	/* choose between Security framework or OpenSSL identifiers */
	if (strncasecmp(spec, "OpenSSL:", 8) == 0) {
		spec += 8;
		ctx.ciphers = xsCiphersOpenSSL;
		ctx.ciphersCount = xsCiphersOpenSSLCount;
		ctx.sets = xsCipherOpenSSLSets;
		ctx.setsCount = xsCipherOpenSSLSetsCount;
	} else {
		ctx.ciphers = xsCiphers;
		ctx.ciphersCount = xsCiphersCount;
		ctx.sets = xsCipherSets;
		ctx.setsCount = xsCipherSetsCount;
	}

	/* parse the spec */
	char *specDup = strdup(spec);
	if (specDup != NULL) {
		char *str = specDup;
		char *token;
		while (ok && (token = strsep(&str, ":, ")) != NULL) {
			char action = 0;
			if (*token == '!' || *token == '+' || *token == '-')
				action = *token++;
			else if (strcasecmp(token, "@STRENGTH") == 0) {
				xsCipherArraySort(ctx.ciphersToReturn, ctx.ciphersToReturnCount, xsCipherStrengthCompare);
				continue;
			}

			/* handle intersections e.g. FOO+BAR */
			SSLCipherSuite *target = NULL;
			size_t targetCount = 0;

			char *tokenDup = strdup(token);
			if (tokenDup != NULL) {
				char *substr = tokenDup;
				char *subtoken;
				while (ok && (subtoken = strsep(&substr, "+")) != NULL) {
					/* is the subtoken a set or an individual cipher? */
					const struct xsCipherSet *set = ctx.sets;
					const struct xsCipherSet *setEnd = ctx.sets + ctx.setsCount;
					while (set < setEnd) {
						if (strcasecmp(subtoken, set->name) == 0)
							break;
						++set;
					}
					const struct xsCipher *cipher = ctx.ciphers;
					const struct xsCipher *cipherEnd = ctx.ciphers + ctx.ciphersCount;
					if (set >= setEnd) {
						while (cipher < cipherEnd) {
							if (strcasecmp(subtoken, cipher->name) == 0)
								break;
							++cipher;
						}
					}

					const SSLCipherSuite *subtarget;
					size_t subtargetCount;
					if (set < setEnd) {
						/* subtoken matches a set */
						subtarget = set->ciphers;
						subtargetCount = set->count;
					} else if (cipher < cipherEnd) {
						/* subtoken matches an individual cipher */
						subtarget = &cipher->cipher;
						subtargetCount = 1;
					} else {
						/* subtoken is unknown so ignore it */
						subtarget = NULL;
						subtargetCount = 0;
					}

					if (subtoken == tokenDup) {
						/* add ciphers from subtarget to target */
						for (size_t i = 0; ok && i < subtargetCount; i++) {
							if (!xsCipherArrayContains(subtarget[i], target, targetCount)) {
								if (!xsCipherArrayAppend(subtarget[i], &target, &targetCount, ctx.ciphersCount))
									ok = 0;
							}
						}
					} else {
						/* intersect subtarget into target */
						xsCipherArrayIntersect(subtarget, subtargetCount, target, &targetCount);
					}
				}

				/* apply action to the target array */
				for (size_t i = 0; ok && i < targetCount; i++)
					if (!xsCipherHandle(target[i], action, &ctx))
						ok = 0;

				if (target != NULL)
					free(target);
				free(tokenDup);
			} else
				ok = 0;
		}

		free(specDup);
	} else
		ok = 0;

	if (!ok)
		xsCipherContextReset(&ctx);

	if (ctx.ciphersKilled != NULL)
		free(ctx.ciphersKilled);
	if (ctx.ciphersToReturn != NULL && ctx.ciphersToReturnCount == 0) {
		free(ctx.ciphersToReturn);
		ctx.ciphersToReturn = NULL;
	}
	*count = ctx.ciphersToReturnCount;
	return ctx.ciphersToReturn;
}

CFDictionaryRef XSCipherCopyCipherProperties(SSLCipherSuite cipher)
{
	CFDictionaryRef properties = NULL;

	const struct xsCipher *ci = xsCipherLookup(cipher);
	if (ci != NULL) {
		CFStringRef name = CFStringCreateWithCString(NULL, ci->name, kCFStringEncodingUTF8);
		if (name != NULL) {
			CFNumberRef strbits = CFNumberCreate(NULL, kCFNumberIntType, &ci->strbits);
			if (strbits != NULL) {
				CFNumberRef algbits = CFNumberCreate(NULL, kCFNumberIntType, &ci->algbits);
				if (algbits != NULL) {
					CFTypeRef keys[] = {
						kXSCipherPropertyName,
						kXSCipherPropertyKeysig,
						kXSCipherPropertyBulk,
						kXSCipherPropertyMAC,
						kXSCipherPropertyStrength,
						kXSCipherPropertyStrengthBits,
						kXSCipherPropertyAlgBits,
						kXSCipherPropertyExport
					};
					CFTypeRef values[] = {
						name,
						*ci->keysig,
						*ci->bulk,
						*ci->mac,
						*ci->strength,
						strbits,
						algbits,
						ci->export ? kCFBooleanTrue : kCFBooleanFalse
					};
					properties = CFDictionaryCreate(NULL, keys, values, sizeof keys / sizeof keys[0], &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

					CFRelease(algbits);
				}
				CFRelease(strbits);
			}
			CFRelease(name);
		}
	}

	return properties;
}
