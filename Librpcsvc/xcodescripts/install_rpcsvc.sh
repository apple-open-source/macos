#!/bin/sh
set -e

if [ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] ; then
    destination="$DSTROOT$SDKROOT"/usr/local/include/rpcsvc
else 
    destination="$DSTROOT"/usr/include/rpcsvc
fi

printf "Creating installation directory\n"
printf " install -m 0755 -d %s\n" "$destination"
install -m 0755 -d "$destination"

for xfile in "$SRCROOT"/*.x; do
	hfile=`basename "$xfile" .x`.h

	printf "Installing %s\n" `basename "$xfile"`

	printf " rpcgen -h -o %s %s\n" "$OBJROOT"/"$hfile" "$xfile"
	rpcgen -h -o "$OBJROOT"/"$hfile" "$xfile"

	printf " install -m 0444 %s %s\n" "$OBJROOT"/"$hfile" "$destination"
	install -m 0444 "$OBJROOT"/"$hfile" "$destination"

	printf " install -m 0444 %s %s\n" "$xfile" "$destination"
	install -m 0444 "$xfile" "$destination"
done
