#!/bin/bash

# fakerloader.sh
#
# Created by Ethan on 2/28/06.

if [[ "$1" ]]
then
    if [ $1 == "load" ]
    then        #load path

        kextload $2/Contents/Resources/BatteryFakerKEXT.kext

        if [ $? -ne 0 ]       # test kextload exit status
        then
#            echo "Error $? loading"
            exit $?     # and exit with return code if not successful
        fi

#        echo "Success loading"
        sleep 2     # delay 2 seconds to let kext load

#        echo "Killing SystemUIServer"

        # Kill SystemUIServer to refresh BatteryMonitor
        killall SystemUIServer     

        exit        # and exit

    elif [ $1 == "unload" ]        # "unload" path
    then
#        echo "Unloading BatteryFakerKEXT"
        
        kextunload $2/Contents/Resources/BatteryFakerKEXT.kext

        exit $?
    elif [ $1 == "kickbattmon" ]
    then
        # Kill SystemUIServer to refresh BatteryMonitor
        killall SystemUIServer     
    fi
else
#    echo "Bad Arguments"
    exit 5
fi
