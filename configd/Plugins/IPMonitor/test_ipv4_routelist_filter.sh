#!/bin/sh
sed '/Process [0-9][0-9]*: [0-9][0-9]* nodes malloced/d; s/^\(Process \)[0-9][0-9]*:\(.*\)/\1XXXX\2/'
