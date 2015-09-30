#!/bin/sh

#  KechainItemsAclTest.sh
#  Security
#
#  Created by Vratislav Kužela on 22/08/14.
#

AGRP="testACL"
SVCE="testACLService"
OPERATION="create"

for i in $@; do
    if [[ "$i" =~ "agrp=" ]]; then
        AGRP=${i#*=}
    elif [[ "$i" =~ "svce=" ]]; then
        SVCE=${i#*=}
    elif [[ "$i" =~ "op=create" ]]; then
        OPERATION="create"
    elif [[ "$i" =~ "op=delete" ]]; then
        OPERATION="delete"
    fi
done

if [ "$OPERATION" = "create" ]; then
security item -a class=genp,svce=$SVCE,agrp=$AGRP,acct=acct1,accc="ak"
security item -a class=genp,svce=$SVCE,agrp=$AGRP,acct=acct2,accc="ak;od:true;odel:true"
security item -a class=genp,svce=$SVCE,agrp=$AGRP,acct=acct3,accc="ak;od:cpo(DeviceOwnerAuthentication);odel:true"
security item -a class=genp,svce=$SVCE,agrp=$AGRP,acct=acct4,accc="akpu"
security item -a class=genp,svce=$SVCE,agrp=$AGRP,acct=acct5,accc="akpu;od:true;odel:true"
security item -a class=genp,svce=$SVCE,agrp=$AGRP,acct=acct6,accc="akpu;od:cpo(DeviceOwnerAuthentication);odel:true"

security item -a class=inet,agrp=$AGRP,acct=acct1,accc="ak"
security item -a class=inet,agrp=$AGRP,acct=acct2,accc="ak;od:true;odel:true"
security item -a class=inet,agrp=$AGRP,acct=acct3,accc="ak;od:cpo(DeviceOwnerAuthentication);odel:true"
security item -a class=inet,agrp=$AGRP,acct=acct4,accc="akpu"
security item -a class=inet,agrp=$AGRP,acct=acct5,accc="akpu;od:true;odel:true"
security item -a class=inet,agrp=$AGRP,acct=acct6,accc="akpu;od:cpo(DeviceOwnerAuthentication);odel:true"

security item -a class=cert,agrp=$AGRP,slnr=slnr1,accc="ak"
security item -a class=cert,agrp=$AGRP,slnr=slnr2,accc="ak;od:true;odel:true"
security item -a class=cert,agrp=$AGRP,slnr=slnr3,accc="ak;od:cpo(DeviceOwnerAuthentication);odel:true"
security item -a class=cert,agrp=$AGRP,slnr=slnr4,accc="akpu"
security item -a class=cert,agrp=$AGRP,slnr=slnr5,accc="akpu;od:true;odel:true"
security item -a class=cert,agrp=$AGRP,slnr=slnr6,accc="akpu;od:cpo(DeviceOwnerAuthentication);odel:true"

security item -a class=keys,agrp=$AGRP,klbl=hash1,accc="ak"
security item -a class=keys,agrp=$AGRP,klbl=hash2,accc="ak;od:true;odel:true"
security item -a class=keys,agrp=$AGRP,klbl=hash3,accc="ak;od:cpo(DeviceOwnerAuthentication);odel:true"
security item -a class=keys,agrp=$AGRP,klbl=hash4,accc="akpu"
security item -a class=keys,agrp=$AGRP,klbl=hash5,accc="akpu;od:true;odel:true"
security item -a class=keys,agrp=$AGRP,klbl=hash6,accc="akpu;od:cpo(DeviceOwnerAuthentication);odel:true"

elif [ "$OPERATION" = "delete" ]; then

security item -D class=genp,agrp=$AGRP
security item -D class=inet,agrp=$AGRP
security item -D class=cert,agrp=$AGRP
security item -D class=keys,agrp=$AGRP

fi
