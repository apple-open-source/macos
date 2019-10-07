import os, sys, getopt, re, subprocess, random, time, json, plistlib
from collections import OrderedDict


copyright = """/*
*
* @APPLE_LICENSE_HEADER_START@
*
* Copyright (c) 2019 Apple Computer, Inc.  All Rights Reserved.
*
* This file contains Original Code and/or Modifications of Original Code
* as defined in and that are subject to the Apple Public Source License
* Version 2.0 (the 'License'). You may not use this file except in
* compliance with the License. Please obtain a copy of the License at
* http://www.opensource.apple.com/apsl/ and read it before using this
* file.
*
* The Original Code and all software distributed under the License are
* distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
* EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
* INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
* Please see the License for the specific language governing rights and
* limitations under the License.
*
* @APPLE_LICENSE_HEADER_END@
*/"""


#
# Return filed which is selector for given field
#
def GetEventFieldSelector(field):
    selectorName = field["selector"]["name"]
    path = []
    GetEventFieldPath(field, path)
    for pathField in path:
        for fieldType in ['fields', 'base_fields']:
            if fieldType in pathField.keys():
                for field in pathField[fieldType]:
                    if field['name'] == selectorName:
                        return field
    return None


#
# field has selector (used to specify which union member is calid/selected)
#
def hasSelector(field):
    if "selector" in field.keys():
        return True

    return False


#
# Ordered path to event
#
def GetEventFieldPath(field, path):
    path.insert(0, field)
    if 'parent' in field.keys():
        GetEventFieldPath(field['parent'], path)


#
# List of all field in event
#
def GetEventFieldsList(event, list):
    for fieldType in ['base_fields', 'fields']:
        if fieldType not in  event.keys():
            continue
        for field in event[fieldType]:
            if 'fields' in field.keys():
                GetEventFieldsList(field, list)
            else:
                list.append(field)


#
# get bit length of value field
#
def GetEventFieldBitLength(field):
    if 'length' in field.keys():
        return field['length']
    if 'int' in field['canonical_type']:
        return 32
    if 'short' in field['canonical_type']:
        return 16
    if 'char' in field['canonical_type']:
        return 8
    if 'long long' in field['canonical_type']:
        return 64

    raise AssertionError()
    return 0


#
# concatenated name of the field
#
def GetEventFieldNameString(field):
    if 'field_def_override' in field.keys():
        return field['field_def_override']
    nameString = ''
    path = []
    GetEventFieldPath(field, path)
    for node in path:
        nodeName = GetTypeName(node)
        if nodeName == "":
            continue
        nameString = nameString + nodeName[0].upper() + nodeName[1:]
    return "kIOHIDEventField" + nameString


#
# Concatenated access path to field
#
def GetEventFieldAccessString(field):
    access = ''
    path = []
    GetEventFieldPath(field, path)
    for index in range(1, len(path)):
        node = path[index]
        nodeName = GetTypeName(node)
        if nodeName == "":
            continue
        if access == '':
            access = access + node['name']
        else:
            access = access + '.' + node['name']
    return access


def hasDefinition(field):
    if (field["name"].title() in ['Size', 'Type', 'Options', 'Depth']) or (
            'reserved' in field["name"]):
        return True
    return False


def hasDeclaration(event):
    if event['name'] in ['Axis', 'Motion', 'Swipe', 'Mouse']:
        return True
    return False


def GetTypeName(element):
    name = ""
    if 'name' in element.keys():
        name = element['name']
    return name


def GetEventTypeName(event):
    name = event['name']
    return name


def GenEventFieldTypeDefs(indent, element, fieldType='fields'):
    for field in element[fieldType]:
        if field["kind"] == "union":
            GenEventUnionTypeField(indent, field)
        if field["kind"] == "struct":
            GenEventStructTypeField(indent, field)
        if field["kind"] == "array":
            GenEventArrayTypeField(indent, field)
        if field["kind"] == "value":
            GenEventValueTypeField(indent, field)


def GenEventArrayTypeField(indent, element):
    print "%s%s %s[%d];" % (indent * ' ', element["type"], element["name"],
                            element["length"])


def GenEventValueTypeField(indent, element):
    if 'length' in element.keys():
        print "%s%s %s:%d;" % (indent * ' ', element["type"], element["name"],
                               element["length"])
    else:
        print "%s%s %s;" % (indent * ' ', element["type"], element["name"])


def GenEventStructTypeField(indent, element):
    print "%sstruct {" % (indent * ' ')
    GenEventFieldTypeDefs(indent + 4, element)
    print "%s} %s;" % (indent * ' ', GetTypeName(element))


def GenEventUnionTypeField(indent, element):
    print "%sunion  {" % (indent * ' ')
    GenEventFieldTypeDefs(indent + 4, element)
    print "%s} %s;" % (indent * ' ', GetTypeName(element))


def GenBaseEventStructDef(event):
    typename = "IOHID%sEventData" % (event['name'])
    print "typedef struct {"
    indent = 4
    GenEventFieldTypeDefs(indent, event, 'base_fields')
    print "} %s;" % (typename)

def GenEventStructDef(event):
    typename = "IOHID%sEventData" % (event['name'])
    print "typedef struct {"
    indent = 4
    #    print "    IOHIDEventDataCommon common;"
    print "    %s base;" % (typename)
    if 'fields' in event.keys():
        GenEventFieldTypeDefs(indent, event, 'fields')
    print "} __%s;" % (typename)

#
# Generate event data structs
#
def GenStructs(events):
    for eventName,event in events.items():
        if hasDeclaration(event):
            continue
        GenBaseEventStructDef(event)
        print ""
        GenEventStructDef(event);
        print ""


def GetTypeInfo(type):
    if type == "IOFixed":
        return "fixed"
    if type == "double":
        return "double"
    if type == "data":
        return "data"
    return "integer"


#
# Generate field definitons
#
def GenFieldsDefs(events):
    for eventName,event in events.items():
        if hasDeclaration(event):
            continue

        fields = []
        fieldDict = {}
        GetEventFieldsList(event, fields)

        for field in fields:
            if hasDefinition(field):
                continue

            filedNameString = GetEventFieldNameString(field)

            if (filedNameString in fieldDict.keys()) and (
                    field['field_number'] != fieldDict[filedNameString]):
                raise AssertionError()

            fieldDict[filedNameString] = field['field_number']

        if fieldDict:
            print "#define kIOHIDEventField%sBase IOHIDEventFieldBase(kIOHIDEventType%s)" % (event['name'], event['name'])
                
                #print "typedef IOHID_ENUM (IOHIDEventField, IOHIDEvent%sFieldType) {" % (event['name'])
            for filedNameString in fieldDict.keys():
                print "static const IOHIDEventField %-58s        =  (kIOHIDEventField%sBase | %d);" % (filedNameString, event['name'], fieldDict[filedNameString])
            print ""


