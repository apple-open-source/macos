#include "k5-int.h"
#include <CommonCrypto/CommonDigest.h>
#include "arcfour-int.h"

#if TARGET_OS_MAC && !defined(DEPEND)
#include <CoreFoundation/CFString.h>
#endif

int krb5_use_broken_arcfour_string2key = 0;

static krb5_error_code 
utf8to16(unsigned char *utf16_buf, const char *utf8_str, size_t slen, size_t *len)
{
    krb5_error_code err = 0;
    
    if (krb5_use_broken_arcfour_string2key == 0 && getenv("KRB5_USE_BROKEN_ARCFOUR_STRING2KEY") == 0) {
	CFStringRef string = NULL;
	CFIndex length = *len;
	
	string = CFStringCreateWithCString (kCFAllocatorDefault, 
					    utf8_str, kCFStringEncodingUTF8);
	if (!string) { err = ENOMEM; }
	
	if (!err) {
	    CFIndex copied = 0;
	    CFRange range = CFRangeMake (0, CFStringGetLength (string));
	    
	    copied = CFStringGetBytes (string, range, kCFStringEncodingUTF16LE, 
				       0, false, utf16_buf, length, &length);
	    if (copied != range.length) { err = ENOMEM; }
	}
	
	if (!err) {
	    *len = length;
	}
	
	if (string) { CFRelease (string); }
	
    } else  {
	/* 
	 * This should be re-evaluated in the future, it makes the assumption that
	 * the user's password is in ascii, not utf-8.  Use iconv?
	 */     
	size_t counter;
	for (counter=0;counter< slen;counter++) {
	    utf16_buf[2*counter]=utf8_str[counter];
	    utf16_buf[2*counter + 1]=0x00;
	}
    }
    
    return err;
}

krb5_error_code
krb5int_arcfour_string_to_key(const struct krb5_enc_provider *enc,
			      const krb5_data *string, const krb5_data *salt,
			      const krb5_data *params, krb5_keyblock *key)
{
  krb5_error_code err = 0;
  size_t len;
  unsigned char *copystr;
  CC_MD4_CTX md4_context;

  if (params != NULL)
      return KRB5_ERR_BAD_S2K_PARAMS;
  
  if (key->length != 16)
    return (KRB5_BAD_MSIZE);

  /* We ignore salt per the Microsoft spec*/

  /* compute the space needed for the new string.
     Since the password must be stored in unicode, we need to increase
     that number by 2x.
  */
  if (string->length > (SIZE_MAX/2))
      return (KRB5_BAD_MSIZE);

  len= string->length * 2;

  copystr = malloc(len);
  if (copystr == NULL)
    return ENOMEM;

  /* make the string.  start by creating the unicode version of the password*/
  err = utf8to16(copystr, string->data, string->length, &len);
  if (err) goto cleanup;

  /* the actual MD4 hash of the data */
  CC_MD4_Init(&md4_context);
  CC_MD4_Update(&md4_context, (unsigned char *)copystr, len);
  CC_MD4_Final(key->contents, &md4_context);

#if 0  
  /* test the string_to_key function */
  printf("Hash=");
  {
    int counter;
    for(counter=0;counter<16;counter++)
      printf("%02x", md4_context.digest[counter]);
    printf("\n");
  }
#endif /* 0 */

cleanup:
  /* Zero out the data behind us */
  memset (copystr, 0, len);
  memset(&md4_context, 0, sizeof(md4_context));
  free(copystr);
  return err;
}
