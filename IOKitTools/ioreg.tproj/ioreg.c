// =============================================================================
// Copyright (c) 2000 Apple Computer, Inc.  All rights reserved. 
//
// ioreg.c
//

#include <CoreFoundation/CoreFoundation.h>            // (CFDictionary, ...)
#include <IOKit/IOCFSerialize.h>                      // (IOCFSerialize, ...)
#include <IOKit/IOKitLib.h>                           // (IOMasterPort, ...)
#include <sys/ioctl.h>                                // (TIOCGWINSZ, ...)
#include <term.h>                                     // (tputs, ...)
#include <unistd.h>                                   // (getopt, ...)

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void assertion(int condition, char * message); // (support routine)
static void boldinit();                               // (support routine)
static void boldon();                                 // (support routine)
static void boldoff();                                // (support routine)
static void print(const char * format, ...);          // (support routine)
static void println(const char * format, ...);        // (support routine)

static void CFArrayShow(CFArrayRef object);           // (support routine)
static void CFBooleanShow(CFBooleanRef object);       // (support routine)
static void CFDataShow(CFDataRef object);             // (support routine)
static void CFDictionaryShow(CFDictionaryRef object); // (support routine)
static void CFNumberShow(CFNumberRef object);         // (support routine)
static void CFObjectShow(CFTypeRef object);           // (support routine)
static void CFSetShow(CFSetRef object);               // (support routine)
static void CFStringShow(CFStringRef object);         // (support routine)

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

const UInt32 kIORegFlagShowBold       = (1 << 0);     // (-b option)
const UInt32 kIORegFlagShowProperties = (1 << 1);     // (-l option)
const UInt32 kIORegFlagShowState      = (1 << 2);     // (-s option)

struct options
{
    char * class;                                     // (-c option)
    UInt32 flags;                                     // (see above)
    char * name;                                      // (-n option)
    char * plane;                                     // (-p option)
    UInt32 width;                                     // (-w option)
    Boolean hex;                                      // (-x option)
};

