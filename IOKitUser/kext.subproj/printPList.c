#include "printPList.h"

static void _indent(CFMutableStringRef string, unsigned indentLevel)
{
    unsigned int i;

    for (i = 0; i < indentLevel; i++) {
        CFStringAppendCString(string, " ", kCFStringEncodingMacRoman);
    }
    return;
}

static void _appendCFString(CFMutableStringRef string, CFStringRef aString, Boolean withQuotes)
{
    char * quote = "";

    if (withQuotes) quote = "\"";
    CFStringAppendFormat(string, NULL, CFSTR("%s%@%s"), quote, aString, quote);
    return;
}

static void _appendCFURL(CFMutableStringRef string, CFURLRef anURL)
{
    CFURLRef absURL = NULL;     // must release
    CFStringRef absPath = NULL; // must release

    absURL = CFURLCopyAbsoluteURL(anURL);
    if (!absURL) {
        goto finish;
    }

    absPath = CFURLCopyFileSystemPath(anURL, kCFURLPOSIXPathStyle);
    if (!absPath) {
        goto finish;
    }

    CFStringAppendCString(string, "[URL]", kCFStringEncodingMacRoman);
    _appendCFString(string, absPath, false);

finish:
    if (absURL)  CFRelease(absURL);
    if (absPath) CFRelease(absPath);
    return;
}

void _appendPlist(CFMutableStringRef string, CFTypeRef plist, unsigned indentLevel)
{
    CFTypeID typeID = NULL;

    if (!plist) {
        return;
    }

    typeID = CFGetTypeID(plist);

    if (typeID == CFDictionaryGetTypeID()) {
        CFDictionaryRef dict = (CFDictionaryRef)plist;
        CFIndex count, i;
        CFStringRef * keys = NULL;   // must free
        CFStringRef * values = NULL; // must free
        count = CFDictionaryGetCount(dict);
        keys = (CFStringRef *)malloc(count * sizeof(CFStringRef));
        values = (CFStringRef *)malloc(count * sizeof(CFTypeRef));

        CFDictionaryGetKeysAndValues(dict, (const void **)keys,
            (const void **)values);

        // no indent before first brace
        CFStringAppendCString(string, "{\n", kCFStringEncodingMacRoman);
        for (i = 0; i < count; i++) {

            _indent(string, indentLevel + 4);
            _appendCFString(string, keys[i], true);
            CFStringAppendCString(string, " = ", kCFStringEncodingMacRoman);
            if (CFGetTypeID(values[i]) == CFStringGetTypeID()) {
                CFIndex keyLength = CFStringGetLength(keys[i]);
                CFIndex valueLength = CFStringGetLength(values[i]);
                if (indentLevel + 4 + keyLength + valueLength > 72) {
                    CFStringAppendCString(string, "\n", kCFStringEncodingMacRoman);
                    _indent(string, indentLevel + 8);
                }
            }
            _appendPlist(string, values[i], indentLevel + 4);
        }
        _indent(string, indentLevel);
        CFStringAppendCString(string, "}\n", kCFStringEncodingMacRoman);
        free(keys);
        free(values);
    } else if (typeID == CFArrayGetTypeID()) {
        CFArrayRef array = (CFArrayRef)plist;
        CFIndex count, i;
        count = CFArrayGetCount(array);

        // no indent before first parenthesis
        CFStringAppendCString(string, "(\n", kCFStringEncodingMacRoman);
        for (i = 0; i < count; i++) {
            _indent(string, indentLevel + 4);
            _appendPlist(string, CFArrayGetValueAtIndex(array, i), indentLevel + 4);
        }
        _indent(string, indentLevel);
        CFStringAppendCString(string, ")\n", kCFStringEncodingMacRoman);
    } else if (typeID == CFStringGetTypeID()) {
        _appendCFString(string, (CFStringRef)plist, indentLevel > 0);
        CFStringAppendCString(string, "\n", kCFStringEncodingMacRoman);
    } else if (typeID == CFURLGetTypeID()) {
        _appendCFURL(string, (CFURLRef)plist);
        CFStringAppendCString(string, "\n", kCFStringEncodingMacRoman);
    } else if (typeID == CFDataGetTypeID()) {
        CFStringAppendCString(string, "(data object)\n", kCFStringEncodingMacRoman);
    } else if (typeID == CFNumberGetTypeID()) {
        CFStringAppendFormat(string, NULL, CFSTR("%@"), (CFNumberRef)plist);
        CFStringAppendCString(string, "\n", kCFStringEncodingMacRoman);
    } else if (typeID == CFBooleanGetTypeID()) {
        CFBooleanRef booleanValue = (CFBooleanRef)plist;
        CFStringAppendFormat(string, NULL, CFSTR("%s\n"),
            CFBooleanGetValue(booleanValue) ? "true" : "false");
    } else if (typeID == CFDateGetTypeID()) {
        CFStringAppendCString(string, "(date object)\n", kCFStringEncodingMacRoman);
    } else {
        CFStringAppendCString(string, "(unknown object)\n", kCFStringEncodingMacRoman);
    }
    return;
}

void printPList(FILE * stream, CFTypeRef plist)
{
    CFMutableStringRef string = NULL;  // must release
    CFIndex stringLength;
    char * c_string = NULL; // must free

    string = createCFStringForPlist(plist);
    if (!string) {
        goto finish;
    }

    stringLength = CFStringGetLength(string);
    c_string = (char *)malloc((1 + stringLength) * sizeof(char));
    if (!c_string) {
        goto finish;
    }

    if (CFStringGetCString(string, c_string, stringLength + 1,
        kCFStringEncodingMacRoman)) {

        fprintf(stream, c_string);
    }

finish:
    if (string) CFRelease(string);
    if (c_string) free(c_string);
    return;
}

// use in GDB
void showPList(CFPropertyListRef plist)
{
    printPList(stdout, plist);
    return;
}

CFMutableStringRef createCFStringForPlist(CFTypeRef plist)
{
    CFMutableStringRef string = NULL;  // must release

    string = CFStringCreateMutable(kCFAllocatorDefault, 0);
    if (!string) {
        goto finish;
    }

    _appendPlist(string, plist, 0);

finish:
    return string;
}
