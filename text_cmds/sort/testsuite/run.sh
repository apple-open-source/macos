#!/bin/sh

#export GNUSORT_NUMERIC_COMPATIBILITY=x
#export GNUSORT_COMPATIBLE_BLANKS=x

TESTED_SORT=../../text_cmds/sort/sort
ORIG_SORT=../../text_cmds_orig/sort/sort

FILECMP=cmp

INPUT_FILE=sample.txt
BIG_INPUT_FILE=bigsample.txt

ERRORS_FILE=errors.log

OUT_DIR=tmp

# clean

rm -rf ${OUT_DIR}
mkdir -p ${OUT_DIR}
rm -rf ${ERRORS_FILE}

# ru_RU.KOI8-R C ru_RU.ISO-8859-5 en_US.ISO8859-15 zh_HK.Big5HKSCS  
#
# ru KOI-8 is an "irregular" locale with non-trivial ordering.
# zh* is a 2-bytes locale.

for lang in en_US.UTF-8 C en_US.ISO8859-15 zh_HK.Big5HKSCS ru_RU.KOI8-R ru_RU.ISO-8859-5
do

    export LANG=${lang}

    for KEYS in -srh -sfrudb -Vs -sM -siz 
    do
	
	echo ${LANG} ${KEYS}

	if [ ${LANG} = "ru_RU.KOI8-R" ] && [ ${KEYS} = "-srh" ] ; then
	    
	    # numeric sorting in ru_RU.KOI8-R incompatible because the thousands separator bug fixed,
	    # for better compatibility with the new GNU sort.
	    # (ru_RU.KOI8-R uses space as thousands separator)
	    
	    continue
	fi

	time ${ORIG_SORT} ${KEYS} ${BIG_INPUT_FILE} -o ${OUT_DIR}/big_orig

	for PARALLEL in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17
	do
	    
	    echo --parallel ${PARALLEL}
	    
	    time ${TESTED_SORT} --parallel ${PARALLEL} ${KEYS} ${BIG_INPUT_FILE} -o ${OUT_DIR}/big_new
	    ER=$?
	    if ! [ ${ER} -eq 0 ] ; then
		echo ${LANG} ${KEYS} big crash --parallel ${PARALLEL} >> ${ERRORS_FILE}
		exit
	    fi
	    
	    if ! ${FILECMP} ${OUT_DIR}/big_new ${OUT_DIR}/big_orig >${OUT_DIR}/res.0.0.big 2>&1 ; then
		echo ${LANG} ${KEYS} big error --parallel ${PARALLEL} >> ${ERRORS_FILE}
	    fi
	    time ${TESTED_SORT} --parallel ${PARALLEL} -c ${KEYS} ${OUT_DIR}/big_new
	    ER=$?
	    if ! [ ${ER} -eq 0 ] ; then
		echo ${LANG} ${KEYS} -c big error --parallel ${PARALLEL} >> ${ERRORS_FILE}
	    fi
	    rm -rf ${OUT_DIR}/res.0.0.big
	    rm -rf ${OUT_DIR}/big_new
	done

	rm -rf ${OUT_DIR}/big_orig
	
	${TESTED_SORT} ${KEYS} ${INPUT_FILE} -o ${OUT_DIR}/sik1
	ER=$?
	if ! [ ${ER} -eq 0 ] ; then
	    echo ${LANG} ${KEYS} crash >> ${ERRORS_FILE}
	    exit
	fi
	${ORIG_SORT} ${KEYS} ${INPUT_FILE} -o ${OUT_DIR}/sik2
	if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.0.0 2>&1 ; then
	    echo ${LANG} ${KEYS}  error >> ${ERRORS_FILE}
	fi
	${TESTED_SORT} -c ${KEYS}  ${OUT_DIR}/sik1
	ER=$?
	if ! [ ${ER} -eq 0 ] ; then
	    echo ${LANG} ${KEYS}  -c error  >> ${ERRORS_FILE}
	fi
	rm ${OUT_DIR}/res.0.0
	
	${TESTED_SORT} ${KEYS} -t " "  ${INPUT_FILE} -o ${OUT_DIR}/sik1
	ER=$?
	if ! [ ${ER} -eq 0 ] ; then
	    echo ${LANG} ${KEYS} -t " " crash >> ${ERRORS_FILE}
	    exit
	fi
	${ORIG_SORT} ${KEYS} -t " "  ${INPUT_FILE} -o ${OUT_DIR}/sik2
	if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.0.0 2>&1 ; then
	    echo ${LANG} ${KEYS} error -t " " >> ${ERRORS_FILE}
	fi
	${TESTED_SORT} -c -t " " ${KEYS}  ${OUT_DIR}/sik1
	ER=$?
	if ! [ ${ER} -eq 0 ] ; then
	    echo $? ${LANG} ${KEYS}  -t " " -c error >> ${ERRORS_FILE}
	fi
	rm ${OUT_DIR}/res.0.0
	
	${TESTED_SORT} ${KEYS} -t "|"  ${INPUT_FILE} -o ${OUT_DIR}/sik1
	ER=$?
	if ! [ ${ER} -eq 0 ] ; then
	    echo ${LANG} ${KEYS} -t "|" crash >> ${ERRORS_FILE}
	    exit
	fi
	${ORIG_SORT} ${KEYS} -t "|"  ${INPUT_FILE} -o ${OUT_DIR}/sik2
	if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.0.0 2>&1 ; then
	    echo ${LANG} ${KEYS}  error -t "|" >> ${ERRORS_FILE}
	fi
	${TESTED_SORT} -c -t "|" ${KEYS}  ${OUT_DIR}/sik1
	ER=$?
	if ! [ ${ER} -eq 0 ] ; then
	    echo ${LANG} ${KEYS}  -c -t "|" error >> ${ERRORS_FILE}
	fi
	rm ${OUT_DIR}/res.0.0
	
	${TESTED_SORT} ${KEYS} -t '\0' ${INPUT_FILE} -o ${OUT_DIR}/sik1
	ER=$?
	if ! [ ${ER} -eq 0 ] ; then
	    echo ${LANG} ${KEYS} -t 0 crash >> ${ERRORS_FILE}
	    exit
	fi
	${ORIG_SORT} ${KEYS} -t '\0' ${INPUT_FILE} -o ${OUT_DIR}/sik2
	if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.0.0 2>&1 ; then
	    echo ${LANG} ${KEYS} error -t '\0' >> ${ERRORS_FILE}
	fi
	${TESTED_SORT} -c -t '\0' ${KEYS} ${OUT_DIR}/sik1
	ER=$?
	if ! [ ${ER} -eq 0 ] ; then
	    echo ${LANG} ${KEYS} -c -t '\0' error >> ${ERRORS_FILE}
	fi
	rm ${OUT_DIR}/res.0.0
	
	for f1 in 1 2 3 4 5 6 7 8 9
	do
	    for c1 in 1 2 3 4 5 10 15 20 25 30
	    do
		echo ${LANG} ${KEYS} ${f1} ${c1}
		
		${TESTED_SORT} ${KEYS} +${f1}.${c1} ${INPUT_FILE} -o ${OUT_DIR}/sik1
		ER=$?
		if ! [ ${ER} -eq 0 ] ; then
	        echo ${LANG} ${KEYS} +${f1}.${c1} crash +- >> ${ERRORS_FILE}
	        exit
		fi
		${ORIG_SORT} ${KEYS} +${f1}.${c1} ${INPUT_FILE} -o ${OUT_DIR}/sik2
		if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.${f1}.${c1} 2>&1 ; then
		    echo ${LANG} ${KEYS} +${f1}.${c1} error +- >> ${ERRORS_FILE}
		fi
		${TESTED_SORT} -c ${KEYS} +${f1}.${c1} ${OUT_DIR}/sik1
		ER=$?
		if ! [ ${ER} -eq 0 ] ; then
		    echo ${LANG} ${KEYS} +${f1}.${c1} -c error +- >> ${ERRORS_FILE}
		fi
		rm ${OUT_DIR}/res.${f1}.${c1}

		${TESTED_SORT} ${KEYS} -k${f1}.${c1} ${INPUT_FILE} -o ${OUT_DIR}/sik1
		ER=$?
		if ! [ ${ER} -eq 0 ] ; then
	        echo ${LANG} ${KEYS} -k${f1}.${c1} crash >> ${ERRORS_FILE}
	        exit
		fi
		${ORIG_SORT} ${KEYS} -k${f1}.${c1} ${INPUT_FILE} -o ${OUT_DIR}/sik2
		if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.${f1}.${c1} 2>&1 ; then
		    echo ${LANG} ${KEYS} -k${f1}.${c1} error >> ${ERRORS_FILE}
		fi
		${TESTED_SORT} -c ${KEYS} -k${f1}.${c1} ${OUT_DIR}/sik1
		ER=$?
		if ! [ ${ER} -eq 0 ] ; then
		    echo ${LANG} ${KEYS} -k${f1}.${c1} -c error >> ${ERRORS_FILE}
		fi
		rm ${OUT_DIR}/res.${f1}.${c1}

		${TESTED_SORT} ${KEYS} -k${f1}.${c1}b ${INPUT_FILE} -o ${OUT_DIR}/sik1
		ER=$?
		if ! [ ${ER} -eq 0 ] ; then
	        echo ${LANG} ${KEYS} -k${f1}.${c1}b crash >> ${ERRORS_FILE}
	        exit
		fi
		${ORIG_SORT} ${KEYS} -k${f1}.${c1}b ${INPUT_FILE} -o ${OUT_DIR}/sik2
		if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.${f1}.${c1} 2>&1 ; then
		    echo ${LANG} ${KEYS} -k${f1}.${c1}b error >> ${ERRORS_FILE}
		fi
		${TESTED_SORT} -c ${KEYS} -k${f1}.${c1}b ${OUT_DIR}/sik1
		ER=$?
		if ! [ ${ER} -eq 0 ] ; then
		    echo ${LANG} ${KEYS} -k${f1}.${c1}b -c error >> ${ERRORS_FILE}
		fi
		rm ${OUT_DIR}/res.${f1}.${c1}
		
		${TESTED_SORT} ${KEYS} -t " " -k${f1}.${c1} ${INPUT_FILE} -o ${OUT_DIR}/sik1
		ER=$?
		if ! [ ${ER} -eq 0 ] ; then
	            echo ${LANG} ${KEYS} -t -k${f1}.${c1} crash >> ${ERRORS_FILE}
	            exit
		fi
		${ORIG_SORT} ${KEYS} -t " " -k${f1}.${c1} ${INPUT_FILE} -o ${OUT_DIR}/sik2
		if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.${f1}.${c1} 2>&1 ; then
		    echo ${LANG} ${KEYS} -k${f1}.${c1} error -t " " >> ${ERRORS_FILE}
		fi
		${TESTED_SORT} -c -t " " ${KEYS} -k${f1}.${c1} ${OUT_DIR}/sik1
		ER=$?
		if ! [ ${ER} -eq 0 ] ; then
		    echo ${LANG} ${KEYS} -k${f1}.${c1} -t " " -c error >> ${ERRORS_FILE}
		fi
		rm ${OUT_DIR}/res.${f1}.${c1}

		if [ ${LANG} != "ru_RU.KOI8-R" ] ; then
		    
		    # numeric sorting in ru_RU.KOI8-R incompatible because the thousands separator bug fixed,
		    # for better compatibility with the new GNU sort.
		    # (ru_RU.KOI8-R uses space as thousands separator)
		    
		    ${TESTED_SORT} ${KEYS} -t " " -k${f1}.${c1}n ${INPUT_FILE} -o ${OUT_DIR}/sik1
		    ER=$?
		    if ! [ ${ER} -eq 0 ] ; then
			echo ${LANG} ${KEYS} -k${f1}.${c1}n crash >> ${ERRORS_FILE}
			exit
		    fi
		    ${ORIG_SORT} ${KEYS} -t " " -k${f1}.${c1}n ${INPUT_FILE} -o ${OUT_DIR}/sik2
		    if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.${f1}.${c1} 2>&1 ; then
			echo ${LANG} ${KEYS} -k${f1}.${c1} error -t " " n >> ${ERRORS_FILE}
		    fi 
		    ${TESTED_SORT} -c -t " " ${KEYS} -k${f1}.${c1}n ${OUT_DIR}/sik1
		    ER=$?
		    if ! [ ${ER} -eq 0 ] ; then
			echo ${LANG} ${KEYS} -k${f1}.${c1} -c -t " " n error >> ${ERRORS_FILE}
		    fi
		    rm ${OUT_DIR}/res.${f1}.${c1}
		fi
		
		${TESTED_SORT} ${KEYS} -t "|" -k${f1}.${c1} ${INPUT_FILE} -o ${OUT_DIR}/sik1
		ER=$?
		if ! [ ${ER} -eq 0 ] ; then
	            echo ${LANG} ${KEYS} -t "|" -k${f1}.${c1} crash >> ${ERRORS_FILE}
	            exit
		fi
		${ORIG_SORT} ${KEYS} -t "|" -k${f1}.${c1} ${INPUT_FILE} -o ${OUT_DIR}/sik2
		if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.${f1}.${c1} 2>&1 ; then
		    echo ${LANG} ${KEYS} -k${f1}.${c1} error -t "|" >> ${ERRORS_FILE}
		fi
		${TESTED_SORT} -c -t "|" ${KEYS} -k${f1}.${c1} ${OUT_DIR}/sik1
		ER=$?
		if ! [ ${ER} -eq 0 ] ; then
		    echo ${LANG} ${KEYS} -k${f1}.${c1} -c -t "|" error >> ${ERRORS_FILE}
		fi
		rm ${OUT_DIR}/res.${f1}.${c1}
		
		for f2 in 1 2 3 4 5 6 7 8 9 10
		do
		    for c2 in 0 1 2 3 4 5 10 15 20 25 30
		    do
			echo ${LANG} ${KEYS} ${f1} ${c1} ${f2} ${c2}
			
			${TESTED_SORT} ${KEYS} +${f1}.${c1} -${f2}.${c2} ${INPUT_FILE} -o ${OUT_DIR}/sik1
	    		ER=$?
	    		if ! [ ${ER} -eq 0 ] ; then
	        	    echo ${LANG} ${KEYS} +${f1}.${c1} -${f2}.${c2} crash >> ${ERRORS_FILE}
	        	    exit
			fi
			${ORIG_SORT} ${KEYS} +${f1}.${c1} -${f2}.${c2} ${INPUT_FILE} -o ${OUT_DIR}/sik2
			if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.${f1}.${c1}.${f2}.${c2} 2>&1 ; then
			    echo ${LANG} ${KEYS} +${f1}.${c1} -${f2}.${c2} error +- >> ${ERRORS_FILE}
			fi
			${TESTED_SORT} -c ${KEYS} +${f1}.${c1} -${f2}.${c2} ${OUT_DIR}/sik1
			ER=$?
			if ! [ ${ER} -eq 0 ] ; then
			    echo ${LANG} ${KEYS} +${f1}.${c1} -${f2}.${c2} -c error +- >> ${ERRORS_FILE}
			fi
			rm ${OUT_DIR}/res.${f1}.${c1}.${f2}.${c2}

			${TESTED_SORT} ${KEYS} -k${f1}.${c1},${f2}.${c2} ${INPUT_FILE} -o ${OUT_DIR}/sik1
	    		ER=$?
	    		if ! [ ${ER} -eq 0 ] ; then
	        	    echo ${LANG} ${KEYS} -k${f1}.${c1},${f2}.${c2} crash >> ${ERRORS_FILE}
	        	    exit
			fi
			${ORIG_SORT} ${KEYS} -k${f1}.${c1},${f2}.${c2} ${INPUT_FILE} -o ${OUT_DIR}/sik2
			if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.${f1}.${c1}.${f2}.${c2} 2>&1 ; then
			    echo ${LANG} ${KEYS} -k${f1}.${c1}.${f2}.${c2} error >> ${ERRORS_FILE}
			fi
			${TESTED_SORT} -c ${KEYS} -k${f1}.${c1},${f2}.${c2} ${OUT_DIR}/sik1
			ER=$?
			if ! [ ${ER} -eq 0 ] ; then
			    echo ${LANG} ${KEYS} -k${f1}.${c1},${f2}.${c2} -c error >> ${ERRORS_FILE}
			fi
			rm ${OUT_DIR}/res.${f1}.${c1}.${f2}.${c2}

			${TESTED_SORT} ${KEYS} -k${f1}.${c1}b,${f2}.${c2} ${INPUT_FILE} -o ${OUT_DIR}/sik1
	    		ER=$?
	    		if ! [ ${ER} -eq 0 ] ; then
	        	    echo ${LANG} ${KEYS} -k${f1}.${c1}b,${f2}.${c2} crash >> ${ERRORS_FILE}
	        	    exit
			fi
			${ORIG_SORT} ${KEYS} -k${f1}.${c1}b,${f2}.${c2} ${INPUT_FILE} -o ${OUT_DIR}/sik2
			if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.${f1}.${c1}.${f2}.${c2} 2>&1 ; then
			    echo ${LANG} ${KEYS} -k${f1}.${c1}.b.${f2}.${c2} error >> ${ERRORS_FILE}
			fi
			${TESTED_SORT} -c ${KEYS} -k${f1}.${c1}b,${f2}.${c2} ${OUT_DIR}/sik1
			ER=$?
			if ! [ ${ER} -eq 0 ] ; then
			    echo ${LANG} ${KEYS} -k${f1}.${c1}b,${f2}.${c2} -c error >> ${ERRORS_FILE}
			fi
			rm ${OUT_DIR}/res.${f1}.${c1}.${f2}.${c2}
			
			${TESTED_SORT} ${KEYS} -t " " -k${f1}.${c1},${f2}.${c2} ${INPUT_FILE} -o ${OUT_DIR}/sik1
	    		ER=$?
	    		if ! [ ${ER} -eq 0 ] ; then
	        	    echo ${LANG} ${KEYS} -t " " -k${f1}.${c1},${f2}.${c2} crash >> ${ERRORS_FILE}
	        	    exit
			fi
			${ORIG_SORT} ${KEYS} -t " " -k${f1}.${c1},${f2}.${c2} ${INPUT_FILE} -o ${OUT_DIR}/sik2
			if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.${f1}.${c1}.${f2}.${c2} 2>&1 ; then
			    echo ${LANG} ${KEYS} -k${f1}.${c1}.${f2}.${c2} error -t " " >> ${ERRORS_FILE}
			fi
			${TESTED_SORT} -c -t " " ${KEYS} -k${f1}.${c1},${f2}.${c2} ${OUT_DIR}/sik1
			ER=$?
			if ! [ ${ER} -eq 0 ] ; then
			    echo ${LANG} ${KEYS} -k${f1}.${c1},${f2}.${c2} -c -t " " error >> ${ERRORS_FILE}
			fi
			rm ${OUT_DIR}/res.${f1}.${c1}.${f2}.${c2}

			if [ ${LANG} != "ru_RU.KOI8-R" ] ; then
		    
			    # numeric sorting in ru_RU.KOI8-R incompatible because the thousands separator bug fixed,
			    # for better compatibility with the new GNU sort.
			    # (ru_RU.KOI8-R uses space as thousands separator)
			    
			    ${TESTED_SORT} ${KEYS} -t " " -k${f1}.${c1}n,${f2}.${c2} ${INPUT_FILE} -o ${OUT_DIR}/sik1
	    		    ER=$?
	    		    if ! [ ${ER} -eq 0 ] ; then
	        		echo ${LANG} ${KEYS} -t " " -k${f1}.${c1}n,${f2}.${c2} crash >> ${ERRORS_FILE}
	        		exit
			    fi
			    ${ORIG_SORT} ${KEYS} -t " " -k${f1}.${c1}n,${f2}.${c2} ${INPUT_FILE} -o ${OUT_DIR}/sik2
			    if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.${f1}.${c1}.${f2}.${c2} 2>&1 ; then
				echo ${LANG} ${KEYS} -t " " -k${f1}.${c1}.${f2}.${c2} error n >> ${ERRORS_FILE}
			    fi
			    ${TESTED_SORT} -c -t " " ${KEYS} -k${f1}.${c1}n,${f2}.${c2} ${OUT_DIR}/sik1
			    ER=$?
			    if ! [ ${ER} -eq 0 ] ; then
				echo ${LANG} ${KEYS} -k${f1}.${c1},${f2}.${c2} -c -t " " n error >> ${ERRORS_FILE}
			    fi
			    rm ${OUT_DIR}/res.${f1}.${c1}.${f2}.${c2}
			
			    ${TESTED_SORT} ${KEYS} -t '\0' -k${f1}.${c1}n,${f2}.${c2} ${INPUT_FILE} -o ${OUT_DIR}/sik1
	    		    ER=$?
	    		    if ! [ ${ER} -eq 0 ] ; then
	        		echo ${LANG} ${KEYS} -t '\0' -k${f1}.${c1}n,${f2}.${c2} crash >> ${ERRORS_FILE}
	        		exit
			    fi
			    ${ORIG_SORT} ${KEYS} -t '\0' -k${f1}.${c1}n,${f2}.${c2} ${INPUT_FILE} -o ${OUT_DIR}/sik2
			    if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.${f1}.${c1}.${f2}.${c2} 2>&1 ; then
				echo ${LANG} ${KEYS} -k${f1}.${c1}.${f2}.${c2} error -t '\0' n >> ${ERRORS_FILE}
			    fi
			    ${TESTED_SORT} -c -t '\0' ${KEYS} -k${f1}.${c1}n,${f2}.${c2} ${OUT_DIR}/sik1
			    ER=$?
			    if ! [ ${ER} -eq 0 ] ; then
				echo ${LANG} ${KEYS} -k${f1}.${c1},${f2}.${c2} -c -t '\0' n error >> ${ERRORS_FILE}
			    fi
			    rm ${OUT_DIR}/res.${f1}.${c1}.${f2}.${c2}
			fi
			
			${TESTED_SORT} ${KEYS} -t "|" -k${f1}.${c1},${f2}.${c2} ${INPUT_FILE} -o ${OUT_DIR}/sik1
	    		ER=$?
	    		if ! [ ${ER} -eq 0 ] ; then
	        	    echo ${LANG} ${KEYS} -t "|" -k${f1}.${c1},${f2}.${c2} crash >> ${ERRORS_FILE}
	        	    exit
			fi
			${ORIG_SORT} ${KEYS} -t "|" -k${f1}.${c1},${f2}.${c2} ${INPUT_FILE} -o ${OUT_DIR}/sik2
			if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.${f1}.${c1}.${f2}.${c2} 2>&1 ; then
			    echo ${LANG} ${KEYS} -k${f1}.${c1}.${f2}.${c2} error -t "|" >> ${ERRORS_FILE}
			fi
			${TESTED_SORT} -c -t "|" ${KEYS} -k${f1}.${c1},${f2}.${c2} ${OUT_DIR}/sik1
			ER=$?
			if ! [ ${ER} -eq 0 ] ; then
			    echo ${LANG} ${KEYS} -k${f1}.${c1},${f2}.${c2} -c -t "|" error >> ${ERRORS_FILE}
			fi
			rm ${OUT_DIR}/res.${f1}.${c1}.${f2}.${c2}
			
			${TESTED_SORT} ${KEYS} -t "|" -k${f1}.${c1},${f2}.${c2} -k${f2}.${c1},${f1}.${c2} ${INPUT_FILE} -o ${OUT_DIR}/sik1
	    		ER=$?
	    		if ! [ ${ER} -eq 0 ] ; then
	        	    echo ${LANG} ${KEYS} -t "|" -k${f1}.${c1},${f2}.${c2} -k${f2}.${c1},${f1}.${c2} crash >> ${ERRORS_FILE}
	        	    exit
			fi
			${ORIG_SORT} ${KEYS} -t "|" -k${f1}.${c1},${f2}.${c2} -k${f2}.${c1},${f1}.${c2} ${INPUT_FILE} -o ${OUT_DIR}/sik2
			if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.${f1}.${c1}.${f2}.${c2} 2>&1 ; then
			    echo ${LANG} ${KEYS} -k${f1}.${c1}.${f2}.${c2} error -t "|" 2k >> ${ERRORS_FILE}
			fi
			${TESTED_SORT} -c -t "|" ${KEYS} -k${f1}.${c1},${f2}.${c2} -k${f2}.${c1},${f1}.${c2} ${OUT_DIR}/sik1
			ER=$?
			if ! [ ${ER} -eq 0 ] ; then
			    echo ${LANG} ${KEYS} -k${f1}.${c1},${f2}.${c2} -c -t "|" 2k error >> ${ERRORS_FILE}
			fi
			rm ${OUT_DIR}/res.${f1}.${c1}.${f2}.${c2}

			${TESTED_SORT} ${KEYS} -k${f1}.${c1}b,${f2}.${c2} -k${f2}.${c1},${f1}.${c2} ${INPUT_FILE} -o ${OUT_DIR}/sik1
	    		ER=$?
	    		if ! [ ${ER} -eq 0 ] ; then
	        	    echo ${LANG} ${KEYS} -k${f1}.${c1}b,${f2}.${c2} -k${f2}.${c1},${f1}.${c2} crash >> ${ERRORS_FILE}
	        	    exit
			fi
			${ORIG_SORT} ${KEYS} -k${f1}.${c1}b,${f2}.${c2} -k${f2}.${c1},${f1}.${c2} ${INPUT_FILE} -o ${OUT_DIR}/sik2
			if ! ${FILECMP} ${OUT_DIR}/sik1 ${OUT_DIR}/sik2 >${OUT_DIR}/res.${f1}.${c1}.${f2}.${c2} 2>&1 ; then
			    echo ${LANG} ${KEYS} -k${f1}.${c1}.b.${f2}.${c2} error 2k >> ${ERRORS_FILE}
			fi
			${TESTED_SORT} -c ${KEYS} -k${f1}.${c1}b,${f2}.${c2} -k${f2}.${c1},${f1}.${c2} ${OUT_DIR}/sik1
			ER=$?
			if ! [ ${ER} -eq 0 ] ; then
			    echo ${LANG} ${KEYS} -k${f1}.${c1}b,${f2}.${c2} -c 2k error >> ${ERRORS_FILE}
			fi
			rm ${OUT_DIR}/res.${f1}.${c1}.${f2}.${c2}
			
		    done
		done
	    done
	done
    done
done

if [ -f ${ERRORS_FILE} ] ; then
    echo TEST FAILED
else
    echo TEST SUCCEEDED
fi