#
# Link fileds hierarchy in lists so each child has reference to parent
#
def ProcessEvents(events):
    for eventName,event in events.items():
        ProcessEventData(event, 'base_fields', True)
        ProcessEventData(event, 'fields', False)

#
# Link fileds hierarchy in lists so each child has reference to parent
#
def ProcessEventData(object, fieldType='fields', isBase = True):
    if fieldType in object.keys():
        for child in object[fieldType]:
            child['parent'] = object
            child['base'] = isBase
            if 'fields' in child.keys():
                ProcessEventData(child, 'fields', isBase)


#
# Gen field access macro
#
def GenAccessMacro(events):
    valueTypes = ["CFIndex", "double", "IOFixed", "data"]
    for eventName,event in events.items():
        if hasDeclaration(event) or event['name'] == "":
            continue
        for valueType in valueTypes:
            GenEventAccessMacro(event, valueType, "setter")
            GenEventAccessMacro(event, valueType, "getter")
    print ""
    for valueType in valueTypes:
        for eventName,event in events.items():
            if hasDeclaration(event):
                continue
            print "#ifndef _IOHID%sGetSynthesizedFieldsAs%sMacro" % (
                GetEventTypeName(event), GetTypeInfo(valueType).title())
            print "#define _IOHID%sGetSynthesizedFieldsAs%sMacro _IOHIDUnknowDefaultField" % (
                GetEventTypeName(event), GetTypeInfo(valueType).title())
            print "#endif"
            print "#ifndef _IOHID%sSetSynthesizedFieldsAs%sMacro" % (
                GetEventTypeName(event), GetTypeInfo(valueType).title())
            print "#define _IOHID%sSetSynthesizedFieldsAs%sMacro _IOHIDUnknowDefaultField" % (
                GetEventTypeName(event), GetTypeInfo(valueType).title())
            print "#endif"
    print ""
    for valueType in valueTypes:
        print "#define IOHIDEventSet%sFieldsMacro(event, field) \\" % (GetTypeInfo(valueType).title())
        macroString = ""
        for eventName,event in events.items():
            if hasDeclaration(event) or event['name'] == "":
                continue
            if macroString != "":
                macroString = macroString + "\n"
            macroString = macroString + "     _IOHID%sSetFieldsAs%sMacro(event, field)" % (GetEventTypeName(event), GetTypeInfo(valueType).title())
        print macroString.replace("\n", "\\\n") + "\n"
    print ""
    for valueType in valueTypes:
        print "#define IOHIDEventGet%sFieldsMacro(event, field) \\" % (
            GetTypeInfo(valueType).title())
        macroString = ""
        for eventName,event in events.items():
            if hasDeclaration(event) or event['name'] == "":
                continue
            if macroString != "":
                macroString = macroString + "\n"
            macroString = macroString + "    _IOHID%sGetFieldsAs%sMacro(event, field) " % (GetEventTypeName(event), GetTypeInfo(valueType).title())
        print macroString.replace("\n", "\\\n") + "\n"

    macroString = "#define IOHIDEventGetBaseSize(type, size)\n"
    macroString += "    switch(type) {\n"
    for eventName,event in events.items():
        if hasDeclaration(event) or event['name'] == "":
            continue
        typename = "IOHID%sEventData" % (event['name'])
        macroString += "      case kIOHIDEventType%s:\n" % (GetEventTypeName(event))
        macroString += "          size = sizeof (%s);\n" % (typename)
        macroString += "          break;\n"
    macroString += "      default:\n"
    macroString += "          size = 0;\n"
    macroString += "    }\n"
    print macroString.replace("\n", "\\\n") + "\n"

    macroString = "#define IOHIDEventGetSize(type, size) \n"
    macroString += "    switch(type) {\n"
    for eventName,event in events.items():
        if hasDeclaration(event) or event['name'] == "":
            continue
        typename = "IOHID%sEventData" % (event['name'])
        macroString += "      case kIOHIDEventType%s:\n" % (
            GetEventTypeName(event))
        macroString += "          size = sizeof (__%s);\n" % (typename)
        macroString += "          break; \n"
    macroString += "      default:\n"
    macroString += "          size = 0;\n"
    macroString += "    }\n"
    print macroString.replace("\n", "\\\n") + "\n"

def GenEventFieldAccessSetMacro(event, field, valueType, indent):
    fieldMacroString = ''
    fieldName = GetEventFieldNameString(field)
    fieldAccess = GetEventFieldAccessString(field)
    fieldTypeCast = GetEventTypecastValueToFieldMacroString(field, valueType)
    fieldMacroString += "%scase %s: \n" % (indent * ' ', fieldName)

    eventTypeName = "IOHID%sEventData" % (event['name'])
    if field['base'] == False:
        eventTypeName = "__" + eventTypeName

    if 'length' in field.keys() and field['length'] == 1:
        fieldMacroString += "%s    ((%s*)event)->%s = value ? 1 : 0; \n" % (
            indent * ' ', eventTypeName, fieldAccess)
    elif field['kind'] == 'array':
        fieldMacroString += "%s    *(typeof(value)*)(((%s*)event)->%s) = value; \n" % (
            indent * ' ', eventTypeName, fieldAccess)
    else:
        fieldMacroString += "%s    ((%s*)event)->%s = (typeof(((%s*)event)->%s)) %s(value); \n" % (
            indent * ' ', eventTypeName, fieldAccess, eventTypeName,
            fieldAccess, fieldTypeCast)
    fieldMacroString = fieldMacroString + "%s    break; \n" % (indent * ' ')
    return fieldMacroString


