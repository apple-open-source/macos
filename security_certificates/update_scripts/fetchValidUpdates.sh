#!/bin/sh
# last update 20241113 (kcm)
#
# script to download and archive the current production Valid updates
# (both full and delta updates) on a per-generation basis, and to
# verify that updating from the previous version is successful.

# tools we use (must specify full path if we run as a cron task)
REVDBUTIL="/usr/local/bin/revdbutil"
NSCURL="/usr/bin/nscurl"

TSTAMP=`date +%Y-%m-%d-%H%M%S`
OUTDIR="$HOME/valid-$TSTAMP"
mkdir "$OUTDIR"

VALIDURL="https://valid.apple.com"

function fetchUpdatesForGeneration() {
    GEN="$1"
    UPDATESNAPSHOT="$2"
    # pull down the current production full update
    URL="${VALIDURL}/g$GEN/v0"
    FULLUPDATEFILE="$OUTDIR/prod-g$GEN-vN-full"
    FULLDBFILE="$OUTDIR/g$GENvN.sqlite3"
    echo "Retrieving current full update from $URL"
    "$NSCURL" -o "$FULLUPDATEFILE" "$URL"
    echo "Creating database from current full update"
    FULLOUT=`"$REVDBUTIL" -g $GEN -o "$FULLDBFILE" "$FULLUPDATEFILE"`
    UPDATE=`echo "$FULLOUT" | grep "Update received:" | awk '{print $3}'`
    VERSION=`echo "$FULLOUT" | grep "version:" | awk '{print $2}'`
    STATUS=`echo "$FULLOUT" | grep "Verify result:" | awk '{print $3}'`
    if [ "$STATUS" != "SUCCESS" ] ; then
        echo "ERROR: expected status SUCCESS, got $STATUS" ; exit 1
    elif [ -z "$UPDATE" ] || [ -z "$VERSION" ] ; then
        echo "ERROR: update $UPDATE or version $VERSION not valid" ; exit 1
    else
        mv "$FULLUPDATEFILE" "$OUTDIR/prod-g$GEN-$UPDATE-full"
        FULLUPDATEFILE="$OUTDIR/prod-g$GEN-$UPDATE-full"
        mv "$FULLDBFILE" "$OUTDIR/g$GEN$UPDATE.sqlite3"
        echo "Processed $UPDATE update, generation $GEN, status: $STATUS"
    fi
    echo "Compressing full update file"
    gzip "$FULLUPDATEFILE"
    
    if [ X$UPDATESNAPSHOT != X ]; then
      if [ -d valid_db_snapshot ]; then
        echo "updating valid and version number"
        cp ${OUTDIR}/g${GEN}${UPDATE}.sqlite3 valid_db_snapshot/valid.sqlite3
        plutil -replace Version -integer ${VERSION} valid_db_snapshot/ValidUpdate.plist
        plutil -replace Generation -integer ${GEN} valid_db_snapshot/ValidUpdate.plist
      else
        echo "not running tool in top level directory"
        exit 1
      fi
    fi
    
    # pull down the previous full update, if there is one
    if [ "$VERSION" -gt 1 ] ; then
        OLDVERS=$(( $VERSION - 1 ))
    else
        OLDVERS=0
        echo "No previous update, so no delta update to retrieve"
        echo "Done"
        return
    fi
    URL="${VALIDURL}/o$GEN/v$OLDVERS"
    OLDUPDATEFILE="$OUTDIR/prod-g$GEN-v$OLDVERS-full"
    OLDDBFILE="$OUTDIR/g$GENv$OLDVERS.sqlite3"
    echo "Retrieving previous full update (v$OLDVERS) from $URL"
    "$NSCURL" -o "$OLDUPDATEFILE" "$URL"
    echo "Creating database from previous full update"
    OLDOUT=`"$REVDBUTIL" -g $GEN -o "$OLDDBFILE" "$OLDUPDATEFILE"`

    # pull down the delta update between previous and current versions
    URL="${VALIDURL}/g$GEN/v$OLDVERS"
    DELTAUPDATEFILE="$OUTDIR/prod-g$GEN-v$OLDVERS-to-$UPDATE-update"
    echo "Retrieving delta update (v$OLDVERS to $UPDATE) from $URL"
    "$NSCURL" -o "$DELTAUPDATEFILE" "$URL"
    echo "Applying delta update (v$OLDVERS to $UPDATE)"
    UPDATEOUT=`/usr/bin/time "$REVDBUTIL" -g $GEN -o "$OLDDBFILE" "$DELTAUPDATEFILE"`
    mv "$OLDDBFILE" "$OUTDIR/g$GEN$UPDATE-updated.sqlite3"

    # compress the old update and delta update files
    echo "Compressing files"
    gzip "$OLDUPDATEFILE" "$DELTAUPDATEFILE"

    echo "Done"
}

fetchUpdatesForGeneration 3
fetchUpdatesForGeneration 4
fetchUpdatesForGeneration 5
fetchUpdatesForGeneration 6
fetchUpdatesForGeneration 7 update
fetchUpdatesForGeneration 8

echo "output directory: ${OUTDIR}"
exit 0
