" Vim syntax file
" Language:	SAS
" Maintainer:	James Kidd <james.kidd@covance.com>
" Last Change:	26 Sep 2001
"		Added keywords for use in SAS SQL procedure and hilighting for
"		SAS base procedures, added logic to distinqush between versions
"		for SAS macro variable hi-lighting (Thanks to user Ronald
"		Höllwarth for pointing out bug)
"  For version 5.x: Clear all syntax items
"  For version 6.x: Quit when a syntax file was already loaded
if version < 600
   syntax clear
elseif exists("b:current_syntax")
   finish
endif

syn case ignore

syn region sasString  start=+"+  skip=+\\\\\|\\"+  end=+"+
syn region sasString  start=+'+  skip=+\\\\\|\\"+  end=+'+

"syn region sasCards    start="cards;"  end=";"
"syn region sasCards    start="CARDS;"  end=";"

syn match sasNumber  "-\=\<\d*\.\=[0-9_]\>"

syn region sasComment    start="/\*"  end="\*/"
syn match sasComment  "^\s*\*.*;"

" This line defines macro variables in code.  HiLink at end of file
" defines the color scheme. Begin region with ampersand and end with
" any non-word character offset by -1; put ampersand in the skip list
" just in case it is used to concatenate macro variable values.

" Thanks to ronald höllwarth for this fix to an intra-versioning
" problem with this little feature

if version < 600
   syn region sasMacroVar   start="\&" skip="[_&]" end="\W"he=e-1
else		 " for the older Vim's just do it their way ...
   syn region sasMacroVar   start="&" skip="[_&]" end="\W"he=e-1
endif

syn keyword sasStep	      RUN QUIT
syn match   sasStep	   "^\s*DATA\s"
"syn match   sasProcs	     "^\s*PROC\s"

" Base SAS Procs - version 8.1
syn region  sasProcs	    start="PROC FORMAT" end="\W"
syn region  sasProcs	    start="PROC APPEND" end="\W"
syn region  sasProcs	    start="PROC CALENDAR" end="\W"
syn region  sasProcs	    start="PROC CATALOG" end="\W"
syn region  sasProcs	    start="PROC CHART" end="\W"
syn region  sasProcs	    start="PROC CIMPORT" end="\W"
syn region  sasProcs	    start="PROC COMPARE" end="\W"
syn region  sasProcs	    start="PROC CONTENTS" end="\W"
syn region  sasProcs	    start="PROC COPY" end="\W"
syn region  sasProcs	    start="PROC CORR" end="\W"
syn region  sasProcs	    start="PROC CPORT" end="\W"
syn region  sasProcs	    start="PROC DATASETS" end="\W"
syn region  sasProcs	    start="PROC DBCSTAB" end="\W"
syn region  sasProcs	    start="PROC DISPLAY" end="\W"
syn region  sasProcs	    start="PROC EXPLODE" end="\W"
syn region  sasProcs	    start="PROC EXPORT" end="\W"
syn region  sasProcs	    start="PROC FORMAT" end="\W"
syn region  sasProcs	    start="PROC FORMS" end="\W"
syn region  sasProcs	    start="PROC FREQ" end="\W"
syn region  sasProcs	    start="PROC FSLIST" end="\W"
syn region  sasProcs	    start="PROC IMPORT" end="\W"
syn region  sasProcs	    start="PROC MEANS" end="\W"
syn region  sasProcs	    start="PROC OPTIONS" end="\W"
syn region  sasProcs	    start="PROC PLOT" end="\W"
syn region  sasProcs	    start="PROC PMENU" end="\W"
syn region  sasProcs	    start="PROC PRINT" end="\W"
syn region  sasProcs	    start="PROC PRINTTO" end="\W"
syn region  sasProcs	    start="PROC RANK" end="\W"
syn region  sasProcs	    start="PROC REGISTRY" end="\W"
syn region  sasProcs	    start="PROC REPORT" end="\W"
syn region  sasProcs	    start="PROC SORT" end="\W"
syn region  sasProcs	    start="PROC SQL" end="\W"
syn region  sasProcs	    start="PROC STANDARD" end="\W"
syn region  sasProcs	    start="PROC SUMMARY" end="\W"
syn region  sasProcs	    start="PROC TABULATE" end="\W"
syn region  sasProcs	    start="PROC TIMEPLOT" end="\W"
syn region  sasProcs	    start="PROC TRANSPOSE" end="\W"
syn region  sasProcs	    start="PROC TRANTAB" end="\W"
syn region  sasProcs	    start="PROC UNIVARIATE" end="\W"

syn keyword sasConditional    DO ELSE END IF THEN UNTIL WHILE

syn keyword sasStatement      ABORT ARRAY ATTRIB BY CALL CARDS CARDS4 CATNAME
syn keyword sasStatement      CONTINUE DATALINES DATALINES4 DELETE DISPLAY
syn keyword sasStatement      DM DROP ENDSAS ERROR FILE FILENAME FOOTNOTE
syn keyword sasStatement      FORMAT GOTO INFILE INFORMAT INPUT KEEP
syn keyword sasStatement      LABEL LEAVE LENGTH LIBNAME LINK LIST LOSTCARD
syn keyword sasStatement      MERGE MISSING MODIFY OPTIONS OUTPUT PAGE
syn keyword sasStatement      PUT REDIRECT REMOVE RENAME REPLACE RETAIN
syn keyword sasStatement      RETURN SELECT SET SKIP STARTSAS STOP TITLE
syn keyword sasStatement      UPDATE WAITSAS WHERE WINDOW X

