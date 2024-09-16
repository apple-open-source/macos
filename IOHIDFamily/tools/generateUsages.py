import codecs
import json
import pathlib
import sys

hidUsageBase = '''//
//  HIDUsage.swift
//  CoreHID
//
//  Copyright © 2024 Apple Inc. All rights reserved.
//

import Foundation

/// A type to represent HID usage pages.
///
/// A HID usage page combines with a HID usage to specify the intended functionality for the associated item.
/// Associated items can be descriptors, devices, reports, report data, elements, etc..
///
/// Currently unsupported cases can be used as ``generic(_:_:)``, but may be added as supported cases later.
///
/// See the HID specification for more details: [](https://www.usb.org/hid).
@available(macOS 15, *)
public enum HIDUsage : Hashable, Sendable {{
{usagePageEnumCases}    case generic(UInt16, UInt16?)

    /// The usage page value.
    ///
    /// This value determines the broader category of the functionality.
    /// This must always be specified, as a usage doesn't have meaning without a page.
    public var page: UInt16 {{
        switch self {{
{usagePageSwitchCases}        case .generic(let page, _):
            return page
        }}
    }}

    /// The usage value.
    ///
    /// The usage combines with the page to determine specific functionality.
    /// This may not be specified, in which case the HIDUsage refers to the overall page.
    public var usage: UInt16? {{
        switch self {{
{usageSwitchCases}        case .generic(_, let usage):
            return usage
        }}
    }}

    /// Create a HIDUsage from raw page and usage values.
    ///
    /// Currently unsupported cases will be returned as ``generic(_:_:)``, but may be added as supported cases later.
    public init(page: UInt16, usage: UInt16?) {{
        switch page {{
{initSwitchCases}        default:
            self = .generic(page, usage)
        }}
    }}

    public static func ==(lhs: HIDUsage, rhs: HIDUsage) -> Bool {{ return lhs.page == rhs.page && lhs.usage == rhs.usage }}
}}

@available(macOS 15, *)
extension HIDUsage : CustomStringConvertible {{
    public var description: String {{ "CoreHID.HIDUsage(page: \(self.page)\(self.usage == nil ? "" : ", usage: \(self.usage!)"))" }}
}}

'''

usagePageEnumCaseBase = '''    case {camelCaseUsagePage}({upperCaseUsagePage}Usage?)
'''

usagePageEnumCaseSpecialBase = '''    case {camelCaseUsagePage}(UInt16?)
'''

usagePageSwitchCaseBase = '''        case .{camelCaseUsagePage}(_):
            return {upperCaseUsagePage}Usage.page
'''

usageSwitchCaseBase = '''        case .{camelCaseUsagePage}(let usage):
            return usage?.rawValue
'''

usageSwitchCaseSpecialBase = '''        case .{camelCaseUsagePage}(let usage):
            return usage
'''

initSwitchCaseBase = '''        case {upperCaseUsagePage}Usage.page:
            if let usageValue = usage {{
                if let usageType = {upperCaseUsagePage}Usage(rawValue: usageValue) {{
                    self = .{camelCaseUsagePage}(usageType)
                }} else {{
                    self = .generic(page, usageValue)
                }}
            }} else {{
                self = .{camelCaseUsagePage}(nil)
            }}

'''

initSwitchCaseSpecialBase = '''        case {upperCaseUsagePage}Usage.page:
            self = .{camelCaseUsagePage}(usage)

'''

extensionBase = '''/// An extension for {upperCaseUsagePage} related ``HIDUsage``s.
///
/// Values are referenced from the HID Usage Tables document: [](https://www.usb.org/hid).
@available(macOS 15.0, *)
public extension HIDUsage {{
    enum {upperCaseUsagePage}Usage : UInt16, Sendable {{
{usageEnumCases}
        public static let page: UInt16 = 0x{usagePageID:02x}
    }}
}}

'''