def GenEventFieldAccessGetMacro(event, field, valueType, indent):
    fieldMacroString = ''
    fieldName = GetEventFieldNameString(field)
    fieldAccess = GetEventFieldAccessString(field)
    fieldTypeCast = GetEventTypecastFieldToTypeMacroString(field, valueType)
    fieldMacroString += "%scase %s: \n" % (indent * ' ', fieldName)
    eventTypeName = "IOHID%sEventData" % (event['name'])
    if field['base'] == False:
        eventTypeName = "__" + eventTypeName
    
    if field['kind'] == 'array':
        fieldMacroString += "%s    value = *(typeof(value)*)((%s*)event)->%s; \n" % (indent * ' ', eventTypeName, fieldAccess)
    else:
        fieldMacroString += "%s    value = (typeof(value))%s(((%s*)event)->%s); \n" % (indent * ' ', fieldTypeCast, eventTypeName, fieldAccess)
    fieldMacroString += "%s    break; \n" % (indent * ' ')
    return fieldMacroString

def GenEventDataFieldSwitchStatment(event, valueType, macroType):
    valid = False
    fields = []
    GetEventFieldsList(event, fields)
    fieldMacroString = ""
    indent = 8

    fieldMacroString += "    switch (field)\n"
    fieldMacroString += "    {\n"
    for field in fields:
        if hasDefinition(field):
            continue

        if field['kind'] != 'array' or macroType == 'setter':
            continue

        fieldName = GetEventFieldNameString(field)
        fieldAccess = GetEventFieldAccessString(field)
        fieldMacroString += "%scase %s: \n" % (indent * ' ', fieldName)
        fieldMacroString += "%s    value = (typeof(value))(((IOHID%sEventData*)event)->%s); \n" % (indent * ' ', event['name'], fieldAccess)
        fieldMacroString += "%s    break;\n" % (indent * ' ')
        valid = True

    if macroType == "setter":
        fieldMacroString += "        _IOHID%sSetSynthesizedFieldsAs%sMacro(event,field) \n" % (GetEventTypeName(event), GetTypeInfo(valueType).title())
    else:
        fieldMacroString += "        _IOHID%sGetSynthesizedFieldsAs%sMacro(event,field) \n" % (GetEventTypeName(event), GetTypeInfo(valueType).title())

    fieldMacroString += "    }\n"
    return fieldMacroString, valid




def GenEventDataAccessMacro(event, valueType, macroType):
    valid = False
    fields = []
    GetEventFieldsList(event, fields)
    fieldMacroString = ""
    indent = 8
    if macroType == "setter":
        fieldMacroString += "#define _IOHID%sSetFieldsAs%sMacro(event, field) \n" % (GetEventTypeName(event), GetTypeInfo(valueType).title())
    else:
        fieldMacroString += "#define _IOHID%sGetFieldsAs%sMacro(event, field) \n" % (GetEventTypeName(event), GetTypeInfo(valueType).title())

    fieldMacroString += "case kIOHIDEventType%s:\n" % (GetEventTypeName(event))
    fieldMacroString += "{\n"
    switchStr,valid = GenEventDataFieldSwitchStatment (event, valueType, macroType);
    fieldMacroString += switchStr
    fieldMacroString += "    break;\n"
    fieldMacroString += "}\n"

    if not valid:
        if macroType == "setter":
            fieldMacroString = "#define _IOHID%sSetFieldsAs%sMacro(event, field) \n" % (GetEventTypeName(event), GetTypeInfo(valueType).title())
        else:
            fieldMacroString = "#define _IOHID%sGetFieldsAs%sMacro(event, field) \n" % (GetEventTypeName(event), GetTypeInfo(valueType).title())

    return fieldMacroString.replace ("\n", "\\\n")


def GenEventFieldSwitchStatment(event, valueType, macroType):
    fields = []
    fieldsWithSelector = {}
    GetEventFieldsList(event, fields)
    fieldMacroString  = "    switch (field)\n"
    fieldMacroString += "    {\n"

    for field in fields:
        if hasDefinition(field):
            continue

        if field['kind'] == 'array' and GetTypeInfo(
                valueType).title() in ["Double", "Fixed"]:
            continue

        if "immutable" in field.keys() and macroType == "setter":
            continue

        if hasSelector(field):
            selector = field['selector']
            selectorName = selector['name']
            selectorValue = selector['value']
            if selectorName not in fieldsWithSelector.keys():
                fieldsWithSelector[selectorName] = {}
            if selectorValue not in fieldsWithSelector[selectorName].keys():
                fieldsWithSelector[selectorName][selectorValue] = []
            fieldsWithSelector[selectorName][selectorValue].append(field)
            continue

        if macroType == "setter":
            fieldMacroString += GenEventFieldAccessSetMacro(event, field, valueType, 8)
        else:
            fieldMacroString += GenEventFieldAccessGetMacro(event, field, valueType, 8)

    for selectorName in fieldsWithSelector.keys():
        fieldNameSet = set()
        for selectorValue in fieldsWithSelector[selectorName].keys():
            for field in fieldsWithSelector[selectorName][selectorValue]:
                fieldName = GetEventFieldNameString(field)
                fieldNameSet.add(fieldName)

        for fieldName in fieldNameSet:
            fieldMacroString += "%scase %s: \n" % (8 * ' ', fieldName)

        ind = 12 * ' '
        for selectorValue in fieldsWithSelector[selectorName].keys():
            selector = GetEventFieldSelector(fieldsWithSelector[selectorName][selectorValue][0])
            selectorAccess = GetEventFieldAccessString(selector)
            
            eventTypeName = "IOHID%sEventData" % (event['name'])
            if selector['base'] == False:
                eventTypeName = "__" + eventTypeName

            fieldMacroString += ind + "if (((%s*)event)->%s == %s) {\n" % (eventTypeName, selectorAccess, selectorValue)
            fieldMacroString += ind + "    switch (field) \n"
            fieldMacroString += ind + "    {\n"

            for field in fieldsWithSelector[selectorName][selectorValue]:
                if macroType == "setter":
                    fieldMacroString += GenEventFieldAccessSetMacro(event, field, valueType, 20)
                else:
                    fieldMacroString += GenEventFieldAccessGetMacro(event, field, valueType, 20)
            
            fieldMacroString += ind + "    }\n"
            fieldMacroString += ind + "    break;\n"
            fieldMacroString += ind + "}\n"
        fieldMacroString += ind + 'break;\n'

    if macroType == "setter":
        fieldMacroString += "        _IOHID%sSetSynthesizedFieldsAs%sMacro(event,field) \n" % (GetEventTypeName(event), GetTypeInfo(valueType).title())
    else:
        fieldMacroString += "        _IOHID%sGetSynthesizedFieldsAs%sMacro(event,field) \n" % (GetEventTypeName(event), GetTypeInfo(valueType).title())

    fieldMacroString += "    }\n"
    return fieldMacroString

