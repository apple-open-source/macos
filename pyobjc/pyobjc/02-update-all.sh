#!/bin/sh


# No longer need to run update twice, update_exceptions
# now also updates metadata :-)
#TARGETS='update_exceptions update_metadata'
TARGETS='update_exceptions'

for proj in  \
	pyobjc-framework-Cocoa 
do 
(
	echo $proj
	cd ${proj}
	python setup.py ${TARGETS}
)
done

for proj in `ls | grep ^pyobjc-framework`
do 
(
	case ${proj} in 
	pyobjc-framework-Cocoa) : ;; 
	*)
		echo $proj
		cd ${proj}
		python setup.py ${TARGETS}
		;;
	esac
) 
done
