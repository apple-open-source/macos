#!/bin/sh
headerdoc2html -o /tmp pccard/*.h
gatherHeaderDoc /tmp
open /tmp/MasterTOC.html
