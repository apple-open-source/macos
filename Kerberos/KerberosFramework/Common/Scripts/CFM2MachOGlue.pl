#!/usr/bin/perl

$libName = shift;
$sourceFile = shift;
$assemblyFile = shift;
$exportFile = shift;
$loadLibrary = shift;

# read in the entire list of symbols
foreach $arg (@ARGV) {
    local $/;
    undef $/; # Ignore end-of-line delimiters in the file

    open (ARG, "$arg") or die ("Can't open $arg for reading.");
    $file .= <ARG>;
    close (ARG);
}

foreach $symbol (split (/\n|\r/, $file)) {
    if ($symbol =~ /^\s*_(.*)\s*$/) { 	   # Ignore blank lines
        push @symbols, $1;
    }
}

# Now build the various files
# The export file:
open (EXPORT, ">$exportFile") or die ("Can't open $exportFile for writing.");
select (EXPORT);

print ("___${libName}_LoadKerberosFramework\n");
foreach $symbol (@symbols) {
    print ("_${symbol}\n");
}
close (EXPORT);

# The assembly file
open (ASSEMBLER, ">$assemblyFile") or die ("Can't open $assemblyFile for writing.");
select (ASSEMBLER);

print ("; Data Section: storage for function pointers\n");
print ("    .data\n\n");
foreach $symbol (@symbols) {
    print <<SYMBOLPTR
    .globl _${symbol}_ProcPtr
_${symbol}_ProcPtr:
    .long 0
    
SYMBOLPTR
}

print ("\n; Text Section: code for the exported functions\n");
print ("    .text\n\n");
foreach $symbol (@symbols) {
    print <<SYMBOLCODE;
    .globl _${symbol}
_${symbol}:
    mflr r0
    bl L0\$${symbol}
L0\$${symbol}:
    mflr r11
    mtlr r0
    addis r11,r11,ha16(_${symbol}_ProcPtr - L0\$${symbol})
    lwz r12,lo16(_${symbol}_ProcPtr - L0\$${symbol})(r11)
    mtctr r12
    bctr
    
SYMBOLCODE
}
close (ASSEMBLER);

# The source file:
open (SOURCE, ">$sourceFile") or die ("Can't open $sourceFile for writing.");
select (SOURCE);

# necessary header file
print ("#include <CoreFoundation/CoreFoundation.h>\n\n");

# externs for pointers:
foreach $symbol (@symbols) {
    print ("extern ProcPtr ${symbol}_ProcPtr;\n");
}

# load the Kerberos Framework:
print <<LOADKERBEROSFRAMEWORK;

void __${libName}_LoadKerberosFramework (void);

void __${libName}_LoadKerberosFramework (void)
{
    CFBundleRef         kerberosBundle = NULL;
    CFURLRef            kerberosURL = NULL;
    int                 loaded = 0;
    
    kerberosURL = CFURLCreateWithFileSystemPath (kCFAllocatorDefault,
                        CFSTR("$loadLibrary"),
                        kCFURLPOSIXPathStyle,
                        true);
        
    if (kerberosURL == NULL) {
        exit (1);
    }
    
    kerberosBundle = CFBundleCreate (kCFAllocatorDefault, kerberosURL);
    CFRelease (kerberosURL);
    if (kerberosBundle == NULL) {
        exit (1);
    }

    loaded = CFBundleLoadExecutable (kerberosBundle);
    if (!loaded) {
        CFRelease (kerberosBundle);
        exit (1);
    }
    
LOADKERBEROSFRAMEWORK

# load each symbol:
foreach $symbol (@symbols) {
    print ("    ${symbol}_ProcPtr = (ProcPtr) CFBundleGetFunctionPointerForName (kerberosBundle, CFSTR(\"${symbol}\"));\n\n");
}

print ("}\n");
close( SOURCE);


