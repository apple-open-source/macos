#include <stddef.h>

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "flags.h"

/*
 * Convert the 'create' and/or 'replace' attributes into a value,
 * so they can be mapped to O_CREATE/O_EXCL values by the caller.
 */
File_ExtAttr_setflags_t
File_ExtAttr_flags2setflags (struct hv *flags)
{
  const size_t CREATE_KEYLEN = strlen(CREATE_KEY);
  const size_t REPLACE_KEYLEN = strlen(REPLACE_KEY);
  SV **psv_ns;
  File_ExtAttr_setflags_t ret = SET_CREATEIFNEEDED;

  /*
   * ASSUMPTION: Perl layer must ensure that create & replace
   * aren't used at the same time.
   */
  if (flags && (psv_ns = hv_fetch(flags, CREATE_KEY, CREATE_KEYLEN, 0)))
    ret = SvIV(*psv_ns) ? SET_CREATE : SET_CREATEIFNEEDED;

  if (flags && (psv_ns = hv_fetch(flags, REPLACE_KEY, REPLACE_KEYLEN, 0)))
    ret = SvIV(*psv_ns) ? SET_REPLACE : SET_CREATEIFNEEDED;

  return ret;
}

/*
 * For platforms that don't support namespacing attributes
 * (Mac OS X, Solaris), provide some smart default behaviour
 * for the 'namespace' attribute for cross-platform compatibility.
 */
int
File_ExtAttr_valid_default_namespace (struct hv *flags)
{
  const size_t NAMESPACE_KEYLEN = strlen(NAMESPACE_KEY);
  SV **psv_ns;
  int ok = 1; /* Default is valid */

  if (flags && (psv_ns = hv_fetch(flags, NAMESPACE_KEY, NAMESPACE_KEYLEN, 0)))
  {
    /*
     * Undefined => default. Otherwise treat "user" as if it were valid,
     * for compatibility with the default on Linux and *BSD.
     * An empty namespace (i.e.: zero-length) is not the same as the default.
     */
    if (SvOK(*psv_ns))
    {
      char *s;
      STRLEN len = 0;

      s = SvPV(*psv_ns, len);

      if (len)
	ok = (memcmp(NAMESPACE_USER, s, len) == 0);
      else
	ok = 0;
    }
  }

  return ok;
}

/*
 * Mac OS X and Solaris doesn't support namespacing attributes.
 * So if there are any attributes, call this function,
 * to return the namespace "user".
 */
ssize_t
File_ExtAttr_default_listxattrns (char *buf, const size_t buflen)
{
  ssize_t ret = 0;

  if (buflen >= sizeof(NAMESPACE_USER))
  {
    memcpy(buf, NAMESPACE_USER, sizeof(NAMESPACE_USER));
    ret = sizeof(NAMESPACE_USER);
  }
  else if (buflen == 0)
  {
    ret = sizeof(NAMESPACE_USER);
  }
  else
  {
    ret = -1;
    errno = ERANGE;
  }

  return ret;
}
