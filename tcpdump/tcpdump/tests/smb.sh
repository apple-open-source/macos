#!/bin/sh

exitcode=0

passed=`cat .passed`
failed=`cat .failed`

# Only attempt OpenSSL-specific tests when compiled with the library.

if grep '^#define ENABLE_SMB 1$' ../config.h &>/dev/null
then
    cat SMBLIST | while read name input output options
    do
        case $name in
            \#*) continue;;
            '') continue;;
        esac
        rm -f core
        [ "$only" != "" -a "$name" != "$only" ] && continue
        SRCDIR=${srcdir}
        export SRCDIR
        # I hate shells with their stupid, useless subshells.
        passed=`cat ${passedfile}`
        failed=`cat ${failedfile}`
        (cd tests  # run TESTonce in tests directory
         if TESTonce.sh $name $input $output "$options"
         then
             passed=`expr $passed + 1`
             echo $passed >.passed
         else
             failed=`expr $failed + 1`
             echo $failed >.failed
         fi
         if [ -d COREFILES ]; then
             if [ -f core ]; then mv core COREFILES/$name.core; fi
         fi)
    done
    # I hate shells with their stupid, useless subshells.
    passed=`cat .passed`
    failed=`cat .failed`
else
    ### APPLE modification starts
    cat SMBLIST | while read name input output options
    do
        case $name in
            \#*) continue;;
            '') continue;;
        esac
	printf '    %-35s: TEST SKIPPED (SMB printing not enabled)\n' $name
    done
    #printf '    %-35s: TEST SKIPPED (SMB printing not enabled)\n' $test_name
    ### APPLE modification ends
fi

exit $exitcode
