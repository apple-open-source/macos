#!/bin/sh

OUTFILE=base.ml1_s.part

> $OUTFILE

awk '{ 
  printf "  *		%s			=	pc+%s%%(v[1])\n", $1, $2; 
}' < layoutRename.lst >> $OUTFILE

awk '{ 
  printf "  *		%s(%s)			=	pc+%s(%s)\n", $1, $2, $3, $4; 
}' < variantRename.lst >> $OUTFILE
