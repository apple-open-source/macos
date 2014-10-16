#include "extattr_os.h"

#ifdef EXTATTR_LINUX

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "flags.h"

static void *
memstr (void *buf, const char *str, const size_t buflen)
{
  void *p = buf;
  size_t len = buflen;
  const size_t slen = strlen(str);

  /* Ignore empty strings and buffers. */
  if ((slen == 0) || (buflen == 0))
    p = NULL;

  while (p && (len >= slen))
  {
    /*
     * Find the first character of the string, then see if the rest
     * matches.
     */
    p = memchr(p, str[0], len);
    if (!p)
      break;

    if (memcmp(p, str, slen) == 0)
      break;

    /* Next! */
    ++p;
    --len;
  }

  return p;
}

static char *
flags2namespace (struct hv *flags)
{
  const char *NAMESPACE_DEFAULT = NAMESPACE_USER;
  const size_t NAMESPACE_KEYLEN = strlen(NAMESPACE_KEY);
  SV **psv_ns;
  char *ns = NULL;

  if (flags && (psv_ns = hv_fetch(flags, NAMESPACE_KEY, NAMESPACE_KEYLEN, 0)))
  {
    char *s;
    STRLEN len;

    s = SvPV(*psv_ns, len);
    ns = malloc(len + 1);
    if (ns)
    {
      strncpy(ns, s, len);
      ns[len] = '\0';
    }
  }
  else
  {
    ns = strdup(NAMESPACE_DEFAULT);
  }

  return ns;
}

static char *
qualify_attrname (const char *attrname, struct hv *flags)
{
  char *res = NULL;
  char *pNS;
  size_t reslen;

  pNS = flags2namespace(flags);
  if (pNS)
  {
    reslen = strlen(pNS) + strlen(attrname) + 2; /* pNS + "." + attrname + nul */
    res = malloc(reslen);
  }

  if (res)
    snprintf(res, reslen, "%s.%s", pNS, attrname);

  if (pNS)
    free(pNS);

  return res;
}

int
linux_setxattr (const char *path,
                const char *attrname,
                const char *attrvalue,
                const size_t slen,
                struct hv *flags)
{
  int ret;
  char *q;
  File_ExtAttr_setflags_t setflags;
  int xflags = 0;

  setflags = File_ExtAttr_flags2setflags(flags);
  switch (setflags)
  {
  case SET_CREATEIFNEEDED: break;
  case SET_CREATE:         xflags |= XATTR_CREATE; break;
  case SET_REPLACE:        xflags |= XATTR_REPLACE; break;
  }

  q = qualify_attrname(attrname, flags);
  if (q)
  {
    ret = setxattr(path, q, attrvalue, slen, xflags);
    if (ret == -1)
      ret = -errno;
    free(q);
  }
  else
  {
    ret = -ENOMEM;
  }

  return ret;
}

int
linux_fsetxattr (const int fd,
                 const char *attrname,
                 const char *attrvalue,
                 const size_t slen,
                 struct hv *flags)
{
  int ret;
  char *q;
  File_ExtAttr_setflags_t setflags;
  int xflags = 0;

  setflags = File_ExtAttr_flags2setflags(flags);
  switch (setflags)
  {
  case SET_CREATEIFNEEDED: break;
  case SET_CREATE:         xflags |= XATTR_CREATE; break;
  case SET_REPLACE:        xflags |= XATTR_REPLACE; break;
  }

  q = qualify_attrname(attrname, flags);
  if (q)
  {
    ret = fsetxattr(fd, q, attrvalue, slen, xflags);
    if (ret == -1)
      ret = -errno;
    free(q);
  }
  else
  {
    ret = -ENOMEM;
  }

  return ret;
}

int
linux_getxattr (const char *path,
                const char *attrname,
                void *attrvalue,
                const size_t slen,
                struct hv *flags)
{
  int ret;
  char *q;

  q = qualify_attrname(attrname, flags);
  if (q)
  {
    ret = getxattr(path, q, attrvalue, slen);
    if (ret == -1)
      ret = -errno;
    free(q);
  }
  else
  {
    ret = -ENOMEM;
  }

  return ret;
}

int
linux_fgetxattr (const int fd,
                 const char *attrname,
                 void *attrvalue,
                 const size_t slen,
                 struct hv *flags)
{
  int ret;
  char *q;

  q = qualify_attrname(attrname, flags);
  if (q)
  {
    ret = fgetxattr(fd, q, attrvalue, slen);
    if (ret == -1)
      ret = -errno;
    free(q);
  }
  else
  {
    ret = -ENOMEM;
  }

  return ret;
}

int
linux_removexattr (const char *path,
                   const char *attrname,
                   struct hv *flags)
{
  int ret;
  char *q;

  /* XXX: Other flags? */
  q = qualify_attrname(attrname, flags);
  if (q)
  {
    ret = removexattr(path, q);
    if (ret == -1)
      ret = -errno;
    free(q);
  }
  else
  {
    ret = -ENOMEM;
  }

  return ret;
}

int
linux_fremovexattr (const int fd,
                    const char *attrname,
                    struct hv *flags)
{
  int ret;
  char *q;

  /* XXX: Other flags? */
  q = qualify_attrname(attrname, flags);
  if (q)
  {
    ret = fremovexattr(fd, q);
    if (ret == -1)
      ret = -errno;
    free(q);
  }
  else
  {
    ret = -ENOMEM;
  }

  return ret;
}

