XCOMM!/bin/ksh
XCOMM X11 MIT-MAGIC-COOKIE to SUN-DES-1 auth. 
XCOMM this script switched the current Xservers authentification 
XCOMM (usually MIT-MAGIC-COOKIE-1) to SUN-DES-1.
XCOMM
XCOMM
XCOMM Copyright 2002-2004 by Roland Mainz <roland.mainz@nrubsig.org>.
XCOMM
XCOMM
XCOMM Requirements:
XCOMM - Solaris/Linux/AIX running as NIS+ client (YP/LDAP not supported yet)
XCOMM - user must have proper credentials ("SecureRPC")
XCOMM - script must be able to "guess" the UID of the Xserver
XCOMM
XCOMM Advantages:
XCOMM - User may allow other users to gain access via
XCOMM   % xhost +jigsaw@
XCOMM   instead of moving 128bit cookies
XCOMM
XCOMM Known bugs:
XCOMM - Was not tested on Linux since several months

/* Avoid problems with CPP processing */
#undef unix

umask 077
XCOMM force POSIX binaries
export PATH=/usr/xpg4/bin:/usr/bin:/usr/dt/bin:/usr/openwin/bin

XCOMM debug
alias xxdebug=true
XCOMM alias xxdebug=

XCOMM get full qualified domain name
getFQDN()
{
    getent hosts ${1} | awk "{print \$2}" - 
}

user2netname()
{
    UID=$(id -u $1)
    DOMAINNAME=$(domainname)
    if [ $UID != 0 ] ; then
        netname=unix.$UID@$DOMAINNAME
    else
        netname=unix.$HOSTNAME@$DOMAINNAME
    fi

    # BUG: SecureRPC isn't limited to NIS+ 
    #      (but there is no "getent publickey ...") ...
    # ToDo:
    # - YP name is "publickey.byname"
    # - What name does LDAP use ?
    if [ "`nismatch "auth_name=$netname" cred.org_dir`" != "" ] ; then
        echo "$netname"
    else
        echo "user ${UID} has no entry in cred.org_dir" >&2
        return 1
    fi
    
    return 0
}    


XCOMM pid to username
getUserOfPID()
{
    ps -p $1 -o user,pid | awk "NR != 1 {print \$1}" -
}

XCOMM test if we can access $DISPLAY via SUN-DES-1 auth. using a temporary
XCOMM Xauthority file
dry_run()
{
(
  principal="$1"
  # XAUTHORITY may not be defined
  if [ "$XAUTHORITY" = "" ] ; then
      export XAUTHORITY=~/.Xauthority
  fi
  
  ORIGINAL_XAUTHORITY="${XAUTHORITY:-~/.Xauthority}"
  TMP_XAUTHORITY=/tmp/mit-cookie2sun-des-1tmpxauth_${LOGNAME}_${RANDOM}.xauth
  export XAUTHORITY="$TMP_XAUTHORITY"
  touch "$XAUTHORITY"

  (echo "add $displayhost/unix:$displaynum SUN-DES-1 $principal" ;
   echo "add $displayhost:$displaynum SUN-DES-1 $principal"
  ) | xauth source -
  
  # check if a sample X11 app. (/usr/openwin/bin/xset) can access Xserver...
  if ! xset q 2>/dev/null 1>/dev/null ; then   
    # clean-up
    rm -f "$TMP_XAUTHORITY"
    return 1
  fi

  rm -f "$TMP_XAUTHORITY"
  
  return 0
)
}

XCOMM main

HOSTNAME=$(hostname)
FQDN=$(getFQDN $HOSTNAME)

XCOMM be sure that DISPLAY contains the host name
XCOMM BUGs: 
XCOMM - this does _not_ catch non-tcp connections (like DECnet).
XCOMM - this may not work with IPv6 addresses
displayhost=${DISPLAY%:*}
displaynum=$(x=${DISPLAY#*:}; echo ${x%.*})
if [ "$displayhost" == "" -o "$displayhost" == "localhost" ] ; then
    # fix DISPLAY
    export DISPLAY="${FQDN}:${DISPLAY#*:}"
    displayhost=${DISPLAY%:*}
fi


XCOMM grant access for current user and for user root
XCOMM (a bug in /usr/dt/bin/dtaction requires this for user "root", too -
XCOMM Solaris 7/8 dtaction runns setuid root and opens a display connection
XCOMM before chaning the EUID to the "destination uid"... ;-( ).
xhost +${LOGNAME}@ +$(user2netname root)

XCOMM get X server principal(=user)
XCOMM this may fail if user isn't local
XCOMM unfortunately we cannot get the Xserver PID with a simply API - we 
XCOMM have to "guess" in this case. "pgrep" creates a list of PIDs which may
XCOMM match. Then we create a list of all matching "principals" and test
XCOMM them - item by item...
XCOMM ... step 1: Create list of principals
principal_list=""          # you can add "most common" principals here...
fallback_principal_list="" # you can add "fallback" principals here
                           # (for example, principals for Xterminals (where
                           # the Xserver always runns under the same UID) 
                           # which use SUN-DES-1)
for i in $(pgrep -f ".*X.* :$displaynum*") ; do    
    principal_list="$(user2netname `getUserOfPID $i`) ${principal_list}"
done

xxdebug echo "principal_list=${principal_list}"

XCOMM ... step 2: Test the list of principals
for PRINCIPAL in ${principal_list} ${fallback_principal_list} ; do    
    # make a "dry run" and test whether we really can use SUN-DES-1 auth.
    # for this display using the given principal
    if dry_run "${PRINCIPAL}" ; then
        # remove old MIT-MAGIC-COOKIES and insert SUN-DES-1 cookies
        # Users ~/.Xauthority _must_ be changed in _one_ step to avoid
        # possible race conditions when switching auth. on a "live" 
        # $DISPLAY...
        (echo "remove $displayhost/unix:$displaynum" ;
         echo "remove $displayhost:$displaynum" ;
         echo "add $displayhost/unix:$displaynum SUN-DES-1 $PRINCIPAL" ;
         echo "add $displayhost:$displaynum SUN-DES-1 $PRINCIPAL"
        ) | xauth source -
    
        # success.
        xxdebug echo "success."
        exit 0
    fi
done

echo "${0}: failure; could not establish SUN-DES-1 auth. on $DISPLAY" >&2
xhost -$LOGNAME@ -$(user2netname root) 

XCOMM failure.
xxdebug echo failure.
exit 1
XCOMM EOF.
