#!/usr/bin/perl
#
# generator.pl - auto-generate code for the CSSM plugin interfaces
#
# Usage:
#	perl generator.pl input-directory h-output-dir c-output-dir
#
# Perry The Cynic, Fall 1999.
#
@API_H=("cssmapi.h");
%SPI_H=("AC" => "cssmaci.h", "CSP" => "cssmcspi.h", "DL" => "cssmdli.h",
        "CL" => "cssmcli.h", "TP"  => "cssmtpi.h");
        
$SOURCEDIR=$ARGV[0];			# where all the input files are
$HTARGETDIR=$ARGV[1];			# where the generated headers go
$CTARGETDIR=$ARGV[2];			# where the generated sources go

(${D}) = $HTARGETDIR =~ m@([/:])@;		# guess directory delimiter
sub macintosh() { return ${D} eq ':'; }

# XXX The configuration file should be passed in as a command line argument
if( macintosh() ) {
$APICFG=":::cdsa:cdsa_pluginlib:generator.cfg";		# configuration file
}
 else{
	$APICFG="generator.cfg";		# configuration file 
 }

$tabs = "\t\t\t";	# argument indentation (noncritical)
$warning = "This file was automatically generated. Do not edit on penalty of futility!";


#
# Open and read the configuration file
#
$/=undef;	# gulp file
open(APICFG, $APICFG) or die "Cannot open $APICFG: $^E";
$_=<APICFG>;
close(APICFG);
tr/\012/\015/ if macintosh;
%optionals = /^\s*optional\s+(\w+)\s+(.*)$/gm;


#
# Pre-arranged arrays for processing below
#
%noDataReturnError = ( CL => "CSSMERR_CL_NO_FIELD_VALUES",
					   DL => "CSSMERR_DL_ENDOFDATA" );


#
# process one SPI at a time
#
while (($type, $header) = each %SPI_H) {
  my(%functions, %methods, %actuals);
  ($typelower = $type) =~ tr/A-Z/a-z/;	# lowercase version of type

  # start in on the $type header file
  open(SPI, "$SOURCEDIR${D}$header") or die "cannot open $SOURCEDIR${D}$header: $^E";
  $/=undef;		# big gulp mode
  $_ = <SPI>;	# aaaaah...
  close(SPI);	# done
  tr/\012/\015/ if macintosh;
  # throw away leading and trailing crud (only interested in SPI structure)
  s/^.*struct cssm_spi.*{(.*)} CSSM_SPI.*$/$1/s
    or die "bad format in $SPI_H{$name}";

  # break up into functions (you'd do that HOW in YOUR language? :-)
  @functions = /CSSM_RETURN \(CSSM${type}I \*([A-Za-z_]+)\)\s+\(([^)]+)\);/g;
  %functions = @functions;

  $MOREHEADERS="";
  $MOREHEADERS .= "#include <Security/context.h>\n" if /CSSM_CONTEXT/;
  $MOREHEADERS .= "#include <Security/cssmacl.h>\n" if /CSSM_(ACL|ACCESS)/;

  # break function arguments into many forms:
  #  functions => formal SPI arguments
  #  methods => formal C++ method arguments
  #  actuals => actual expression forms for transition layer use
  # and (by the way) massage them into a more palatable form...
  $nFunctions = 0;
  while (($function, $_) = each %functions) {
    #
    # Turn CSSM SPI formal into method formal
	#
	$returntype{$function} = "void";
    $prefix{$function} = "";
	$postfix{$function} = ";";
    # reshape initial argument (the module handle, more or less)
    s/^CSSM_${type}_HANDLE ${type}Handle(,\s*\n\s*|$)//s; # remove own handle (-> this)
    s/^CSSM_DL_DB_HANDLE DLDBHandle/CSSM_DB_HANDLE DBHandle/s; # DL_DB handle -> DB handle
    s/CSSM_HANDLE_PTR ResultsHandle(,?)\n//m	   	# turn ptr-to-resultshandle into fn result
      and do {
		$returntype{$function} = "CSSM_HANDLE";
		$prefix{$function} = "if ((Required(ResultsHandle) = ";
		$postfix{$function} = ") == CSSM_INVALID_HANDLE)\n    return $noDataReturnError{$type};";
	  };
	if ($function =~ /GetNext/) {					# *GetNext* returns a bool
	  $returntype{$function} = "bool";
	  $prefix{$function} = "if (!";
	  $postfix{$function} = ")\n    return $noDataReturnError{$type};";
	}
    # reshape subsequent arguments
	s/([su]int32) \*(\w+,?)/$1 \&$2/gm;				# int * -> int & (output integer)
    s/(CSSM_\w+_PTR) \*(\w+,?)/$1 \&$2/gm;			# _PTR * -> _PTR &
	s/(CSSM_\w+)_PTR (\w+)/$1 \*$2/gm;				# XYZ_PTR -> XYZ * (explicit)
	s/(const )?CSSM_DATA \*(\w+)Bufs/$1CssmData $2Bufs\[\]/gm; # c DATA *Bufs (plural)
    s/(const )?CSSM_(DATA|OID) \*/$1CssmData \&/gm;	# c DATA * -> c Data &
	s/(const )?CSSM_FIELD \*(\w+)Fields/$1CSSM_FIELD $2Fields\[\]/gm; # c FIELD *Fields (plural)
	s/(const )?CSSM_FIELD \*CrlTemplate/$1CSSM_FIELD CrlTemplate\[\]/gm; # c FIELD *CrlTemplate
	s/const CSSM_CONTEXT \*/const Context \&/gm;	# c CSSM_CONTEXT * -> c Context &
	s/(const )?CSSM_ACCESS_CREDENTIALS \*/$1AccessCredentials \&/gm; # ditto
	s/(const )?CSSM_QUERY_SIZE_DATA \*/$1QuerySizeData \&/gm; # ditto
	s/(const )?CSSM_CSP_OPERATIONAL_STATISTICS \*/$1CSPOperationalStatistics \&/gm; # ditto
    s/(const )?CSSM_(WRAP_)?KEY \*/$1CssmKey \&/gm;	# CSSM[WRAP]KEY * -> CssmKey &
    s/const CSSM_QUERY \*/const DLQuery \&/gm;		# c QUERY * -> c Query &
	s/(const )?(CSSM_[A-Z_]+) \*/$1$2 \&/gm;		# c CSSM_ANY * -> c CSSM_ANY &
    $methods{$function} = $_;

	#
    # Now turn the method formal into the transition invocation actuals
	#
    s/^CSSM_DB_HANDLE \w+(,?)/DLDBHandle.DBHandle$1/s;		# matching change to DL_DB handles
	s/(const )?([A-Z][a-z]\w+) &(\w+)(,?)/$2::required($3)$4/gm; # BIG_ * -> Small_ &
	s/(const )?CssmData (\w+)Bufs\[\](,?)/\&\&CssmData::required($2Bufs)$3/gm; # c DATA *DataBufs
	s/(const )?CSSM_FIELD (\w+)Fields\[\](,?)/$2Fields$3/gm; # c CSSM_FIELD *Fields
	s/(const )?CSSM_FIELD CrlTemplate\[\](,?)/CrlTemplate$2/gm; # c CSSM_FIELD *CrlTemplate
    # now remove formal arguments and clean up
	s/^.* \&\&(\w+,?)/$tabs\&$1/gm;					# && escape (to keep real &)
	s/^.* \&(\w+)(,?)/${tabs}Required($1)$2/gm;		# dereference for ref transition
    s/^.* \**(\w+,?)/$tabs$1/gm;					# otherwise, plain actual argument
    s/^$tabs//;
    $actuals{$function} = $_;

	#
	# Fix optional arguments
	#
	foreach $opt (split " ", $optionals{$function}) {
	  $methods{$function} =~ s/\&$opt\b/\*$opt/;	# turn refs back into pointers
	  $actuals{$function} =~ s/::required\($opt\)/::optional($opt)/; # optional specific
	  $actuals{$function} =~ s/Required\($opt\)/$opt/; # optional generic
	};
    $nFunctions++;
  };

  #
  # Prepare to write header and source files
  #
  open(H, ">$HTARGETDIR${D}${type}abstractsession.h") or die "cannot write ${type}abstractsession.h: $^E";
  open(C, ">$CTARGETDIR${D}${type}abstractsession.cpp") or die "cannot write ${type}abstractsession.cpp: $^E";

  #
  # Create header file
  #
  print H <<HDRHEAD;