extensionSpecialBase = '''/// An extension for {upperCaseUsagePage} related ``HIDUsage``s.
///
/// {upperCaseUsagePage} usages are generic, and can be created with any UInt16 value.
/// ```swift
/// let {camelCaseUsagePage}Usage = HIDUsage.{camelCaseUsagePage}(1)
/// ```
/// Values are referenced from the HID Usage Tables document: [](https://www.usb.org/hid).
@available(macOS 15.0, *)
public extension HIDUsage {{
    enum {upperCaseUsagePage}Usage : Sendable {{
        public static let page: UInt16 = 0x{usagePageID:02x}
    }}
}}

'''

usageEnumCaseBase = '''        case {camelCaseUsage}{buffer}= 0x{usageID:02x}
'''

# There are a lot of special string rules that we have to follow
# Such as camel case LED being led instead of lED, and removing '/' characters
def applyStringRules(token, camelCase):
    # There are a few '/' cases where converting to "Or" is not appropriate
    token = token.replace("I/O", "IO")
    token = token.replace("A/V", "AV")
    token = token.replace("EAN 2/3", "EAN 2Of3")
    token = token.replace('/', "Or")
    token = token.replace('+', "Plus")
    token = token.replace(':', '')
    token = token.replace('(', '')
    token = token.replace(')', '')
    token = token.replace(',', '')
    token = token.replace("Mfg", "Manufacturing")
    token = token.replace("Misc.", "Miscellaneous")
    token = token.replace("PROPERTYKEY", "PropertyKey")
    token = token.replace("VT\\_", "VariableType")
    token = token.replace("BOOL", "Bool")
    token = token.replace("VECTOR", "Vector")

    if token.startswith("SoC"):
        token = token.replace("SoC", "SOC", 1)

    if token.startswith("3D"):
        token = token[3:] + "3D"

    if token.startswith("2D"):
        token = token[3:] + "2D"

    if token.startswith("Misc"):
        token = token.replace("Misc", "Miscellaneous", 1)

    if token.startswith("Err") and not token.startswith("Error"):
        token = token.replace("Err", "Error", 1)

    # repeat is an illegal name in Swift
    if token == "Repeat":
        token = "RepeatTrack"

    # We can't have numbers at the start of the name
    # Instead of dealing with the mess of installing python packages during build or writing a crazy conversion function, let's just brute force this with the known cases
    if token.startswith("14"):
        token = token.replace("14", "Fourteen", 1)
    elif token.startswith("11"):
        token = token.replace("11", "Eleven", 1)
    elif token.startswith("10"):
        token = token.replace("10", "Ten", 1)
    elif token.startswith("9"):
        token = token.replace("9", "Nine", 1)
    elif token.startswith("8"):
        token = token.replace("8", "Eight", 1)
    elif token.startswith("7"):
        token = token.replace("7", "Seven", 1)
    elif token.startswith("6"):
        token = token.replace("6", "Six", 1)
    elif token.startswith("5"):
        token = token.replace("5", "Five", 1)
    elif token.startswith("4"):
        token = token.replace("4", "Four", 1)
    elif token.startswith("3"):
        token = token.replace("3", "Three", 1)
    elif token.startswith("2"):
        token = token.replace("2", "Two", 1)
    elif token.startswith("1"):
        token = token.replace("1", "One", 1)

    # Make all words start with an uppercase, a lot of words like 'and' were left lowercase
    result = []
    for t in token.split(' '):
        result.append(t[0].upper() + t[1:])

    if camelCase:
        # Make the first letter and all letters for acronyms in the first word lowercase
        # Examples: AM / PM -> am / PM, ErrorUndefined -> errorUndefined, VCR / TV -> vcr / TV, AL OEM Help -> al OEM Help
        firstWord = []
        notDone = True
        for i, char in enumerate(result[0]):
            if notDone and i + 1 < len(result[0]):
                # We're done the first time the next character is lowercase
                notDone = result[0][i + 1].isupper() or result[0][i + 1].isnumeric() or result[0][i + 1] == '-'
            if notDone or i == 0:
                firstWord.append(char.lower())
            else:
                firstWord.append(char)
        result[0] = ''.join(firstWord)

    token = ''.join(result)

    # We have to remove a few select characters and set the next character to uppercase
    result = []
    make_upper = False
    for char in token:
        if char == '-' or char == '‐':
            make_upper = True
        else:
            if make_upper:
                result.append(char.upper())
                make_upper = False
            else:
                result.append(char)
    token = ''.join(result)

    return token