static ssize_t
attrlist2list (char *sbuf, const size_t slen,
               char *buf, const size_t buflen,
               const int iWantNames, const char *pWantNS)
{
  ssize_t sbuiltlen = 0;
  ssize_t spos = 0;
  int ret = -1;

  for (spos = 0; (spos < slen); )
  {
    const char *psrc;
    char *pNS, *pname;
    int src_len;

    /* Get the namespace. */
    pNS = &sbuf[spos];
    pname = strchr(pNS, '.');
    if (!pname)
      break;

    /* Point spos at the next attribute. */
    spos += strlen(pNS) + 1;

    *pname = '\0';
    ++pname;

    if (iWantNames)
    {
      psrc = pname;

      /* Name list wanted. Check this is in the right namespace. */
      if (strcmp(pNS, pWantNS) != 0)
        continue;
    }
    else
    {
      psrc = pNS;

      /*
       * Namespace list wanted. Check we haven't already seen
       * this namespace.
       */
      if (memstr(sbuf, pNS, sbuiltlen) != NULL)
        continue;
    }

    /*
     * We build the results in sbuf. So sbuf will contain the list
     * returned by listxattr and the list of namespaces.
     * We shift the namespaces from the list to the start of the buffer.
     */
    src_len = strlen(psrc) + 1;
    memmove(&sbuf[sbuiltlen], psrc, src_len);
    sbuiltlen += src_len;
  }

  if (buflen == 0)
  {
    /* Return what space is required. */
    ret = sbuiltlen;
  }
  else if (sbuiltlen <= buflen)
  {
    memcpy(buf, sbuf, sbuiltlen);
    ret = sbuiltlen;
  }
  else
  {
    ret = -ERANGE;
  }

  return ret;
}

/* XXX: More common code below */
/* XXX: Just return a Perl list? */

ssize_t
linux_listxattr (const char *path,
                 char *buf,
                 const size_t buflen,
                 struct hv *flags)
{
  char *pNS;
  ssize_t ret = 0;

  pNS = flags2namespace(flags);
  if (!pNS)
  {
    ret = -ENOMEM;
  }

  /*
   * Get a buffer of nul-delimited "namespace.attribute"s,
   * then extract the attributes into buf.
   */
  if (ret == 0)
  {
    ssize_t slen;

    slen = listxattr(path, buf, 0);
    if (slen == -1) {
      ret = -errno;
    } else if (slen >= 0) {
      char *sbuf;
   
      sbuf = malloc(slen);
      if (sbuf) {
        slen = listxattr(path, sbuf, slen);
        if (slen >= 0) {
          ret = attrlist2list(sbuf, slen, buf, buflen, 1, pNS);
        } else {
          ret = -errno;
        }
      } else {
        ret = -errno;
        slen = 0;
      }

      if (sbuf)
        free(sbuf);
    }
  }

  if (pNS)
    free(pNS);

  return ret;
}

ssize_t
linux_flistxattr (const int fd,
                  char *buf,
                  const size_t buflen,
                  struct hv *flags)
{
  char *pNS;
  ssize_t ret = 0;

  pNS = flags2namespace(flags);
  if (!pNS)
  {
    ret = -ENOMEM;
  }

  /*
   * Get a buffer of nul-delimited "namespace.attribute"s,
   * then extract the attributes into buf.
   */
  if (ret == 0)
  {
    ssize_t slen;

    slen = flistxattr(fd, buf, 0);
    if (slen == -1) {
      ret = -errno;
    } else if (slen >= 0) {
      char *sbuf;
   
      sbuf = malloc(slen);
      if (sbuf) {
        slen = flistxattr(fd, sbuf, slen);
        if (slen >= 0) {
          ret = attrlist2list(sbuf, slen, buf, buflen, 1, pNS);
        } else {
          ret = -errno;
        }
      } else {
        ret = -errno;
      }

      if (sbuf)
        free(sbuf);
    }
  }

  if (pNS)
    free(pNS);

  return ret;
}

ssize_t
linux_listxattrns (const char *path,
		   char *buf,
		   const size_t buflen,
		   struct hv *flags)
{
  ssize_t slen;
  ssize_t ret;

  /*
   * Get a buffer of nul-delimited "namespace.attribute"s,
   * then extract the namespaces into buf.
   */
  slen = listxattr(path, buf, 0);
  if (slen >= 0)
  {
    char *sbuf;
   
    sbuf = malloc(slen);
    if (sbuf) {
      slen = listxattr(path, sbuf, slen);
      if (slen >= 0) {
        ret = attrlist2list(sbuf, slen, buf, buflen, 0, NULL);
      } else {
        ret = -errno;
      }
    } else {
      ret = -errno;
    }

    if (sbuf)
      free(sbuf);
  }
  else
  {
    ret = -errno;
  }

  return ret;
}

ssize_t
linux_flistxattrns (const int fd,
		    char *buf,
		    const size_t buflen,
		    struct hv *flags)
{
  ssize_t slen;
  ssize_t ret;

  /*
   * Get a buffer of nul-delimited "namespace.attribute"s,
   * then extract the namespaces into buf.
   */
  slen = flistxattr(fd, buf, 0);
  if (slen >= 0)
  {
    char *sbuf;
   
    sbuf = malloc(slen);
    if (sbuf) {
      slen = flistxattr(fd, sbuf, slen);
      if (slen >= 0) {
        ret = attrlist2list(sbuf, slen, buf, buflen, 0, NULL);
      } else {
        ret = -errno;
      }
    } else {
      ret = -errno;
    }

    if (sbuf)
      free(sbuf);
  }
  else
  {
    ret = -errno;
  }

  return ret;
}

#endif /* EXTATTR_LINUX */
