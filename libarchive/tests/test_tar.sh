#!/bin/sh

# Called only once per script.
oneTimeSetUp()
{
  SCRIPTDIR=`dirname "$0"`
  STATUS=0
  SCRATCHDIR=`mktemp -d -t test_tar`
}

# Called only once per script.
oneTimeTearDown()
{
  chmod -R 0777 "${SCRATCHDIR}"
  rm -fr "${SCRATCHDIR}"
  rm -f /tmp/myfile
}

# Called before each test.
setUp()
{
  rm -f /tmp/myfile
}

test_libarchive_r26496124()
{
  /usr/bin/tar -C "${SCRATCHDIR}" -xvf "${SCRIPTDIR}"/radar-26496124.tar
  assertFalse 'Radar 26496124' "${?}"
}

test_libarchive_r26561820()
{
  /usr/bin/tar -C "${SCRATCHDIR}" -xvf "${SCRIPTDIR}"/radar-26561820.tar
  assertFalse 'Radar 26561820' "${?}"
}

test_libarchive_r28015866()
{
  /usr/bin/tar -tvf "${SCRIPTDIR}"/radar-28015866.tar
  assertFalse 'Radar 28015866' "${?}"
}

test_libarchive_r28024754()
{
  /usr/bin/tar -C "${SCRATCHDIR}" -xvf "${SCRIPTDIR}"/radar-28024754.tar
  assertFalse 'Radar 28024754' "${?}"
}

test_libarchive_r28101193()
{
  /usr/bin/tar -C "${SCRATCHDIR}" -xvf "${SCRIPTDIR}"/radar-28101193.tar
  assertFalse 'Radar 28101193' "${?}"
  [ ! -f /tmp/myfile ]
  assertTrue 'Radar 28101193 /tmp/myfile exists' "${?}"
}

test_libarchive_r107759049()
{
  /usr/bin/tar -tvf "${SCRATCHDIR}"/radar-107759049.zip
  assertEqual 'Radar 107759049' "${?}" 1
}

. /usr/local/bin/shunit2_coreos