" Keywords that are used in Proc SQL
" I left them as statements because SAS's enhanced editor highlights
" them the same as normal statements used in data steps

syn keyword sasStatement      ADD AND ALTER AS CASCADE CHECK CREATE
syn keyword sasStatement      DELETE DESCRIBE DISTINCT DROP FOREIGN
syn keyword sasStatement      FROM GROUP HAVING INDEX INSERT INTO IN
syn keyword sasStatement      KEY LIKE MESSAGE MODIFY MSGTYPE NOT
syn keyword sasStatement      NULL ON OR ORDER PRIMARY REFERENCES
syn keyword sasStatement      RESET RESTRICT SELECT SET TABLE
syn keyword sasStatement      UNIQUE UPDATE VALIDATE VIEW WHERE


syn match   sasStatement      "FOOTNOTE\d"
syn match   sasStatement      "TITLE\d"

syn match   sasMacro      "%BQUOTE"
syn match   sasMacro      "%NRBQUOTE"
syn match   sasMacro      "%CMPRES"
syn match   sasMacro      "%QCMPRES"
syn match   sasMacro      "%COMPSTOR"
syn match   sasMacro      "%DATATYP"
syn match   sasMacro      "%DISPLAY"
syn match   sasMacro      "%DO"
syn match   sasMacro      "%ELSE"
syn match   sasMacro      "%END"
syn match   sasMacro      "%EVAL"
syn match   sasMacro      "%GLOBAL"
syn match   sasMacro      "%GOTO"
syn match   sasMacro      "%IF"
syn match   sasMacro      "%INDEX"
syn match   sasMacro      "%INPUT"
syn match   sasMacro      "%KEYDEF"
syn match   sasMacro      "%LABEL"
syn match   sasMacro      "%LEFT"
syn match   sasMacro      "%LENGTH"
syn match   sasMacro      "%LET"
syn match   sasMacro      "%LOCAL"
syn match   sasMacro      "%LOWCASE"
syn match   sasMacro      "%MACRO"
syn match   sasMacro      "%MEND"
syn match   sasMacro      "%NRBQUOTE"
syn match   sasMacro      "%NRQUOTE"
syn match   sasMacro      "%NRSTR"
syn match   sasMacro      "%PUT"
syn match   sasMacro      "%QCMPRES"
syn match   sasMacro      "%QLEFT"
syn match   sasMacro      "%QLOWCASE"
syn match   sasMacro      "%QSCAN"
syn match   sasMacro      "%QSUBSTR"
syn match   sasMacro      "%QSYSFUNC"
syn match   sasMacro      "%QTRIM"
syn match   sasMacro      "%QUOTE"
syn match   sasMacro      "%QUPCASE"
syn match   sasMacro      "%SCAN"
syn match   sasMacro      "%STR"
syn match   sasMacro      "%SUBSTR"
syn match   sasMacro      "%SUPERQ"
syn match   sasMacro      "%SYSCALL"
syn match   sasMacro      "%SYSEVALF"
syn match   sasMacro      "%SYSEXEC"
syn match   sasMacro      "%SYSFUNC"
syn match   sasMacro      "%SYSGET"
syn match   sasMacro      "%SYSLPUT"
syn match   sasMacro      "%SYSPROD"
syn match   sasMacro      "%SYSRC"
syn match   sasMacro      "%SYSRPUT"
syn match   sasMacro      "%THEN"
syn match   sasMacro      "%TO"
syn match   sasMacro      "%TRIM"
syn match   sasMacro      "%UNQUOTE"
syn match   sasMacro      "%UNTIL"
syn match   sasMacro      "%UPCASE"
syn match   sasMacro      "%VERIFY"
syn match   sasMacro      "%WHILE"
syn match   sasMacro      "%WINDOW"

" SAS Functions

