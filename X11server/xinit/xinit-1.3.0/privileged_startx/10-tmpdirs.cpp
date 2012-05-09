XCOMM!/bin/sh
XCOMM Copyright (c) 2008-2012 Apple Inc.
XCOMM
XCOMM Permission is hereby granted, free of charge, to any person
XCOMM obtaining a copy of this software and associated documentation files
XCOMM (the "Software"), to deal in the Software without restriction,
XCOMM including without limitation the rights to use, copy, modify, merge,
XCOMM publish, distribute, sublicense, and/or sell copies of the Software,
XCOMM and to permit persons to whom the Software is furnished to do so,
XCOMM subject to the following conditions:
XCOMM
XCOMM The above copyright notice and this permission notice shall be
XCOMM included in all copies or substantial portions of the Software.
XCOMM
XCOMM THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
XCOMM EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
XCOMM MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
XCOMM NONINFRINGEMENT.  IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT
XCOMM HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
XCOMM WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
XCOMM OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
XCOMM DEALINGS IN THE SOFTWARE.
XCOMM
XCOMM Except as contained in this notice, the name(s) of the above
XCOMM copyright holders shall not be used in advertising or otherwise to
XCOMM promote the sale, use or other dealings in this Software without
XCOMM prior written authorization.

XCOMM Make sure these are owned by root

XCOMM Our usage of mktemp fails with GNU, so prefer /usr/bin to hopefully
XCOMM get BSD mktemp
if [ -x /usr/bin/mktemp ] ; then
    MKTEMP=/usr/bin/mktemp
else
    MKTEMP=mktemp
fi

STAT=/usr/bin/stat

for dir in /tmp/.ICE-unix /tmp/.X11-unix /tmp/.font-unix ; do
	success=0
	for attempt in 1 2 3 4 5 ; do
		check=`${STAT} -f '%#p %u %g' ${dir}`
		if [ "${check}" = "041777 0 0" ] ; then
			success=1
			break
		else
			saved=$(${MKTEMP} -d /tmp/${dir}-XXXXXXXX)
			mv ${dir} ${saved}
			echo "${dir} exists but is insecure.  It has been moved into ${saved}"
		fi

		# Use mktemp rather than mkdir to avoid possible security issue
		# if $dir exists and is a symlink (ie protect against a race
		# against the above check)
		if ${MKTEMP} -d ${dir} >& /dev/null ; then
			chmod 1777 $dir
			chown 0:0 $dir
			success=1
			break
		fi
	done

	if [ "${success}" -eq 0 ] ; then
		echo "Could not successfully create ${dir}" >&2
	fi
done
