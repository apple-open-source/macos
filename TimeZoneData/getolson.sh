#!/bin/sh

rsync --links rsync://rsync.iana.org/tz/tzdata-latest.tar.gz /tmp/tzdata-latest.tar.gz
rsync --links rsync://rsync.iana.org/tz/`readlink /tmp/tzdata-latest.tar.gz` ./

rsync --links rsync://rsync.iana.org/tz/tzcode-latest.tar.gz /tmp/tzcode-latest.tar.gz
rsync --links rsync://rsync.iana.org/tz/`readlink /tmp/tzcode-latest.tar.gz` ./