def GenEventValueSwitchStatment(event, valueType, macroType):
    fieldMacroString = ""
    fieldMacroString += "case kIOHIDEventType%s:\n" % (GetEventTypeName(event))
    fieldMacroString += "{\n"

    fieldMacroString += GenEventFieldSwitchStatment (event,valueType, macroType)

    fieldMacroString += "    break;\n"
    fieldMacroString += "}\n"
    return fieldMacroString


def GenEventValueAccessMacro(event, valueType, macroType):
    fieldMacroString = ""

    if macroType == "setter":
        fieldMacroString += "#define _IOHID%sSetFieldsAs%sMacro(event, field) \\\n" % (GetEventTypeName(event), GetTypeInfo(valueType).title())
    else:
        fieldMacroString += "#define _IOHID%sGetFieldsAs%sMacro(event, field) \\\n" % (GetEventTypeName(event), GetTypeInfo(valueType).title())

    fieldMacroString += GenEventValueSwitchStatment (event, valueType, macroType).replace("\n","\\\n") 
    return fieldMacroString


def GenEventAccessMacro(event, valueType, macroType):

    if GetTypeInfo(valueType).title() in ["Double", "Fixed", "Integer"]:
        fieldMacroString = GenEventValueAccessMacro(event, valueType, macroType)
    else:
        fieldMacroString = GenEventDataAccessMacro(event, valueType, macroType)

    print fieldMacroString



def GetSimpleTypeString(type):
    if type == "CFIndex":
        return "integer"
    if type == "IOFixed":
        return "fixed"
    if type == "double":
        return "double"
    return None


def GetEventTypecastFieldToTypeMacroString(field, valueType):
    simpleTypeString = GetSimpleTypeString(valueType)
    if field['canonical_type'] == 'double':
        return "CAST_DOUBLE_TO_%s" % (simpleTypeString.upper())
    if field['type'] == 'IOFixed':
        return "CAST_FIXED_TO_%s" % (simpleTypeString.upper())

    if simpleTypeString == 'fixed':
        fieldBitLength = GetEventFieldBitLength(field)
        if fieldBitLength < 32:
            return "CAST_SHORTINTEGER_TO_%s" % (simpleTypeString.upper())
    return "CAST_INTEGER_TO_%s" % (simpleTypeString.upper())


def GetEventTypecastValueToFieldMacroString(field, valueType):
    simpleTypeString = GetSimpleTypeString(valueType)
    if field['canonical_type'] == 'double':
        return "CAST_%s_TO_DOUBLE" % (simpleTypeString.upper())
    if field['type'] == 'IOFixed':
        return "CAST_%s_TO_FIXED" % (simpleTypeString.upper())
    return "CAST_%s_TO_INTEGER" % (simpleTypeString.upper())


def GetEventForName(events, name):
    for eventName,event in events.items():
        if event['name'] == name:
            return event
    return None

#
# create variable name for field
#
def GetVarName(field, fieldName, eventName):
    remStr = "kIOHIDEventField" #+ eventName
    nameString = ''
    if remStr in fieldName:
        nameString = fieldName.replace(remStr, '')
        if eventName.isupper() and eventName in fieldName:
            nameString = nameString.replace(eventName, eventName.lower())
        else:
            nameString = nameString[0].lower() + nameString[1:]

    return nameString

def getVarType(field):
    if field['kind'] == "array":
        return "uint8_t"
    
    return "NSNumber"

def getNSSubType(field):
    if field['kind'] == "array":
        return "Array"
    elif field['type'] == "IOFixed" or field['type'] == "IOHIDDouble":
        return "Float"
    
    return "Integer"

def getNSProperties(field):
    properties = []
    if "immutable" in field.keys():
        properties.append("readonly")
    
    return properties

def GenHeader(fieldDict, eventName):
    print "@interface HIDEvent (HIDUtil%sEvent)\n" % (eventName)
    descName = eventName
    
    for fieldName in fieldDict.keys():
        varName         = fieldDict[fieldName]['var_name']
        varType         = fieldDict[fieldName]['var_type']
        nsProperties    = fieldDict[fieldName]['ns_properties']

        sys.stdout.write("@property ")
        if nsProperties:
            sys.stdout.write("(")
            for property in nsProperties:
                sys.stdout.write(property)
            sys.stdout.write(") ")
        print "%s *%s;" % (varType, varName)

    if eventName.isupper():
        eventName = eventName.replace(eventName, eventName.lower())
    else:
        eventName = eventName[0].lower() + eventName[1:]

    print "\n- (NSString *)%sDescription;" % (eventName)
    
    print "\n@end\n\n"

def hasMultipleSelectors(field):
    ret = False
    for key in field.keys():
        if key == 'selector':
            if isinstance(field[key], list):
                ret = True
            break

    return ret
def GetSelectorFieldData(event, fieldData, eventsWithSelector, selectorValueList, otherFieldData):
    
    fields = []
    GetEventFieldsList(event, fields)
    
    for field in fields:
        if hasDefinition(field):
            continue
        fieldDict = {}
        fieldDict['name'] = GetEventFieldNameString(field)
        fieldDict['type'] = field['type']
        fieldDict['field_number'] = field['field_number']
        fieldDict['base'] = field['base']
        if field['kind'] == "array" or "immutable" in field.keys():
            fieldDict['readonly'] = True
        else:
            fieldDict['readonly'] = False

        if hasSelector(field):
            multipleSelectors = hasMultipleSelectors(field)
            if multipleSelectors:
                print multipleSelectors
                #future expansion
            else:
                selector = field['selector']
                selectorName = selector['name']
                selectorValue = selector['value']
                selctorNameString = GetEventFieldNameString(selector)
                fieldDict['selectorValue'] = selectorValue
                eventsWithSelector.add(event['name'])
                tmp = GetEventFieldSelector(field)
                fieldDict['selectorName'] = GetEventFieldNameString(tmp)
                fieldData.append(fieldDict)
                if fieldDict['selectorName'] not in selectorValueList.keys():
                    selectorValueList[fieldDict['selectorName']] = {}
                
                if selectorValue not in selectorValueList[fieldDict['selectorName']].keys():
                    selectorValueList[fieldDict['selectorName']][selectorValue] = []
                selectorValueList[fieldDict['selectorName']][selectorValue].append(fieldDict)
        else:
            otherFieldData.append(fieldDict)


