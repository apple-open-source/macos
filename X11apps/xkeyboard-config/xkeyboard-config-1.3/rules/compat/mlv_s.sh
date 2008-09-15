#!/bin/sh

OUTFILE=base.mlv_s.part

> $OUTFILE

awk '{ 
  printf "  *		%s		%s		=	pc+%s(%s)\n", $1, $2, $3, $4; 
}' < variantRename.lst >> $OUTFILE
