REM 
/* OS/2 generate header files */
/* $XFree86: xc/lib/dps/genheader.cmd,v 1.2 2000/05/18 23:46:14 dawes Exp $ */
cat psclrops.h psctrlops.h psctxtops.h psdataops.h psfontops.h psgsttops.h psioops.h psmathops.h psmtrxops.h psmiscops.h pspntops.h pspathops.h pssysops.h pswinops.h psopstack.h psXops.h psl2ops.h >.ph
sed -e "/^$$/D" -e "/#/D" -e "/^\//D" -e "/^   gener/D" -e "/^.\//D" .ph | sort >.sort
awk "/;/ {print;printf(\"\n\");}" .sort >.ttt
cat psname.txt header.txt psifdef.txt .ttt psendif.txt > psops.h.os2
rm .ph .sort .ttt

cat dpsclrops.h dpsctrlops.h dpsctxtops.h dpsdataops.h dpsfontops.h dpsgsttops.h dpsioops.h dpsmathops.h dpsmtrxops.h dpsmiscops.h dpspntops.h dpspathops.h dpssysops.h dpswinops.h dpsopstack.h dpsXops.h dpsl2ops.h >.ph
sed -e "/^$$/D" -e "/#/D" -e "/^\//D" -e "/^   gener/D" -e "/^.\//D" .ph | sort >.sort
awk "/;/ {print;printf(\"\n\");}" .sort >.ttt
cat dpsname.txt header.txt dpsifdef.txt .ttt dpsendif.txt > dpsops.h.os2
rm .ph .sort .ttt