def GenObjectDescription(event):
    fields = []
    fieldsWithSelector = {}
    GetEventFieldsList(event, fields)
    formatStr   = ""
    argumentStr = "" 
    eventName = event['name']
    if eventName.isupper():
        eventName = eventName.replace(eventName, eventName.lower())
    else:
        eventName = eventName[0].lower() + eventName[1:]

    descriptionString  = "- (NSString *)%sDescription {\n" % (eventName)
    descriptionString += "    NSString * desc;\n"

    # YUCK
    if event['name'] == "VendorDefined":
        descriptionString += "    if (self.vendorDefinedUsagePage.unsignedIntValue == kHIDPage_AppleVendor && self.vendorDefinedUsage.unsignedIntValue == kHIDUsage_AppleVendor_Perf) {\n"
        descriptionString += "        IOHIDEventPerfData *perfData = (IOHIDEventPerfData *)self.vendorDefinedData;\n"
        descriptionString += '        NSString *perfStr = [NSString stringWithFormat:@"driverDispatchTime:%llu eventSystemReceiveTime:%llu eventSystemDispatchTime:%llu eventSystemFilterTime:%llu eventSystemClientDispatchTime:%llu",\n'
        descriptionString += "            perfData->driverDispatchTime ? _IOHIDGetTimestampDelta(perfData->driverDispatchTime, self.timestamp.unsignedLongLongValue, kMicrosecondScale) : 0,\n"
        descriptionString += "            perfData->eventSystemReceiveTime ? _IOHIDGetTimestampDelta(perfData->eventSystemReceiveTime, self.timestamp.unsignedLongLongValue, kMicrosecondScale) : 0,\n"
        descriptionString += "            perfData->eventSystemDispatchTime ? _IOHIDGetTimestampDelta(perfData->eventSystemDispatchTime, self.timestamp.unsignedLongLongValue, kMicrosecondScale) : 0,\n"
        descriptionString += "            perfData->eventSystemFilterTime ? _IOHIDGetTimestampDelta(perfData->eventSystemFilterTime, self.timestamp.unsignedLongLongValue, kMicrosecondScale) : 0,\n"
        descriptionString += "            perfData->eventSystemClientDispatchTime ? _IOHIDGetTimestampDelta(perfData->eventSystemClientDispatchTime, self.timestamp.unsignedLongLongValue, kMicrosecondScale) : 0];\n\n"
        descriptionString += '        desc = [NSString stringWithFormat:@"vendorDefinedUsagePage:%@ vendorDefinedUsage:%@ vendorDefinedVersion:%@ vendorDefinedDataLength:%@ %@", self.vendorDefinedUsagePage, self.vendorDefinedUsage, self.vendorDefinedVersion, self.vendorDefinedDataLength, perfStr];\n'
        descriptionString += "    } else {\n"
        descriptionString += '        desc = [NSString stringWithFormat:@"vendorDefinedUsagePage:%@ vendorDefinedUsage:%@ vendorDefinedVersion:%@ vendorDefinedDataLength:%@ vendorDefinedDatastr:%@", self.vendorDefinedUsagePage, self.vendorDefinedUsage, self.vendorDefinedVersion, self.vendorDefinedDataLength, self.vendorDefinedDatastr];\n'
        descriptionString += "    }\n"
        descriptionString += "    return desc;\n"
        descriptionString += "}\n"
        descriptionString += "@end\n"
        print descriptionString
        return

    for field in fields:
        if hasDefinition(field):
            continue

        if hasSelector(field):
            selector = field['selector']
            selectorName = selector['name']
            selectorValue = selector['value']
            if selectorName not in fieldsWithSelector.keys():
                fieldsWithSelector[selectorName] = {}
            if selectorValue not in fieldsWithSelector[selectorName].keys():
                fieldsWithSelector[selectorName][selectorValue] = []
            fieldsWithSelector[selectorName][selectorValue].append(field)
            continue

        fieldName   = GetEventFieldNameString(field)
        varName     = GetVarName(field, fieldName, event['name'])
        if field['kind'] == "array":
            varName += 'str'
        formatStr   += " %s:%%@" % varName
        if field == fields[0]:
            argumentStr += "self.%s" % varName
        else:
            argumentStr += ", self.%s" % varName

    ind = 4 * ' '    
    descriptionString += ind + 'desc = [NSString stringWithFormat:@"%s"%s];\n' % (formatStr.lstrip(), argumentStr)
    
    for selectorName in fieldsWithSelector.keys():
        fieldNameSet = set()
        for selectorValue in fieldsWithSelector[selectorName].keys():
            for field in fieldsWithSelector[selectorName][selectorValue]:
                fieldName = GetEventFieldNameString(field)
                fieldNameSet.add(fieldName)

        # for fieldName in fieldNameSet:
        #     fieldMacroString += "%scase %s: \n" % (8 * ' ', fieldName)

        for selectorValue in fieldsWithSelector[selectorName].keys():
            selector = GetEventFieldSelector(fieldsWithSelector[selectorName][selectorValue][0])
            selectorAccess = GetEventFieldAccessString(selector)
            
            eventTypeName = "IOHID%sEventData" % (event['name'])
            if selector['base'] == False:
                eventTypeName = "__" + eventTypeName

            filedNameString = GetEventFieldNameString(selector)    
            descriptionString += ind + "if (IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, %s) == %s) {\n" % (filedNameString, selectorValue)
            formatStr = ""
            argumentStr = ""
            for field in fieldsWithSelector[selectorName][selectorValue]:
                fieldName   = GetEventFieldNameString(field)
                varName     = GetVarName(field, fieldName, event['name'])
                if field['kind'] == "array":
                    varName += 'str'
                formatStr += " %s:%%@" % varName
                argumentStr += ", self.%s" % varName

            descriptionString += ind + '    desc = [NSString stringWithFormat:@"%s %s", desc%s];\n' % ("%@", formatStr.lstrip(), argumentStr) 
            descriptionString += ind + "}\n"

    descriptionString += "    return desc;\n" 
    descriptionString += "}\n" 
    descriptionString += "@end\n" 

    print descriptionString 

