# FORTRAN version
#
# Scans the source text for a manual page and locates "_names" macro
# invocations.  Looks up the names in gl.h (or glu.h or glx.h) and generates
# definitions for the OpenGL m4 man page macros (_cmnd, _param1, etc.)
# Extracts machine dependencies for the names and stores them in a file
# for later inclusion in the man page.  (See glman shell script.)
#
# basename, prefix, machdeps, and machdepsout are set on the command line.
# The regexp below is replaced by an expression to do case insensitive
# searching on the name of a GL function before running this program.
#
BEGIN {
  FS = " |\t|\\("
  basename = substr(basename,2,length(basename)-1)  # strip leading 'f'
  prefix = "f" prefix
  found = 0
  numargs = 0
  newargs[0] = ""
  stderr = "cat 1>&2"
  vee = 0
  getnames("f" basename)

  for (j=0; j<numNames; j++)
  {
    rexpNames[j] = names[j] regExps[j]
    head[j] = ""
    numFound[j] = 0
    nargs[0] = 0
  }
}

/extern/ {
  cmdfield = 0;
  # search for the command string
  for (i = 3; i <= NF; i++)
  {
    name = $i;
    if (index(name,"gl") == 1)
    {
      cmdfield = i;
      # strip the gl, glu, or glX prefix
      if (index(name,"glu") == 1 || index(name,"glX") == 1)
	name = substr(name,4);
      else
	name = substr(name,3);
      truename = name;
      break;
    }
  }
  if (cmdfield != 0) {
    for (i=0; i<numNames; i++)
    {
      if (name ~ rexpNames[i])
      {  # Found one - $cmdfield is the name, $2 through $(cmdfield-1) is
         # the return type.  The rest is the argument list.
         match(name,names[i])
         if ((RLENGTH != length(names[i])) || (match(name,rexpNames[i]) != 1))
           continue
         if (RLENGTH != length(name))
           continue
         cname = name
         names[i,numFound[i]] = truename
         p = index($0,"(")
         addargs(substr($0,p,length($0)-p-1),i)   # Strips trailing semicolon
         typestr = $2
         for (j = 3; j < cmdfield; j++)
         {
  	 typestr = typestr " " $j
         }
         head[i] = head[i] makeHead(truename,typestr,i)
         numFound[i]++
         found++
       }
    }
  }
}

