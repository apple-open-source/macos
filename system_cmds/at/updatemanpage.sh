#!/bin/sh -

# Pulled from FreeBSD usr.bin/at/Makefile

# Some from system_cmds.xcodeproj/project.pbxproj, some from
# atrun

ATSPOOL_DIR=/usr/lib/cron/spool
ATJOB_DIR=/usr/lib/cron/jobs
DEFAULT_BATCH_QUEUE=b
DEFAULT_AT_QUEUE=a
LOADAVG_MX=1.5
PERM_PATH=/usr/lib/cron
LOCKFILE=".lockfile"

# added -i for inplace editing
sed -i .bak -e \
		"s@_ATSPOOL_DIR@${ATSPOOL_DIR}@g; \
		s@_ATJOB_DIR@${ATJOB_DIR}@g; \
		s@_DEFAULT_BATCH_QUEUE@${DEFAULT_BATCH_QUEUE}@g; \
		s@_DEFAULT_AT_QUEUE@${DEFAULT_AT_QUEUE}@g; \
		s@_LOADAVG_MX@${LOADAVG_MX}@g; \
		s@_PERM_PATH@${PERM_PATH}@g; \
		s@_LOCKFILE@${LOCKFILE}@g" \
	at.1