def GenObject(fieldDict, event):
    eventName = event['name']
    print "@implementation HIDEvent (HIDUtil%sEvent)\n" % (eventName)
    
    for fieldName in fieldDict.keys():
        varName         = fieldDict[fieldName]['var_name']
        varType         = fieldDict[fieldName]['var_type']
        nsSubType       = fieldDict[fieldName]['ns_subtype']
        nsProperties    = fieldDict[fieldName]['ns_properties']
        numType         = nsSubType[0].lower() + nsSubType[1:]
        
        if numType == 'integer':
            numType = 'unsignedInt'
        
        print "- (%s *)%s {" % (varType, varName)
        if nsSubType == "Array":
            print "    return IOHIDEventGetDataValue((__bridge IOHIDEventRef)self, %s);" % (fieldName)
        else:
            print "    return [NSNumber numberWith%s:IOHIDEventGet%sValue((__bridge IOHIDEventRef)self, %s)];" % (
                nsSubType, nsSubType, fieldName)
        print "}\n"
        
        if nsSubType == "Array":
            print "- (NSString *)%sstr {" % (varName)
            print "    NSString *%sstr = [[NSString alloc] init];" % (varName)
            print "    uint8_t *%s = self.%s;\n" % (varName, varName)
            print "    for (uint32_t i = 0; i < self.vendorDefinedDataLength.unsignedIntValue; i++) {"
            print '        %sstr = [%sstr stringByAppendingString:[NSString stringWithFormat:@"%%02x ", %s[i]]];' % (
                    varName, varName, varName)
            print "    }"
            print "\n    return [%sstr substringToIndex:%sstr.length-1];" % (varName, varName)
            print "}\n"
        
        if "readonly" not in nsProperties:
            setName = varName[0].upper() + varName[1:]
            print "- (void)set%s:(%s *)%s {" % (setName, varType, varName)
            print "    IOHIDEventSet%sValue((__bridge IOHIDEventRef)self, %s, %s.%sValue);" % (
                nsSubType, fieldName, varName, numType)
            print "}\n"
    
    GenObjectDescription(event)

def GenObjectFields(fieldData, event):
    eventName = event['name']
    print "\nstatic HIDEventFieldInfo %sEventFields[] = {" % (eventName)
    for fieldDict in fieldData:
        print "\t{ %s, kEventFieldDataType_%s, %d, %d, \"%s\" }," % (fieldDict['name'], getFieldType(fieldDict['type']) , fieldDict['base'], fieldDict['readonly'], fieldDict['name'][len("kIOHIDEventField"):])
    print "\t{ 0, kEventFieldDataType_None,  0, 0, NULL }"
    print "};\n"

def GenObjectData(event):
    fieldDict = {}

    if hasDeclaration(event):
        return

    fields = []
    GetEventFieldsList(event, fields)

    for field in fields:
        if hasDefinition(field):
            continue

        fieldName       = GetEventFieldNameString(field)
        varName         = GetVarName(field, fieldName, event['name'])
        varType         = getVarType(field)
        nsSubType       = getNSSubType(field)
        nsProperties    = getNSProperties(field)
        
        if field['kind'] == "array":
            nsProperties.append("readonly")

        fieldDict[fieldName] = { "var_name": varName, 
                                 "var_type": varType, 
                                 "ns_subtype": nsSubType, 
                                 "ns_properties": nsProperties }
    
    return fieldDict

def GenFieldsData(event):
    
    fieldData = []
    fields = []
    GetEventFieldsList(event, fields)
    
    for field in fields:
        if hasDefinition(field):
            continue

        fieldNameString = GetEventFieldNameString(field)
        fieldDict = {}
        fieldDict['name'] = fieldNameString
        
        if 'type' not in field.keys() or 'field_number' not in field.keys():
            raise AssertionError()
        
        fieldDict['type'] = field['type']
        fieldDict['field_number'] = field['field_number']
        fieldDict['base'] = field['base']
        fieldData.append(fieldDict)
        if field['kind'] == "array" or "immutable" in field.keys():
            fieldDict['readonly'] = True
        else:
            fieldDict['readonly'] = False

    return fieldData


def GenHeaders(events):
    for eventName,event in events.items():
        objData = GenObjectData(event)
        if objData:
            GenHeader(objData, event['name'])

def GenObjects(events):
    for eventName,event in events.items():
        objData = GenObjectData(event)
        if objData:
            GenObject(objData, event)

#
# HIDEvent accessor header generation
#

supportedEventTypes = [ "VendorDefined", "Button", "Keyboard", "Scroll", "Orientation", 
                        "Digitizer", "AmbientLightSensor", "Accelerometer", "Proximity", 
                        "Temperature", "Pointer", "Gyro", "Compass", "Power", "LED", 
                        "Biometric", "AtmosphericPressure", "Force", "MotionActivity", 
                        "MotionGesture", "GameController", "Humidity", "Brightness", 
                        "GenericGesture"]

def getEventVarType(field):
    if field['kind'] == "array":
        return "uint8_t *"
    elif field['type'] == "IOFixed" or field['type'] == "IOHIDDouble":
        return "double"

    return field['type']

def GenEventObjectData(event):
    fieldDict = {}

    if hasDeclaration(event):
        return

    fields = []
    GetEventFieldsList(event, fields)

    for field in fields:
        if hasDefinition(field):
            continue

        fieldName       = GetEventFieldNameString(field)
        varName         = GetVarName(field, fieldName, event['name'])
        varType         = getEventVarType(field)
        nsProperties    = getNSProperties(field)
        
        if field['kind'] == "array":
            nsProperties.append("readonly")

        fieldDict[fieldName] = { "var_name": varName, 
                                 "var_type": varType, 
                                 "ns_properties": nsProperties }
        fieldDict['base'] = field['base']
    
    return fieldDict

def FieldForFieldName(event, fieldName):
    result = {}
    fields = []
    GetEventFieldsList(event, fields)

    for field in fields:
        if field["name"] == fieldName:
            result = field
            break
    return result

def FieldForSelectorName(event, selectorName, fieldName):
    result = {}
    fields = []
    GetEventFieldsList(event, fields)

    for field in fields:
        if "selector" in field:
            if field["selector"]["value"] == selectorName:
                if fieldName is None:
                    result = field
                    break
                elif fieldName == field['name']:
                    result = field
                    break
    return result

