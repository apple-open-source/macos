#define PERL_constant_NOTFOUND	1
#define PERL_constant_NOTDEF	2
#define PERL_constant_ISIV	3
#define PERL_constant_ISNO	4
#define PERL_constant_ISNV	5
#define PERL_constant_ISPV	6
#define PERL_constant_ISPVN	7
#define PERL_constant_ISSV	8
#define PERL_constant_ISUNDEF	9
#define PERL_constant_ISUV	10
#define PERL_constant_ISYES	11

#ifndef NVTYPE
typedef double NV; /* 5.6 and later define NVTYPE, and typedef NV to it.  */
#endif
#ifndef aTHX_
#define aTHX_ /* 5.6 or later define this for threading support.  */
#endif
#ifndef pTHX_
#define pTHX_ /* 5.6 or later define this for threading support.  */
#endif

static int
constant_9 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     EVFILT_FS EV_DELETE EV_ENABLE NOTE_EXEC NOTE_EXIT NOTE_FORK NOTE_LINK */
  /* Offset 7 gives the best switch position.  */
  switch (name[7]) {
  case 'E':
    if (memEQ(name, "NOTE_EXEC", 9)) {
    /*                      ^       */
#ifdef NOTE_EXEC
      *iv_return = NOTE_EXEC;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'F':
    if (memEQ(name, "EVFILT_FS", 9)) {
    /*                      ^       */
#ifdef EVFILT_FS
      *iv_return = EVFILT_FS;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'I':
    if (memEQ(name, "NOTE_EXIT", 9)) {
    /*                      ^       */
#ifdef NOTE_EXIT
      *iv_return = NOTE_EXIT;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'L':
    if (memEQ(name, "EV_ENABLE", 9)) {
    /*                      ^       */
#ifdef EV_ENABLE
      *iv_return = EV_ENABLE;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'N':
    if (memEQ(name, "NOTE_LINK", 9)) {
    /*                      ^       */
#ifdef NOTE_LINK
      *iv_return = NOTE_LINK;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'R':
    if (memEQ(name, "NOTE_FORK", 9)) {
    /*                      ^       */
#ifdef NOTE_FORK
      *iv_return = NOTE_FORK;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'T':
    if (memEQ(name, "EV_DELETE", 9)) {
    /*                      ^       */
#ifdef EV_DELETE
      *iv_return = EV_DELETE;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_10 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     EV_DISABLE EV_ONESHOT NOTE_CHILD NOTE_LOWAT NOTE_TRACK NOTE_WRITE */
  /* Offset 5 gives the best switch position.  */
  switch (name[5]) {
  case 'C':
    if (memEQ(name, "NOTE_CHILD", 10)) {
    /*                    ^           */
#ifdef NOTE_CHILD
      *iv_return = NOTE_CHILD;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'E':
    if (memEQ(name, "EV_ONESHOT", 10)) {
    /*                    ^           */
#ifdef EV_ONESHOT
      *iv_return = EV_ONESHOT;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'L':
    if (memEQ(name, "NOTE_LOWAT", 10)) {
    /*                    ^           */
#ifdef NOTE_LOWAT
      *iv_return = NOTE_LOWAT;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'S':
    if (memEQ(name, "EV_DISABLE", 10)) {
    /*                    ^           */
#ifdef EV_DISABLE
      *iv_return = EV_DISABLE;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'T':
    if (memEQ(name, "NOTE_TRACK", 10)) {
    /*                    ^           */
#ifdef NOTE_TRACK
      *iv_return = NOTE_TRACK;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'W':
    if (memEQ(name, "NOTE_WRITE", 10)) {
    /*                    ^           */
#ifdef NOTE_WRITE
      *iv_return = NOTE_WRITE;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_11 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     EVFILT_PROC EVFILT_READ NOTE_ATTRIB NOTE_DELETE NOTE_EXTEND NOTE_RENAME
     NOTE_REVOKE */
  /* Offset 9 gives the best switch position.  */
  switch (name[9]) {
  case 'A':
    if (memEQ(name, "EVFILT_READ", 11)) {
    /*                        ^        */
#ifdef EVFILT_READ
      *iv_return = EVFILT_READ;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'I':
    if (memEQ(name, "NOTE_ATTRIB", 11)) {
    /*                        ^        */
#ifdef NOTE_ATTRIB
      *iv_return = NOTE_ATTRIB;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'K':
    if (memEQ(name, "NOTE_REVOKE", 11)) {
    /*                        ^        */
#ifdef NOTE_REVOKE
      *iv_return = NOTE_REVOKE;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'M':
    if (memEQ(name, "NOTE_RENAME", 11)) {
    /*                        ^        */
#ifdef NOTE_RENAME
      *iv_return = NOTE_RENAME;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'N':
    if (memEQ(name, "NOTE_EXTEND", 11)) {
    /*                        ^        */
#ifdef NOTE_EXTEND
      *iv_return = NOTE_EXTEND;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'O':
    if (memEQ(name, "EVFILT_PROC", 11)) {
    /*                        ^        */
#ifdef EVFILT_PROC
      *iv_return = EVFILT_PROC;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'T':
    if (memEQ(name, "NOTE_DELETE", 11)) {
    /*                        ^        */
#ifdef NOTE_DELETE
      *iv_return = NOTE_DELETE;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_12 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     EVFILT_TIMER EVFILT_VNODE EVFILT_WRITE */
  /* Offset 7 gives the best switch position.  */
  switch (name[7]) {
  case 'T':
    if (memEQ(name, "EVFILT_TIMER", 12)) {
    /*                      ^           */
#ifdef EVFILT_TIMER
      *iv_return = EVFILT_TIMER;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'V':
    if (memEQ(name, "EVFILT_VNODE", 12)) {
    /*                      ^           */
#ifdef EVFILT_VNODE
      *iv_return = EVFILT_VNODE;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'W':
    if (memEQ(name, "EVFILT_WRITE", 12)) {
    /*                      ^           */
#ifdef EVFILT_WRITE
      *iv_return = EVFILT_WRITE;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant (pTHX_ const char *name, STRLEN len, IV *iv_return) {
  /* Initially switch on the length of the name.  */
  /* When generated this function returned values for the list of names given
     in this section of perl code.  Rather than manually editing these functions
     to add or remove constants, which would result in this comment and section
     of code becoming inaccurate, we recommend that you edit this section of
     code, and use it to regenerate a new set of constant functions which you
     then use to replace the originals.

     Regenerate these constant functions by feeding this entire source file to
     perl -x

#!perl -w
use ExtUtils::Constant qw (constant_types C_constant XS_constant);

my $types = {map {($_, 1)} qw(IV)};
my @names = (qw(EVFILT_FS EVFILT_PROC EVFILT_READ EVFILT_SIGNAL EVFILT_TIMER
	       EVFILT_VNODE EVFILT_WRITE EV_ADD EV_CLEAR EV_DELETE EV_DISABLE
	       EV_ENABLE EV_EOF EV_ERROR EV_ONESHOT NOTE_ATTRIB NOTE_CHILD
	       NOTE_DELETE NOTE_EXEC NOTE_EXIT NOTE_EXTEND NOTE_FORK NOTE_LINK
	       NOTE_LOWAT NOTE_PCTRLMASK NOTE_PDATAMASK NOTE_RENAME NOTE_REVOKE
	       NOTE_TRACK NOTE_TRACKERR NOTE_WRITE));

print constant_types(), "\n"; # macro defs
foreach (C_constant ("IO::KQueue", 'constant', 'IV', $types, undef, 3, @names) ) {
    print $_, "\n"; # C constant subs
}
print "\n#### XS Section:\n";
print XS_constant ("IO::KQueue", $types);
__END__
   */

  switch (len) {
  case 6:
    /* Names all of length 6.  */
    /* EV_ADD EV_EOF */
    /* Offset 5 gives the best switch position.  */
    switch (name[5]) {
    case 'D':
      if (memEQ(name, "EV_AD", 5)) {
      /*                    D     */
#ifdef EV_ADD
        *iv_return = EV_ADD;
        return PERL_constant_ISIV;
#else
        return PERL_constant_NOTDEF;
#endif
      }
      break;
    case 'F':
      if (memEQ(name, "EV_EO", 5)) {
      /*                    F     */
#ifdef EV_EOF
        *iv_return = EV_EOF;
        return PERL_constant_ISIV;
#else
        return PERL_constant_NOTDEF;
#endif
      }
      break;
    }
    break;
  case 8:
    /* Names all of length 8.  */
    /* EV_CLEAR EV_ERROR */
    /* Offset 3 gives the best switch position.  */
    switch (name[3]) {
    case 'C':
      if (memEQ(name, "EV_CLEAR", 8)) {
      /*                  ^          */
#ifdef EV_CLEAR
        *iv_return = EV_CLEAR;
        return PERL_constant_ISIV;
#else
        return PERL_constant_NOTDEF;
#endif
      }
      break;
    case 'E':
      if (memEQ(name, "EV_ERROR", 8)) {
      /*                  ^          */
#ifdef EV_ERROR
        *iv_return = EV_ERROR;
        return PERL_constant_ISIV;
#else
        return PERL_constant_NOTDEF;
#endif
      }
      break;
    }
    break;
  case 9:
    return constant_9 (aTHX_ name, iv_return);
    break;
  case 10:
    return constant_10 (aTHX_ name, iv_return);
    break;
  case 11:
    return constant_11 (aTHX_ name, iv_return);
    break;
  case 12:
    return constant_12 (aTHX_ name, iv_return);
    break;
  case 13:
    /* Names all of length 13.  */
    /* EVFILT_SIGNAL NOTE_TRACKERR */
    /* Offset 3 gives the best switch position.  */
    switch (name[3]) {
    case 'E':
      if (memEQ(name, "NOTE_TRACKERR", 13)) {
      /*                  ^                */
#ifdef NOTE_TRACKERR
        *iv_return = NOTE_TRACKERR;
        return PERL_constant_ISIV;
#else
        return PERL_constant_NOTDEF;
#endif
      }
      break;
    case 'I':
      if (memEQ(name, "EVFILT_SIGNAL", 13)) {
      /*                  ^                */
#ifdef EVFILT_SIGNAL
        *iv_return = EVFILT_SIGNAL;
        return PERL_constant_ISIV;
#else
        return PERL_constant_NOTDEF;
#endif
      }
      break;
    }
    break;
  case 14:
    /* Names all of length 14.  */
    /* NOTE_PCTRLMASK NOTE_PDATAMASK */
    /* Offset 6 gives the best switch position.  */
    switch (name[6]) {
    case 'C':
      if (memEQ(name, "NOTE_PCTRLMASK", 14)) {
      /*                     ^              */
#ifdef NOTE_PCTRLMASK
        *iv_return = NOTE_PCTRLMASK;
        return PERL_constant_ISIV;
#else
        return PERL_constant_NOTDEF;
#endif
      }
      break;
    case 'D':
      if (memEQ(name, "NOTE_PDATAMASK", 14)) {
      /*                     ^              */
#ifdef NOTE_PDATAMASK
        *iv_return = NOTE_PDATAMASK;
        return PERL_constant_ISIV;
#else
        return PERL_constant_NOTDEF;
#endif
      }
      break;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