class UsagesGenerator:

    def printUsageAndExit(self, errorMessage = None):
        code = 0
        if errorMessage:
            print("\nERROR: {}".format(errorMessage))
            code = 1

        print("\nusage: generateUsages.py [-h | --help] [-i | --internal] [-f | --file <input file path>] [-o | --output <output directory path>\n")

        # TODO: Implement generation of IOHIDUsageTables.h
        # print("A helper command line tool for generating HIDUsage.swift and from HidUsageTables.json, the official source of HID usages.\nThis file is embedded in the HID Usage Tables document: https://www.usb.org/hid.\n")
        print("A helper command line tool for generating HIDUsage.swift from HidUsageTables.json, the official source of HID usages.\nThis file is embedded in the HID Usage Tables document: https://www.usb.org/hid.\n")

        print("Options:\n")

        print("    -h | --help                         Print this usage message.")
        print("    -i | --internal                     Run the internal version, which outputs AppleHIDUsage.swift and AppleHIDUsageTables.h.")
        print("    -f | --file   <input file path>     The path to the input JSON file of HID usages, the format should be similar to HidUsageTables.json.")
        print("    -o | --output <output file path>    The output directory path for the generated files.\n")

        exit(code)

    def __init__(self, argumentList):
        self.inputFilePath       = None
        self.outputDirectoryPath = None
        self.inputFile           = None
        self.usagePages          = None

        self.parseArguments(argumentList)
        self.verifyFiles()

    def parseArguments(self, argumentList):
        i       = 1
        numArgs = len(argumentList)

        if i == numArgs:
            self.printUsageAndExit()

        while(i < numArgs):
            argument = argumentList[i]
            if i + 1 < numArgs:
                nextArgument = argumentList[i + 1]
            else:
                nextArgument = None

            if argument == "-h" or argument == "--help":
                self.printUsageAndExit()

            elif argument == "-i" or argument == "--internal":
                # TODO: Implement the internal version
                print("\nThe internal version is not yet supported.\n")
                exit(1)

            elif argument == "-f" or argument == "--file":
                if self.inputFilePath:
                    self.printUsageAndExit("Input file path specified multiple times.")
                if nextArgument == None:
                    self.printUsageAndExit("Missing input file path.")
                else:
                    self.inputFilePath = nextArgument
                    i += 1

            elif argument == "-o" or argument == "--output":
                if self.outputDirectoryPath:
                    self.printUsageAndExit("Output directory path specified multiple times.")
                if nextArgument == None:
                    self.printUsageAndExit("Missing output directory path.")
                else:
                    self.outputDirectoryPath = nextArgument
                    i += 1

            else:
                self.printUsageAndExit("Unrecognized argument: {}".format(argument))

            i += 1

    def verifyFiles(self):
        if self.inputFilePath == None or self.outputDirectoryPath == None:
            self.printUsageAndExit("-f and -o are required.")

        self.inputFilePath       = pathlib.Path(self.inputFilePath)
        self.outputDirectoryPath = pathlib.Path(self.outputDirectoryPath)

        if not self.inputFilePath.is_file():
            self.printUsageAndExit("Bad input file: {}".format(self.inputFilePath))

        self.outputDirectoryPath.mkdir(parents=True, exist_ok=True)

    def parseInputFile(self):
        with self.inputFilePath.open(mode='r') as self.inputFile:
            # The usage pages are not sorted for some reason, but the usages are
            self.usagePages = sorted(json.load(self.inputFile)["UsagePages"], key=lambda x: x['Id'])

    def writeSwiftFile(self):
        # Iterate over every usage page and construct the usage page templates
        usagePageEnumCases   = ""
        usagePageSwitchCases = ""
        usageSwitchCases     = ""
        initSwitchCases      = ""
        extensions           = ""
        for usagePage in self.usagePages:
            upperCaseUsagePage = applyStringRules(usagePage["Name"], False)
            camelCaseUsagePage = applyStringRules(usagePage["Name"], True)

            # The Button, Ordinal and MonitorEnumerated pages don't have defined usages, their usages are just 1, 2, 3, 4, ..., 65535
            # So we have to handle them specially and allow creation from any UInt16
            if len(usagePage["UsageIds"]) == 0:
                special = True
                usagePageEnumCases += usagePageEnumCaseSpecialBase.format(camelCaseUsagePage=camelCaseUsagePage, upperCaseUsagePage=upperCaseUsagePage)
                usageSwitchCases   += usageSwitchCaseSpecialBase.format(camelCaseUsagePage=camelCaseUsagePage)
                initSwitchCases    += initSwitchCaseSpecialBase.format(camelCaseUsagePage=camelCaseUsagePage, upperCaseUsagePage=upperCaseUsagePage)

            else:
                special = False
                usagePageEnumCases += usagePageEnumCaseBase.format(camelCaseUsagePage=camelCaseUsagePage, upperCaseUsagePage=upperCaseUsagePage)
                usageSwitchCases   += usageSwitchCaseBase.format(camelCaseUsagePage=camelCaseUsagePage)
                initSwitchCases    += initSwitchCaseBase.format(camelCaseUsagePage=camelCaseUsagePage, upperCaseUsagePage=upperCaseUsagePage)

            usagePageSwitchCases += usagePageSwitchCaseBase.format(camelCaseUsagePage=camelCaseUsagePage, upperCaseUsagePage=upperCaseUsagePage)

            # Iterate over every usage for the usage page and construct the usage templates
            # Do a first round to get the largest name, for pretty printing
            largestName = 0
            for usage in usagePage["UsageIds"]:
                upperCaseUsage = applyStringRules(usage["Name"], False)
                largestName    = max(len(upperCaseUsage), largestName)

            usageEnumCases = ""
            for usage in usagePage["UsageIds"]:
                camelCaseUsage = applyStringRules(usage["Name"], True)
                buffer         = ' ' * (largestName - len(camelCaseUsage) + 1)

                usageEnumCases += usageEnumCaseBase.format(camelCaseUsage=camelCaseUsage, buffer=buffer, usageID=usage["Id"])

            if special:
                extensions += extensionSpecialBase.format(upperCaseUsagePage=upperCaseUsagePage, camelCaseUsagePage=camelCaseUsagePage, usagePageID=usagePage["Id"])
            else:
                extensions += extensionBase.format(upperCaseUsagePage=upperCaseUsagePage, usageEnumCases=usageEnumCases, usagePageID=usagePage["Id"])

        hidUsage = hidUsageBase.format(usagePageEnumCases=usagePageEnumCases, usagePageSwitchCases=usagePageSwitchCases, usageSwitchCases=usageSwitchCases, initSwitchCases=initSwitchCases)

        with open(self.outputDirectoryPath.joinpath("HIDUsage.swift"), 'w') as outputFile:
            outputFile.write(hidUsage)
            outputFile.write(extensions)

def generateUsages(argumentList=None):
    if argumentList == None:
        argumentList = sys.argv

    generator = UsagesGenerator(argumentList)

    # Read the input JSON
    generator.parseInputFile()

    # Create the file of HIDUsages in Swift for CoreHID
    generator.writeSwiftFile()

    print("Success")

if __name__ == "__main__":
    generateUsages()