def GenEventHeader(fieldDict, event, public):
    eventName = event['name']

    if eventName not in supportedEventTypes:
        return

    if public:
        print "@interface HIDEvent (HID%sEvent)\n" % (eventName)
    else:
        print "@interface HIDEvent (HID%sEventPrivate)\n" % (eventName)
    
    descName = eventName

    if eventName.isupper():
        eventName = eventName.replace(eventName, eventName.lower())
    else:
        eventName = eventName[0].lower() + eventName[1:]

    if "createFunctions" in event and public == True:
        for function in event['createFunctions']:
            if "name" in function:
                setName = eventName[0].upper() + eventName[1:]
                sys.stdout.write("+ (instancetype)%s%sEvent:(uint64_t)timestamp " % (function['name'], setName))
            else:
                sys.stdout.write("+ (instancetype)%sEvent:(uint64_t)timestamp " % (eventName))

            for fieldName in function['fields']:
                if "selector" in function:
                    field = FieldForSelectorName(event, function["selector"], fieldName)
                else:
                    field = FieldForFieldName(event, fieldName)

                varType = getEventVarType(field)
                varName = field['name']

                if "var_name" in field:
                    varName = field["var_name"]

                sys.stdout.write("%s:(%s)%s " % (varName, varType, varName))

            print "options:(uint32_t)options;\n"

    for fieldName in sorted(fieldDict.keys()):
        if fieldName == "base" or public == True:
            continue

        varName         = fieldDict[fieldName]['var_name']
        varType         = fieldDict[fieldName]['var_type']
        nsProperties    = fieldDict[fieldName]['ns_properties']

        sys.stdout.write("@property ")
        if nsProperties:
            sys.stdout.write("(")
            for property in nsProperties:
                sys.stdout.write(property)
            sys.stdout.write(") ")
        print "%s %s;" % (varType, varName)
    
    print "@end\n"

def GenEventAccessor(fieldDict, event):
    eventName = event['name']

    if eventName not in supportedEventTypes:
        return

    if "createFunctions" in event:
        print "@implementation HIDEvent (HID%sEvent)\n" % (eventName)

        for function in event['createFunctions']:
            if "name" in function:
                setName = eventName[0].upper() + eventName[1:]
                sys.stdout.write("+ (instancetype)%s%sEvent:(uint64_t)timestamp " % (function['name'], setName))
            else:
                setName = eventName[0].lower() + eventName[1:]
                sys.stdout.write("+ (instancetype)%sEvent:(uint64_t)timestamp " % (setName))
            
            for fieldName in function['fields']:
                if "selector" in function:
                    field = FieldForSelectorName(event, function["selector"], fieldName)
                else:
                    field = FieldForFieldName(event, fieldName)

                varType = getEventVarType(field)
                varName = field['name']

                if "var_name" in field:
                    varName = field["var_name"]

                sys.stdout.write("%s:(%s)%s " % (varName, varType, varName))

            print "options:(uint32_t)options\n{"

            eventDataStr = "IOHID"+ eventName + "EventData"

            if fieldDict['base'] == False:
                eventDataStr = "__" + eventDataStr

            if eventName == "VendorDefined":
                print "    CFIndex eventSize = sizeof(%s) + length;" % (eventDataStr)
            else:
                print "    CFIndex eventSize = sizeof(%s);" % (eventDataStr)

            print "    HIDEvent *event = (__bridge_transfer HIDEvent *)_IOHIDEventCreate(kCFAllocatorDefault, eventSize, kIOHIDEventType%s, timestamp, options);" % (eventName)
            print "    %s *eventData = (%s *)event->_event.eventData;\n" % (eventDataStr, eventDataStr)

            if "selector" in function:
                field = FieldForSelectorName(event, function["selector"], None)
                if field:
                    print "    eventData->%s = %s;" % (field["selector"]["name"], function["selector"])

            for fieldName in function['fields']:
                if "selector" in function:
                    field = FieldForSelectorName(event, function["selector"], fieldName)
                else:
                    field = FieldForFieldName(event, fieldName)

                varType = getEventVarType(field)
                varName = field['name']
                baseStr = ""

                if "var_name" in field:
                    varName = field["var_name"]

                if fieldDict['base'] == False and field['base'] == True:
                    baseStr = "base."

                if field['kind'] == "array":
                    print "    bcopy(%s, eventData->%s, length);" % (field['name'], field['name'])
                elif field['type'] == "IOFixed":
                    print "    eventData->%s%s = CAST_DOUBLE_TO_FIXED(%s);" % (baseStr, GetEventFieldAccessString(field), varName)
                else:
                    print "    eventData->%s%s = (%s)%s;" % (baseStr, GetEventFieldAccessString(field), field['type'], varName)

            print "\n    return event;\n}\n"

        print "@end\n"

    print "@implementation HIDEvent (HID%sEventPrivate)\n" % (eventName)

    for fieldName in sorted(fieldDict.keys()):
        if fieldName == "base":
            continue

        varName         = fieldDict[fieldName]['var_name']
        varType         = fieldDict[fieldName]['var_type']
        nsProperties    = fieldDict[fieldName]['ns_properties']
        numType         = "Integer"
        setCast         = "CFIndex"
        
        if varType == "uint8_t *":
            numType = "Data"
        elif varType == "double":
            numType = "Float"
            setCast = "IOHIDFloat"
        
        print "- (%s)%s {" % (varType, varName)
        print "    return (%s)IOHIDEventGet%sValue((__bridge IOHIDEventRef)self, %s);" % (varType, numType, fieldName)
        print "}\n"
        
        if "readonly" not in nsProperties:
            setName = varName[0].upper() + varName[1:]
            print "- (void)set%s:(%s)%s {" % (setName, varType, varName)
            print "    IOHIDEventSet%sValue((__bridge IOHIDEventRef)self, %s, (%s)%s);" % (
                numType, fieldName, setCast, varName)
            print "}\n"
    print "@end\n"

def GenEventAccessorHeaders(events, public):
    for eventName,event in events.items():
        objData = GenEventObjectData(event)
        if objData:
            GenEventHeader(objData, event, public)

def GenEventAccessors(events):
    for eventName,event in events.items():
        objData = GenEventObjectData(event)
        if objData:
            GenEventAccessor(objData, event)

def getFieldType(type):
    dataTypeToNativeTypeMap = {"IOHIDGenericGestureType" : "Integer", "IOFixed" : "IOFixed", "uint8_t" : "Integer",
        "IOHIDDouble" : "Double", "Boolean" : "Integer", "boolean_t" : "Integer", "uint32_t" : "Integer" , "IOHIDEventColorSpace" : "Integer" , "IOHIDGestureMotion" : "Integer" , "uint64_t" : "Integer" , "uint16_t" : "Integer", "IOHIDGestureFlavor" : "Integer", "IOHIDSwipeMask" : "Integer"
        }
    if type in dataTypeToNativeTypeMap.keys():
        return dataTypeToNativeTypeMap[type]

    return "*"

