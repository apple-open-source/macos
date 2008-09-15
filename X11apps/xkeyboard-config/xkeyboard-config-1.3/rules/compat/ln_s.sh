#!/bin/sh

variant=$1

OUTFILE=base.l${variant}_s.part

> $OUTFILE

awk '{ 
  printf "  %s		=	+%s%%(v['${variant}']):'${variant}'\n", $1, $2; 
}' < layoutRename.lst >> $OUTFILE

awk '{ 
  printf "  %s(%s)	=	+%s(%s):'${variant}'\n", $1, $2, $3, $4; 
}' < variantRename.lst >> $OUTFILE