END {
  if (found == 0) {
    if (basename == "glxintro") {
      numNames = 1;
      numFound[0] = 1;
      names[0,0] = "Intro";
    }
    else if (basename == "intro") {
      numNames = 1;
      numFound[0] = 1;
      names[0,0] = "Intro";
    }
    else {
      printf "No GL call found that matches '%s'.\n", basename | stderr
      printf "Edit macros/mkhead.awk to add a special name.\n" | stderr
    }
  }
  printf "_define(_samething,@<.PP\n"
  printf "The above subroutines are functionally equivalent;\n"
  printf "they differ only in the specification of their parameters.\n"
  printf ">@)dnl\n"

  printf "_define(_header,@<dnl\n"
  printf "_setup()dnl\n"
  printf "_define(_cname,$1)dnl\n"
  printf ".TH %s$1 \n", prefix
  printf ".SH NAME\n"
  printf ".B \""
  for (i=0; i<numNames; i++)
    for (j=0; j<numFound[i]; j++)
      if ((i == (numNames-1)) && (j == (numFound[i]-1)))
        printf "%s%s\n", prefix, names[i,j]
      else
        printf "%s%s, ", prefix, names[i,j]
  printf "\\- $2\n"
  printf ">@)dnl\n"

  printf "_define(_names,@<dnl\n"
  printf "_ifelse($3,@<>@,.SH FORTRAN SPECIFICATION\n)"
  printf "_ifelse("
  for (i=0; i<numNames; i++)
  {
    printf "_namenum,@<%d>@,@<%s>@,dnl\n",i,head[i]
  }
  printf "ERROR)dnl\n"
  printf "_define(@<_namenum>@,_incr(_namenum))>@)dnl\n"

# Define the _machdeps macro to include the machine dependencies
# extracted earlier:

  printf "_define(@<_machdeps>@,@<_sinclude(@<%s>@)>@)dnl\n", machdepsout

#  if (found > 1)
#  {
#    printf "_samething()\n"
#  }
  maxArgs = 0
  for (i=0; i<numNames; i++)
    if (maxArgs < nargs[i])
      maxArgs = nargs[i]
  long = ""
  for (j=1; j<=maxArgs; j++)
  {
    printf "_define(_param%d,@<",j
    printf "_define(@<_tmpnum>@,_ifelse($#,0,_namenum,$1))dnl\n"
    printf "_ifelse("
    for (i=0; i<numNames; i++)
    {
      printf "_tmpnum,@<%d>@,\\f2%s\\fP,dnl\n",i+1,args[j,i]
      if (length(args[j,i]) > length(long))
        long = args[j,i]
    }
    printf "???)>@)dnl\n"
  }
# next loop same as above, except does not italicize
# resulting definitions can be used in equations
  for (j=1; j<=maxArgs; j++)
  {
    printf "_define(_eqnparam%d,@<",j
    printf "_define(@<_tmpnum>@,_ifelse($#,0,_namenum,$1))dnl\n"
    printf "_ifelse("
    for (i=0; i<numNames; i++)
    {
      printf "_tmpnum,@<%d>@,\"%s\",dnl\n",i+1,args[j,i]
      if (length(args[j,i]) > length(long))
        long = args[j,i]
    }
    printf "???)>@)dnl\n"
  }
# The following stuff is designed to find the longest argument so that
# the '.TP' indentation can be set in the first instaciation of _phead
# (thus the use of the '_first' macro as a flag).  Things are complicated
# by the possibility of multiple arguments in the call to _phead.
# This is what _makelist is for: to turn a space separted multiple
# argument list into a comma separated one (commas can't be used in
# the original list because they have special meaning to m4).
# Unfortunately, this means that (currently) if the longest string is
# a multiple argument, the indentation will only be right if it occurs
# in the first _phead.  This is because only the API file is scanned
# for arguments, and not the man page file, so this script can't know
# which instance of phead has the longest (multiple) argument.

  long = "\\fI" long "\\fP"
  printf "_define(_phead,@<dnl\n"
  printf "_ifdef(@<_first>@,@<.TP>@,@<.TP \\w'"
  printf "_ifelse(_eval(_len(%s)>_len(_makelist($1))),1,",long
  printf "%s,translit(_makelist($1),@<+>@,@<\\>@))\\ \\ 'u dnl\n", long
  printf "_define(_first,first)>@)\n"
  printf "translit(_makelist($1),@<+>@,@<,>@)>@)dnl\n"
  printf "_define(_cmnd,@<_ifelse($1,@<>@,\\f3" prefix "@<>@_cname\\fP,dnl\n"
  printf "\\f3" prefix "$1\\fP)>@)dnl\n"
  printf "_define(_glcmnd,@<_ifelse($1,@<>@,\\f3gl@<>@_cname\\fP,dnl\n"
  printf "\\f3gl$1\\fP)>@)dnl\n"
  printf "_define(_glucmnd,@<_ifelse($1,@<>@,\\f3glu@<>@_cname\\fP,dnl\n"
  printf "\\f3glu$1\\fP)>@)dnl\n"
  printf "_define(_xcmnd,@<_ifelse($1,@<>@,\\f3X@<>@_cname\\fP,dnl\n"
  printf "\\f3X$1\\fP)>@)dnl\n"
#  printf "syscmd(@<${maCdIr}/mkname.awk>@ ${maCdIr}/pglspec >_tmpnam)dnl\n"
#  printf "_include(_tmpnam)syscmd(rm -f _tmpnam)>@))dnl\n"
}