//
// $type plugin transition layer.
// $warning
//
#ifndef _H_${type}ABSTRACTSESSION
#define _H_${type}ABSTRACTSESSION

#include <Security/pluginsession.h>
$MOREHEADERS
#if defined(_CPP_${type}ABSTRACTSESSION)
# pragma export on
#endif

namespace Security
{

//
// A pure abstract class to define the ${type} module interface
//
class ${type}AbstractPluginSession {
public:
HDRHEAD

  $functionCount = 0;
  while (($function, $arglist) = each %methods) {
    # generate method declaration
    print H "  virtual $returntype{$function} $function($arglist) = 0;\n";
    $functionCount++;
  };
  print H <<HDREND;
};

} // end namespace Security

#if defined(_CPP_${type}ABSTRACTSESSION)
# pragma export off
#endif

#endif //_H_${type}ABSTRACTSESSION
HDREND

  #
  # Create source file
  #
  print C <<BODY;
//
// $type plugin transition layer.
// $warning
//
#if defined(__MWERKS__)
# define _CPP_${type}ABSTRACTSESSION
# define _CPP_${type}SESSION
#endif
#include <Security/${type}session.h>
#include <Security/cssmplugin.h>
#include <Security/cssm${typelower}i.h>

BODY

  # write transition layer functions
  while (($function, $arglist) = each %functions) {
    $lookupHandle = "${type}Handle";
	$lookupHandle = "DLDBHandle.DLHandle" if $arglist =~ /DL_DB_HANDLE/;
    print C <<SHIM;
static CSSM_RETURN CSSM${type}I cssm_$function($arglist)
{
  BEGIN_API
  ${prefix{$function}}findSession<${type}PluginSession>($lookupHandle).$function($actuals{$function})${postfix{$function}}
  END_API($type)
}

SHIM
  };

  # generate dispatch table - in the right order, please
  print C "\nstatic const CSSM_SPI_${type}_FUNCS ${type}FunctionStruct = {\n";
  while ($function = shift @functions) {
    print C "  cssm_$function,\n";
    shift @functions;	# skip over arglist part
  };
  print C "};\n\n";

  print C <<END;
static CSSM_MODULE_FUNCS ${type}FunctionTable = {
  CSSM_SERVICE_$type,	// service type
  $functionCount,	// number of functions
  (const CSSM_PROC_ADDR *)&${type}FunctionStruct
};

CSSM_MODULE_FUNCS_PTR ${type}PluginSession::construct()
{
   return &${type}FunctionTable;
}
END

  #
  # Done with this type
  #
  close(H);
  close(C);
  
  print "$nFunctions functions generated for $type SPI transition layer.\n";
};