struct context
{
    UInt32 depth;
    UInt64 stackOfBits;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void printinit(struct options opt);            // (support routine)

static void indent( Boolean isNode,
                    UInt32  serviceDepth,
                    UInt64  stackOfBits );

static void scan( io_registry_entry_t service,
                  Boolean             serviceHasMoreSiblings,
                  UInt32              serviceDepth,
                  UInt64              stackOfBits,      // (see indent routine)
                  struct options      options );

static void show( io_registry_entry_t service,
                  UInt32              serviceDepth,
                  UInt64              stackOfBits,
                  struct options      options );

static void showItem( const void * key,
                      const void * value,
                      void *       parameter );

static void usage();

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int main(int argc, char ** argv)
{
    int                 argument  = 0;
    mach_port_t         iokitPort = 0; // (don't release)
    struct options      options;
    io_registry_entry_t service   = 0; // (needs release)
    kern_return_t       status    = KERN_SUCCESS;
    struct winsize      winsize;

    // Initialize our minimal state.

    options.class = 0;
    options.flags = 0;
    options.name  = 0;
    options.plane = kIOServicePlane;
    options.width = 0;
    options.hex   = 0;

    // Obtain the screen width.

    if (ioctl(fileno(stdout), TIOCGWINSZ, &winsize) == 0)
        options.width = winsize.ws_col;
    else if (ioctl(fileno(stdin), TIOCGWINSZ, &winsize) == 0)
        options.width = winsize.ws_col;

    // Obtain the command-line arguments.

    while ( (argument = getopt(argc, argv, ":bc:ln:p:sw:x")) != -1 )
    {
        switch (argument)
        {
            case 'b':
                options.flags |= kIORegFlagShowBold;
                break;
            case 'c':
                options.class = optarg;
                break;
            case 'l':
                options.flags |= kIORegFlagShowProperties;
                break;
            case 'n':
                options.name = optarg;
                break;
            case 'p':
                options.plane = optarg;
                break;
            case 's':
                options.flags |= kIORegFlagShowState;
                break;
            case 'w':
                options.width = atoi(optarg);
                assertion(options.width >= 0, "invalid width");
                break;
	    case 'x':
		options.hex = TRUE;
		break;
            default:
                usage();
                break;
        }
    }    

    // Initialize text output functions.

    printinit(options);

    if (options.flags & kIORegFlagShowBold)  boldinit();

    // Obtain the I/O Kit communication handle.

    status = IOMasterPort(bootstrap_port, &iokitPort);
    assertion(status == KERN_SUCCESS, "can't obtain I/O Kit's master port");

    // Obtain the I/O Kit root service.

    service = IORegistryGetRootEntry(iokitPort);
    assertion(service, "can't obtain I/O Kit's root service");

    // Traverse over all the I/O Kit services.

    scan( /* service                */ service,
          /* serviceHasMoreSiblings */ FALSE,
          /* serviceDepth           */ 0,
          /* stackOfBits            */ 0,
          /* options                */ options );

    // Release resources.

    IOObjectRelease(service); service = 0;

    // Quit.

    exit(0);	
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void scan( io_registry_entry_t service,
                  Boolean             serviceHasMoreSiblings,
                  UInt32              serviceDepth,
                  UInt64              stackOfBits,
                  struct options      options )
{
    io_registry_entry_t child       = 0; // (needs release)
    io_registry_entry_t childUpNext = 0; // (don't release)
    io_iterator_t       children    = 0; // (needs release)
    kern_return_t       status      = KERN_SUCCESS;

    // Obtain the service's children.

    status = IORegistryEntryGetChildIterator(service, options.plane, &children);
    assertion(status == KERN_SUCCESS, "can't obtain children");

    childUpNext = IOIteratorNext(children);

    // Save has-more-siblings state into stackOfBits for this depth.

    if (serviceHasMoreSiblings)
        stackOfBits |=  (1 << serviceDepth);
    else
        stackOfBits &= ~(1 << serviceDepth);

    // Save has-children state into stackOfBits for this depth.

    if (childUpNext)
        stackOfBits |=  (2 << serviceDepth);
    else
        stackOfBits &= ~(2 << serviceDepth);

    // Print out the relevant service information.

    show(service, serviceDepth, stackOfBits, options);

    // Traverse over the children of this service.

    while (childUpNext)
    {
        child       = childUpNext;
        childUpNext = IOIteratorNext(children);

        scan( /* service                */ child,
              /* serviceHasMoreSiblings */ (childUpNext) ? TRUE : FALSE,
              /* serviceDepth           */ serviceDepth + 1,
              /* stackOfBits            */ stackOfBits,
              /* options                */ options );

        IOObjectRelease(child); child = 0;
    }

    IOObjectRelease(children); children = 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void show( io_registry_entry_t service,
                  UInt32              serviceDepth,
                  UInt64              stackOfBits,
                  struct options      options )
{
    io_name_t       class;          // (don't release)
    struct context  context    = { serviceDepth, stackOfBits };
    int             integer    = 0; // (don't release)
    io_name_t       location;       // (don't release)
    io_name_t       name;           // (don't release)
    CFDictionaryRef properties = 0; // (needs release)
    kern_return_t   status     = KERN_SUCCESS;

    // Print out the name of the service.

    status = IORegistryEntryGetNameInPlane(service, options.plane, name);
    assertion(status == KERN_SUCCESS, "can't obtain name");

    indent(TRUE, serviceDepth, stackOfBits);

    if (options.flags & kIORegFlagShowBold)  boldon();

    print("%s", name);

    if (options.flags & kIORegFlagShowBold)  boldoff();

    // Print out the location of the service.

    status = IORegistryEntryGetLocationInPlane(service, options.plane, location);
    if (status == KERN_SUCCESS)  print("@%s", location);

    // Print out the class of the service.

    status = IOObjectGetClass(service, class);
    assertion(status == KERN_SUCCESS, "can't obtain class name");

    print("  <class %s", class);

    // Prepare to print out the service's useful debug information.

    if (options.flags & kIORegFlagShowState)
    {
        // Print out the busy state of the service (for IOService objects).

        if (IOObjectConformsTo(service, "IOService"))
        {
            status = IOServiceGetBusyState(service, &integer);
            assertion(status == KERN_SUCCESS, "can't obtain busy state");

            print(", busy %d", integer);
        }

        // Print out the retain count of the service.

        integer = IOObjectGetRetainCount(service);
        assertion(integer >= 0, "can't obtain retain count");

        print(", retain count %d", integer);
    }

    println(">");

    // Prepare to print out the service's properties.

    if (options.class && IOObjectConformsTo(service, options.class))
        options.flags |= kIORegFlagShowProperties;

    if (options.name && !strcmp(name, options.name))
        options.flags |= kIORegFlagShowProperties;

    if (options.flags & kIORegFlagShowProperties)
    {
        indent(FALSE, serviceDepth, stackOfBits);
        println("{");

        // Obtain the service's properties.

        status = IORegistryEntryCreateCFProperties(service,
                                                   &properties,
                                                   kCFAllocatorDefault,
                                                   kNilOptions);
        assertion(status == KERN_SUCCESS, "can't obtain properties");
        assertion(CFGetTypeID(properties) == CFDictionaryGetTypeID(), NULL);

        // Print out the service's properties.

        CFDictionaryApplyFunction(properties, showItem, &context);

        indent(FALSE, serviceDepth, stackOfBits);
        println("}");
        indent(FALSE, serviceDepth, stackOfBits);
        println("");

        // Release resources.

        CFRelease(properties);
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void showItem(const void * key, const void * value, void * parameter)
{
    struct context * context = parameter; // (don't release)

    // Print out one of the service's properties.

    indent(FALSE, context->depth, context->stackOfBits);
    print("  ");
    CFStringShow(key);
    print(" = ");
    CFObjectShow(value);
    println("");
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void indent(Boolean isNode, UInt32 depth, UInt64 stackOfBits)
{
    // stackOfBits representation, given current zero-based depth is n:
    //   bit n+1             = does depth n have children?       1=yes, 0=no
    //   bit [n, .. i .., 0] = does depth i have more siblings?  1=yes, 0=no

    UInt32 index;

    if (isNode)
    {
        for (index = 0; index < depth; index++)
            print( (stackOfBits & (1 << index)) ? "| " : "  " );

        print("+-o ");
    }
    else // if (!isNode)
    {
        for (index = 0; index <= depth + 1; index++)
            print( (stackOfBits & (1 << index)) ? "| " : "  " );
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void usage()
{
    fprintf( stderr,
     "usage: ioreg [-b] [-c class | -l | -n name] [-p plane] [-s] [-w width] [-x]\n"
     "where options are:\n"
     "\t-b show object name in bold\n"
     "\t-c list properties of objects with the given class\n"
     "\t-l list properties of all objects\n"
     "\t-n list properties of objects with the given name\n"
     "\t-p traverse registry over the given plane (IOService is default)\n"
     "\t-s show object state (eg. busy state, retain count)\n"
     "\t-w clip output to the given line width (0 is unlimited)\n"
     "\t-x print numeric property values in hexadecimal\n"
     );
    exit(1);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void assertion(int condition, char * message)
{
    if (condition == 0)
    {
        fprintf(stderr, "ioreg: error: %s.\n", message);
        exit(1);
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static char * termcapstr_boldon  = 0;
static char * termcapstr_boldoff = 0;

static int termcapstr_outc(int c)
{
    return putchar(c);
}

static void boldinit()
{
    char *      term;
    static char termcapbuf[64];
    char *      termcapbufptr = termcapbuf;

    term = getenv("TERM");

    if (term)
    {
        if (tgetent(NULL, term) > 0)
        {
            termcapstr_boldon  = tgetstr("md", &termcapbufptr);
            termcapstr_boldoff = tgetstr("me", &termcapbufptr);

            assertion(termcapbufptr - termcapbuf <= sizeof(termcapbuf), NULL);
        }
    }

    if (termcapstr_boldon  == 0)  termcapstr_boldon  = "";
    if (termcapstr_boldoff == 0)  termcapstr_boldoff = "";
}

static void boldon()
{
    tputs(termcapstr_boldon, 1, termcapstr_outc);
}

static void boldoff()
{
    tputs(termcapstr_boldoff, 1, termcapstr_outc);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static char * printbuf     = 0;
static int    printbufclip = FALSE;
static int    printbufleft = 0;
static int    printbufsize = 0;
static Boolean printhex = FALSE;

static void printinit(struct options opt)
{
    if (opt.width)
    {
        printbuf     = malloc(opt.width);
        printbufleft = opt.width;
        printbufsize = opt.width;

        assertion(printbuf != NULL, "can't allocate buffer");
    }
    printhex = opt.hex;
}

static void printva(const char * format, va_list arguments)
{
    if (printbufsize)
    {
        char * c;
        int    count = vsnprintf(printbuf, printbufleft, format, arguments);

        while ( (c = strchr(printbuf, '\n')) )  *c = ' ';    // (strip newlines)

        printf("%s", printbuf);

        if (count >= printbufleft)
        {
            count = printbufleft - 1;
            printbufclip = TRUE;
        }

        printbufleft -= count;   // (printbufleft never hits zero, stops at one)
    }
    else
    {
        vprintf(format, arguments);
    }
}

static void print(const char * format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    printva(format, arguments);
    va_end(arguments);
}

static void println(const char * format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    printva(format, arguments);
    va_end(arguments);

    if (printbufclip)  printf("$");

    printf("\n");

    printbufclip = FALSE;
    printbufleft = printbufsize;
}    

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void CFArrayShow_Applier(const void * value, void * parameter)
{
    Boolean * first = (Boolean *) parameter;

    if (*first)
        *first = FALSE;
    else
        print(",");

    CFObjectShow(value);
}

static void CFArrayShow(CFArrayRef object)
{
    Boolean first = TRUE;
    CFRange range = { 0, CFArrayGetCount(object) };

    print("(");
    CFArrayApplyFunction(object, range, CFArrayShow_Applier, &first);
    print(")");
}

static void CFBooleanShow(CFBooleanRef object)
{
    print(CFBooleanGetValue(object) ? "Yes" : "No");
}

static void CFDataShow(CFDataRef object)
{
    UInt32        asciiNormalCount = 0;
    UInt32        asciiSymbolCount = 0;
    const UInt8 * bytes;
    CFIndex       index;
    CFIndex       length;

    print("<");
    length = CFDataGetLength(object);
    bytes  = CFDataGetBytePtr(object);

    //
    // This algorithm detects ascii strings, or a set of ascii strings, inside a
    // stream of bytes.  The string, or last string if in a set, needn't be null
    // terminated.  High-order symbol characters are accepted, unless they occur
    // too often (80% of characters must be normal).  Zero padding at the end of
    // the string(s) is valid.  If the data stream is only one byte, it is never
    // considered to be a string.
    //

    for (index = 0; index < length; index++)  // (scan for ascii string/strings)
    {
        if (bytes[index] == 0)       // (detected null in place of a new string,
        {                            //  ensure remainder of the string is null)
            for (; index < length && bytes[index] == 0; index++) { }

            break;          // (either end of data or a non-null byte in stream)
        }
        else                         // (scan along this potential ascii string)
        {
            for (; index < length; index++)
            {
                if (isprint(bytes[index]))
                    asciiNormalCount++;
                else if (bytes[index] >= 128 && bytes[index] <= 254)
                    asciiSymbolCount++;
                else
                    break;
            }

            if (index < length && bytes[index] == 0)          // (end of string)
                continue;
            else             // (either end of data or an unprintable character)
                break;
        }
    }

    if ((asciiNormalCount >> 2) < asciiSymbolCount)    // (is 80% normal ascii?)
        index = 0;
    else if (length == 1)                                 // (is just one byte?)
        index = 0;

    if (index >= length && asciiNormalCount) // (is a string or set of strings?)
    {
        Boolean quoted = FALSE;

        for (index = 0; index < length; index++)
        {
            if (bytes[index])
            {
                if (quoted == FALSE)
                {
                    quoted = TRUE;
                    if (index)
                        print(",\"");
                    else
                        print("\"");
                }
                print("%c", bytes[index]);
            }
            else
            {
                if (quoted == TRUE)
                {
                    quoted = FALSE;
                    print("\"");
                }
                else
                    break;
            }
        }
        if (quoted == TRUE)
            print("\"");
    }
    else                                  // (is not a string or set of strings)
    {
        for (index = 0; index < length; index++)  print("%02x", bytes[index]);
    }

    print(">");
}

static void CFDictionaryShow_Applier( const void * key,
                                      const void * value,
                                      void *       parameter )
{
    Boolean * first = (Boolean *) parameter;

    if (*first)
        *first = FALSE;
    else
        print(",");

    CFObjectShow(key);
    print("=");
    CFObjectShow(value);
}

static void CFDictionaryShow(CFDictionaryRef object)
{
    Boolean first = TRUE;

    print("{");
    CFDictionaryApplyFunction(object, CFDictionaryShow_Applier, &first);
    print("}");
}

static void CFNumberShow(CFNumberRef object)
{
    long long number;

    if (CFNumberGetValue(object, kCFNumberLongLongType, &number))
    {
        if (printhex) {
            print("0x%qx", number); 
        } else {
            print("%qu", number); 
        }
    }
}

static void CFObjectShow(CFTypeRef object)
{
    CFTypeID type = CFGetTypeID(object);

    if      ( type == CFArrayGetTypeID()      )  CFArrayShow(object);
    else if ( type == CFBooleanGetTypeID()    )  CFBooleanShow(object);
    else if ( type == CFDataGetTypeID()       )  CFDataShow(object);
    else if ( type == CFDictionaryGetTypeID() )  CFDictionaryShow(object);
    else if ( type == CFNumberGetTypeID()     )  CFNumberShow(object);
    else if ( type == CFSetGetTypeID()        )  CFSetShow(object);
    else if ( type == CFStringGetTypeID()     )  CFStringShow(object);
    else print("<unknown object>");
}

static void CFSetShow_Applier(const void * value, void * parameter)
{
    Boolean * first = (Boolean *) parameter;

    if (*first)
        *first = FALSE;
    else
        print(",");

    CFObjectShow(value);
}

static void CFSetShow(CFSetRef object)
{
    Boolean first = TRUE;
    print("[");
    CFSetApplyFunction(object, CFSetShow_Applier, &first);
    print("]");
}

static void CFStringShow(CFStringRef object)
{
    const char * c = CFStringGetCStringPtr(object, kCFStringEncodingMacRoman);

    if (c)
        print("\"%s\"", c);
    else
    {
        CFIndex bufferSize = CFStringGetLength(object) + 1;
        char *  buffer     = malloc(bufferSize);

        if (buffer)
        {
            if ( CFStringGetCString(
                    /* string     */ object,
                    /* buffer     */ buffer,
                    /* bufferSize */ bufferSize,
                    /* encoding   */ kCFStringEncodingMacRoman ) )
                print("\"%s\"", buffer);

            free(buffer);
        }
    }
}
