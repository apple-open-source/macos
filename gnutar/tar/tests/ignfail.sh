#! /bin/sh
# Unreadable directories yielded error despite --ignore-failed-read.

. ./preset
. $srcdir/before

> check-uid
set - x`ls -l check-uid`
if test $3 = root; then

  # The test is meaningless for super-user.
  rm check-uid

else

   touch file
   mkdir directory
   touch directory/file

   echo 1>&2 -----
   chmod 000 file
   tar cf archive file
   status=$?
   chmod 600 file
   test $status = 2 || exit 1

   echo 1>&2 -----
   chmod 000 file
   tar cf archive --ignore-failed-read file || exit 1
   status=$?
   chmod 600 file
   test $status = 0 || exit 1

   echo 1>&2 -----
   chmod 000 directory
   tar cf archive directory
   status=$?
   chmod 700 directory
   test $status = 2 || exit 1

   echo 1>&2 -----
   chmod 000 directory
   tar cf archive --ignore-failed-read directory || exit 1
   status=$?
   chmod 700 directory
   test $status = 0 || exit 1

   err="\
-----
tar: Cannot add file file: Permission denied
tar: Error exit delayed from previous errors
-----
tar: Cannot add file file: Permission denied
-----
tar: Cannot add directory directory: Permission denied
tar: Error exit delayed from previous errors
-----
tar: Cannot add directory directory: Permission denied
"

fi

. $srcdir/after