#
# function to make the troff to typeset the function header
#
function makeHead(fname,type,i)
{
  fname = prefix fname
  headString = sprintf("%s \\f3%s\\fP(",fortranize(type,0),fname)
  if (numargs > 0)
  {
    headString = headString "\n"
    if (numargs > 1)
    {
      headString = headString sprintf("%s,\n",fortranize(targs[1,i],1))
      headString = headString ".nf\n"
      headString = headString sprintf(".ta \\w'\\f3%s \\fP%s( 'u\n",fortranize(type,0),fname)
      for (j=2; j<=numargs-1; j++)
        headString = headString sprintf("\t%s,\n",fortranize(targs[j,i],1))
    headString = headString sprintf("\t%s )\n",fortranize(targs[numargs,i],1))
    headString = headString ".fi\n"
    }
    else
      headString = headString sprintf("%s )\n.nf\n.fi\n",fortranize(targs[numargs,i],1))
  }
  else
    headString = headString " )\n.nf\n.fi\n"
  return headString
}


# Convert a gl C type to the FORTRAN equivalent
# If flag = 1, then convert a function argument;
#   else, convert a function return type.

function fortranize(typedArg,flag)
{
  item = typedArg;
  if (flag == 0) { # return type
    sub(/void/,"SUBROUTINE",item)
  }
  # function argument or return type
  if (item ~ /\*/) {
    sub(/\*/,"",item);
    sub(/const /,"",item)
    if (item ~ /GLubyte/)
      sub(/GLubyte/,"CHARACTER*256",item)
    else
      sub(/GL[^ ]*/,"CHARACTER*8",item)
  }
  else {
    sub(/GLu*byte/,"INTEGER*1",item)
    sub(/GLu*short/,"INTEGER*2",item)
    sub(/GLu*int/,"INTEGER*4",item)
    sub(/GLsizei/,"INTEGER*4",item)
    sub(/GLfloat/,"REAL*4",item)
    sub(/GLclampf/,"REAL*4",item)
    sub(/GLdouble/,"REAL*8",item)
    sub(/GLclampd/,"REAL*4",item)
    sub(/GLboolean/,"LOGICAL*1",item)
    sub(/GLenum/,"INTEGER*4",item)
    sub(/GLbitfield/,"INTEGER*4",item)
  }
  return item
}


# Generates 'args' of untyped argument names
# Also generates 'targs' of typed argument names and 'nargs', the number
# of arguments.

function addargs(arglist,i)
{
#
# First strip leading '(' and trailing ')'
#
  if (substr(arglist,1,1) == "(")
    arglist = substr(arglist,2,length(arglist))
  while (substr(arglist,1,1) == " ")
    arglist = substr(arglist,2,length(arglist))

  if (substr(arglist,length(arglist),1) == ")")
    arglist = substr(arglist,1,length(arglist)-1)
  while (substr(arglist,length(arglist),1) == " ")
    arglist = substr(arglist,1,length(arglist)-1)

  numargs = split(arglist,newargs, ",[ \t]")
  if (newargs[1] == "void")
  {
    numargs = 0;
    targs[1,i] = newargs[1];
    args[1,i] = newargs[1];
  }
  if (nargs[i] < numargs)
    nargs[i] = numargs;

  for (j=1; j<=numargs; j++)
  {
# targs[j,i] italicizes the argument but not the type
    targs[j,i] = newargs[j]
    numWords = split(targs[j,i],words,"[ \t]")
    args[j,i] = words[numWords]
    targs[j,i] = words[1]
    for (k=2; k<=numWords-1; k++)
      targs[j,i] = targs[j,i] " " words[k]
    targs[j,i] = targs[j,i] " \\fI" words[numWords] "\\fP"
    sub(/\[.*\]/,"",args[j,i])
    gsub("[*()]","",args[j,i])
  }
}

#
# Parse and save away the _names(name,regexp) declarations in the file
# for later use in matching the entries in the API file.
#

function getnames(file)
{
  numNames = 0
  fname = file ".gl"
  while (getline < fname)
  {
    if (index($0,"_names(") != 0)
    {
      start = index($0,"_names(") + 7
      stuff = substr($0,start,length($0)-start)
      split(stuff,things,",")
      names[numNames] = things[1]
      regExps[numNames] = things[2]
      numNames++
    }      
  }
  close(fname)
  if (numNames == 0)
  {
    names[0] = file
    regExps[0] = "[1-9]*u*[lbsifd]*v*"
    numNames++
  }
}
