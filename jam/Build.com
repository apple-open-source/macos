! Bootstrap build script for Jam
$ cc command.c
$ cc compile.c
$ cc expand.c
$ cc execvms.c
$ cc filevms.c
$ cc glob.c
$ cc hash.c
$ cc headers.c
$ cc jambase.c
$ cc lists.c
$ cc make.c
$ cc make1.c
$ cc newstr.c
$ cc option.c
$ cc parse.c
$ cc pathvms.c
$ cc regexp.c
$ cc rules.c
$ cc scan.c
$ cc search.c
$ cc timestamp.c
$ cc variable.c
$ cc jam.c
$ cc jamgram.c
$ link/exe=jam.exe command.obj, compile.obj, execvms.obj, expand.obj, -
    filevms.obj, glob.obj, hash.obj, headers.obj, lists.obj, make.obj, -
    make1.obj, newstr.obj, option.obj, parse.obj, pathvms.obj, regexp.obj, -
    rules.obj, scan.obj, search.obj, timestamp.obj, variable.obj, jam.obj, -
    jamgram.obj, jambase.obj