syn keyword sasFunction ABS ADDR AIRY ARCOS ARSIN ATAN ATTRC ATTRN
syn keyword sasFunction BAND BETAINV BLSHIFT BNOT BOR BRSHIFT BXOR
syn keyword sasFunction BYTE CDF CEIL CEXIST CINV CLOSE CNONCT COLLATE
syn keyword sasFunction COMPBL COMPOUND COMPRESS COS COSH CSS CUROBS
syn keyword sasFunction CV DACCDB DACCDBSL DACCSL DACCSYD DACCTAB
syn keyword sasFunction DAIRY DATE DATEJUL DATEPART DATETIME DAY
syn keyword sasFunction DCLOSE DEPDB DEPDBSL DEPDBSL DEPSL DEPSL
syn keyword sasFunction DEPSYD DEPSYD DEPTAB DEPTAB DEQUOTE DHMS
syn keyword sasFunction DIF DIGAMMA DIM DINFO DNUM DOPEN DOPTNAME
syn keyword sasFunction DOPTNUM DREAD DROPNOTE DSNAME ERF ERFC EXIST
syn keyword sasFunction EXP FAPPEND FCLOSE FCOL FDELETE FETCH FETCHOBS
syn keyword sasFunction FEXIST FGET FILEEXIST FILENAME FILEREF FINFO
syn keyword sasFunction FINV FIPNAME FIPNAMEL FIPSTATE FLOOR FNONCT
syn keyword sasFunction FNOTE FOPEN FOPTNAME FOPTNUM FPOINT FPOS
syn keyword sasFunction FPUT FREAD FREWIND FRLEN FSEP FUZZ FWRITE
syn keyword sasFunction GAMINV GAMMA GETOPTION GETVARC GETVARN HBOUND
syn keyword sasFunction HMS HOSTHELP HOUR IBESSEL INDEX INDEXC
syn keyword sasFunction INDEXW INPUT INPUTC INPUTN INT INTCK INTNX
syn keyword sasFunction INTRR IRR JBESSEL JULDATE KURTOSIS LAG LBOUND
syn keyword sasFunction LEFT LENGTH LGAMMA LIBNAME LIBREF LOG LOG10
syn keyword sasFunction LOG2 LOGPDF LOGPMF LOGSDF LOWCASE MAX MDY
syn keyword sasFunction MEAN MIN MINUTE MOD MONTH MOPEN MORT N
syn keyword sasFunction NETPV NMISS NORMAL NOTE NPV OPEN ORDINAL
syn keyword sasFunction PATHNAME PDF PEEK PEEKC PMF POINT POISSON POKE
syn keyword sasFunction PROBBETA PROBBNML PROBCHI PROBF PROBGAM
syn keyword sasFunction PROBHYPR PROBIT PROBNEGB PROBNORM PROBT PUT
syn keyword sasFunction PUTC PUTN QTR QUOTE RANBIN RANCAU RANEXP
syn keyword sasFunction RANGAM RANGE RANK RANNOR RANPOI RANTBL RANTRI
syn keyword sasFunction RANUNI REPEAT RESOLVE REVERSE REWIND RIGHT
syn keyword sasFunction ROUND SAVING SCAN SDF SECOND SIGN SIN SINH
syn keyword sasFunction SKEWNESS SOUNDEX SPEDIS SQRT STD STDERR STFIPS
syn keyword sasFunction STNAME STNAMEL SUBSTR SUM SYMGET SYSGET SYSMSG
syn keyword sasFunction SYSPROD SYSRC SYSTEM TAN TANH TIME TIMEPART
syn keyword sasFunction TINV TNONCT TODAY TRANSLATE TRANWRD TRIGAMMA
syn keyword sasFunction TRIM TRIMN TRUNC UNIFORM UPCASE USS VAR
syn keyword sasFunction VARFMT VARINFMT VARLABEL VARLEN VARNAME
syn keyword sasFunction VARNUM VARRAY VARRAYX VARTYPE VERIFY VFORMAT
syn keyword sasFunction VFORMATD VFORMATDX VFORMATN VFORMATNX VFORMATW
syn keyword sasFunction VFORMATWX VFORMATX VINARRAY VINARRAYX VINFORMAT
syn keyword sasFunction VINFORMATD VINFORMATDX VINFORMATN VINFORMATNX
syn keyword sasFunction VINFORMATW VINFORMATWX VINFORMATX VLABEL
syn keyword sasFunction VLABELX VLENGTH VLENGTHX VNAME VNAMEX VTYPE
syn keyword sasFunction VTYPEX WEEKDAY YEAR YYQ ZIPFIPS ZIPNAME ZIPNAMEL
syn keyword sasFunction ZIPSTATE

" Handy settings for using vim with log files
syn keyword sasErrMsg	      ERROR
syn keyword sasWarnMsg	      WARNING
syn keyword sasLogMsg	      NOTE

" End of SAS Functions

"  Define the default highlighting.
"  For version 5.7 and earlier: only when not done already
"  For version 5.8 and later: only when an item doesn't have highlighting yet

if version >= 508 || !exists("did_sas_syntax_inits")
   if version < 508
      let did_sas_syntax_inits = 1
      command -nargs=+ HiLink hi link <args>
   else
      command -nargs=+ HiLink hi def link <args>
   endif
" Special hilighting for the SAS proc section
   hi	  sasProcs		term=bold ctermfg=Blue gui=bold guifg=Blue

   HiLink sasComment		Comment
   HiLink sasConditional	Statement
   HiLink sasStep		Statement
   HiLink sasFunction		Function
   HiLink sasMacro		PreProc
   HiLink sasMacroVar		NonText
   HiLink sasNumber		Number
   HiLink sasStatement		Statement
   HiLink sasString		String
   HiLink sasErrMsg		ErrorMsg
   HiLink sasWarnMsg		WarningMsg
   HiLink sasLogMsg		MoreMsg
   HiLink sasCards		MoreMsg
  delcommand HiLink
endif

let b:current_syntax = "sas"

" vim: ts=8