def GenFieldsDescHeader(events):
    print "%s" % (copyright)
    print "//DO NOT EDIT THIS FILE. FILE AUTO-GENERATED\n\n"
    print "#include <IOKit/hid/IOHIDEvent.h>"
    print "#include <HID/HIDEvent.h>\n\n"
    eventFieldDataTypes = ["None","Integer", "Double", "Float", "IOFixed"]
    print "enum {\n"
    for dataType in eventFieldDataTypes:
        print "\tkEventFieldDataType_%s," % (dataType)
    print "};\n"
    print "typedef struct {"
    print "\tIOHIDEventField field;"
    print "\tuint8_t         fieldType:6;"
    print "\tuint8_t         base:1;"
    print "\tuint8_t         readonly:1;"
    print "\tchar *          name;"
    print "} HIDEventFieldInfo;\n"
    print "typedef struct {"
    print "\tIOHIDEventField    value;"
    print "\tHIDEventFieldInfo *eventFieldDescTable;\n"
    print "} HIDEventFieldDescSelectorTable;"
    print "typedef struct  {"
    print "\tIOHIDEventField                 value;"
    print "\tHIDEventFieldDescSelectorTable *selectorTables;"
    print "} HIDSelectorTable;\n"
    print "typedef struct {"
    print "\tIOHIDEventType     type;"
    print "\tHIDEventFieldInfo *eventFieldDescTable;"
    print "\tHIDSelectorTable  *selectors;"
    print "} HIDEventFieldDescTableCollection;\n"

def GenFieldsDesc(events):
    print "%s" % (copyright)
    print "//DO NOT EDIT THIS FILE. FILE AUTO-GENERATED\n\n"
    print "#include <IOKit/hid/IOHIDEvent.h>"
    print "#include <HID/HIDEvent.h>\n\n"
    
    #get selector events
    eventsWithSelector = set([])
    eventSelectorData = {}

    for eventName,event in events.items():
        if hasDeclaration(event):
            continue
        fieldData = []
        selectorValueList = {}
        otherFieldData = []
        GetSelectorFieldData(event, fieldData, eventsWithSelector, selectorValueList, otherFieldData)
        if len(selectorValueList) > 0:
            if eventName not in eventSelectorData.keys() and len(fieldData) > 0:
                tmp = {}
                tmp['size'] = len(selectorValueList)
                eventSelectorData[eventName] = tmp
            for selector in selectorValueList.keys():
                for selectorValueKey in selectorValueList[selector].keys():
                    print "static HIDEventFieldInfo %s%s%sEventField[] = {" % (eventName, selector, selectorValueKey)
                    for fieldDict in otherFieldData:
                        print "\t{ %s, kEventFieldDataType_%s, %d, %d, \"%s\" }," % (fieldDict['name'], getFieldType(fieldDict['type']), fieldDict['base'], fieldDict['readonly'], fieldDict['name'][len("kIOHIDEventField"):])
                    for fieldDict in selectorValueList[selector][selectorValueKey]:
                        print "\t{ %s, kEventFieldDataType_%s, %d, %d,  \"%s\" }," % (fieldDict['name'], getFieldType(fieldDict['type']),  fieldDict['base'], fieldDict['readonly'], fieldDict['name'][len("kIOHIDEventField"):])
                    print "\t{ 0, kEventFieldDataType_None, 0, 0, NULL }"
                    print "};\n"

            for selector in selectorValueList.keys():
                print "static HIDEventFieldDescSelectorTable %s%sHIDEventFieldSelectorTable[] = {" % (eventName, selector)
                for selectorValueKey in selectorValueList[selector].keys():
                    print "\t{ %s, %s%s%sEventField }," % (selectorValueKey, eventName, selector, selectorValueKey)
                print "\t{ 0, NULL }"
                print "};\n"
                
            print "static HIDSelectorTable %sHIDSelectorTable[] = {" % (eventName)
            for selector in selectorValueList.keys():
                print "\t{ %s, %s%sHIDEventFieldSelectorTable }," % (selector, eventName, selector)
            print "\t{ 0, NULL }"
            print "};\n"

    for eventName,event in events.items():
        if hasDeclaration(event) or eventName in eventsWithSelector:
            continue
        
        fieldData = GenFieldsData(event)
        if fieldData:
            GenObjectFields(fieldData,event)

    count = 0
    print "\nstatic HIDEventFieldDescTableCollection hidEventFieldDescTable[] = {"
    for eventName,event in events.items():
        if hasDeclaration(event):
            continue
        fieldData = GenFieldsData(event)
        if fieldData:
            if eventName in eventsWithSelector:
                print "\t{ kIOHIDEventType%s, NULL, %sHIDSelectorTable }," % (eventName, eventName )
            else:
                print "\t{ kIOHIDEventType%s, %sEventFields, NULL }," % (eventName, eventName )
            count = count + 1
    print "\t{ kIOHIDEventTypeCount, NULL, NULL }"
    print "};\n"


def main(argv):
    file = None
    type = None
    name = None
    opts, args = getopt.getopt(argv, "f:t:e:", ["file=", "type=", "event="])

    for opt, arg in opts:
        if opt in ("-f", "--file"):
            file = arg
        if opt in ("-t", "--type"):
            type = arg
        if opt in ("-e", "--event"):
            name = arg

    if file:
        #f = open(file, 'r')
        #j = f.read()
        
        events  = plistlib.readPlist(file)
        #events = json.loads(j, object_pairs_hook=OrderedDict)
        ProcessEvents(events)

        if name:
            event = GetEventForName(events, name)
            events = [event]

        if type == "struct":
            GenStructs(events)
        elif type == "macro":
            GenAccessMacro(events)
        elif type == "fields":
            GenFieldsDefs(events)
        elif type == "objectHeaders":
            GenHeaders(events)
        elif type == "objects":
            GenObjects(events)
        elif type == "eventAccessorHeaders":
            GenEventAccessorHeaders(events, True)
        elif type == "eventAccessorHeadersPrivate":
            GenEventAccessorHeaders(events, False)
        elif type == "eventAccessors":
            GenEventAccessors(events)
        elif type == "fieldsDescHeader":
            GenFieldsDescHeader(events)
        elif type == "fieldsDesc":
            GenFieldsDesc(events)


if __name__ == "__main__":
    main(sys.argv[1:])
