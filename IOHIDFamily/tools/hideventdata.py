import os, sys, getopt, re, subprocess, random, time, json
from collections import OrderedDict


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
    print "    %s base;" % (typename)
    if 'fields' in event.keys():
        GenEventFieldTypeDefs(indent, event, 'fields')
    print "} __%s;" % (typename)

#
# Generate event data structs
#
def GenStructs(events):
    for event in events:
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
    for event in events:
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
            print "#define kIOHIDEventField%sBase IOHIDEventFieldBase(kIOHIDEventType%s)" % (
                event['name'], event['name'])
                
                #print "typedef IOHID_ENUM (IOHIDEventField, IOHIDEvent%sFieldType) {" % (event['name'])
            for filedNameString in fieldDict.keys():
                print "static const IOHIDEventField %-58s        =  (kIOHIDEventField%sBase | %d);" % (
                    filedNameString, event['name'], fieldDict[filedNameString])
            #print "};"
            print ""


#
# Link fileds hierarchy in lists so each child has reference to parent
#
def ProcessEvents(events):
    for event in events:
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
                ProcessEventData(child)


#
# Gen field access macro
#
def GenAccessMacro(events):
    valueTypes = ["CFIndex", "double", "IOFixed", "data"]
    for event in events:
        if hasDeclaration(event) or event['name'] == "":
            continue
        for valueType in valueTypes:
            GenEventAccessMacro(event, valueType, "setter")
            GenEventAccessMacro(event, valueType, "getter")
    print ""
    for valueType in valueTypes:
        for event in events:
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
        print "#define IOHIDEventSet%sFieldsMacro(event, field) \\" % (
            GetTypeInfo(valueType).title())
        macroString = ""
        for event in events:
            if hasDeclaration(event) or event['name'] == "":
                continue
            if macroString != "":
                macroString = macroString + "\\\n"
            macroString = macroString + "     _IOHID%sSetFieldsAs%sMacro(event, field)" % (
                GetEventTypeName(event), GetTypeInfo(valueType).title())
        print macroString + "\n"
    print ""
    for valueType in valueTypes:
        print "#define IOHIDEventGet%sFieldsMacro(event, field) \\" % (
            GetTypeInfo(valueType).title())
        macroString = ""
        for event in events:
            if hasDeclaration(event) or event['name'] == "":
                continue
            if macroString != "":
                macroString = macroString + "\\\n"
            macroString = macroString + "    _IOHID%sGetFieldsAs%sMacro(event, field) " % (
                GetEventTypeName(event), GetTypeInfo(valueType).title())
        print macroString + "\n"

    macroString = "#define IOHIDEventGetBaseSize(type, size) \\\n"
    macroString += "    switch(type) {\\\n"
    for event in events:
        if hasDeclaration(event) or event['name'] == "":
            continue
        typename = "IOHID%sEventData" % (event['name'])
        macroString += "      case kIOHIDEventType%s:\\\n" % (
            GetEventTypeName(event))
        macroString += "          size = sizeof (%s);\\\n" % (typename)
        macroString += "          break; \\\n"
    macroString += "      default:\\\n"
    macroString += "          size = 0;\\\n"
    macroString += "    }\n"
    print macroString + "\n"

    macroString = "#define IOHIDEventGetSize(type, size) \\\n"
    macroString += "    switch(type) {\\\n"
    for event in events:
        if hasDeclaration(event) or event['name'] == "":
            continue
        typename = "IOHID%sEventData" % (event['name'])
        macroString += "      case kIOHIDEventType%s:\\\n" % (
            GetEventTypeName(event))
        macroString += "          size = sizeof (__%s);\\\n" % (typename)
        macroString += "          break; \\\n"
    macroString += "      default:\\\n"
    macroString += "          size = 0;\\\n"
    macroString += "    }\n"
    print macroString + "\n"

def GenEventFieldAccessSetMacro(event, field, valueType, indent):
    fieldMacroString = ''
    fieldName = GetEventFieldNameString(field)
    fieldAccess = GetEventFieldAccessString(field)
    fieldTypeCast = GetEventTypecastValueToFieldMacroString(field, valueType)
    fieldMacroString += "%scase %s: \\\n" % (indent * ' ', fieldName)

    eventTypeName = "IOHID%sEventData" % (event['name'])
    if field['base'] == False:
        eventTypeName = "__" + eventTypeName

    if 'length' in field.keys() and field['length'] == 1:
        fieldMacroString += "%s    ((%s*)event)->%s = value ? 1 : 0; \\\n" % (
            indent * ' ', eventTypeName, fieldAccess)
    elif field['kind'] == 'array':
        fieldMacroString += "%s    *(typeof(value)*)(((%s*)event)->%s) = value; \\\n" % (
            indent * ' ', eventTypeName, fieldAccess)
    else:
        fieldMacroString += "%s    ((%s*)event)->%s = (typeof(((%s*)event)->%s)) %s(value); \\\n" % (
            indent * ' ', eventTypeName, fieldAccess, eventTypeName,
            fieldAccess, fieldTypeCast)
    fieldMacroString = fieldMacroString + "%s    break; \\\n" % (indent * ' ')
    return fieldMacroString


def GenEventFieldAccessGetMacro(event, field, valueType, indent):
    fieldMacroString = ''
    fieldName = GetEventFieldNameString(field)
    fieldAccess = GetEventFieldAccessString(field)
    fieldTypeCast = GetEventTypecastFieldToTypeMacroString(field, valueType)
    fieldMacroString += "%scase %s: \\\n" % (indent * ' ', fieldName)
    eventTypeName = "IOHID%sEventData" % (event['name'])
    if field['base'] == False:
        eventTypeName = "__" + eventTypeName
    
    if field['kind'] == 'array':
        fieldMacroString += "%s    value = *(typeof(value)*)((%s*)event)->%s; \\\n" % (
            indent * ' ', eventTypeName, fieldAccess)
    else:
        fieldMacroString += "%s    value = (typeof(value))%s(((%s*)event)->%s); \\\n" % (
            indent * ' ', fieldTypeCast, eventTypeName, fieldAccess)
    fieldMacroString += "%s    break; \\\n" % (indent * ' ')
    return fieldMacroString


def GenEventDataAccessMacro(event, valueType, macroType):
    valid = False
    fields = []
    fieldsWithSelector = {}
    GetEventFieldsList(event, fields)
    fieldMacroString = ""
    indent = 8
    if macroType == "setter":
        fieldMacroString += "#define _IOHID%sSetFieldsAs%sMacro(event, field) \\\n" % (
            GetEventTypeName(event), GetTypeInfo(valueType).title())
    else:
        fieldMacroString += "#define _IOHID%sGetFieldsAs%sMacro(event, field) \\\n" % (
            GetEventTypeName(event), GetTypeInfo(valueType).title())

    fieldMacroString += "case kIOHIDEventType%s:\\\n" % (
        GetEventTypeName(event))
    fieldMacroString += "{\\\n"
    fieldMacroString += "    switch (field)\\\n"
    fieldMacroString += "    {\\\n"
    for field in fields:
        if hasDefinition(field):
            continue

        if field['kind'] != 'array' or macroType == 'setter':
            continue

        fieldName = GetEventFieldNameString(field)
        fieldAccess = GetEventFieldAccessString(field)
        fieldMacroString += "%scase %s: \\\n" % (indent * ' ', fieldName)
        fieldMacroString += "%s    value = (typeof(value))(((IOHID%sEventData*)event)->%s); \\\n" % (
            indent * ' ', event['name'], fieldAccess)
        valid = True

    if macroType == "setter":
        fieldMacroString += "        _IOHID%sSetSynthesizedFieldsAs%sMacro(event,field) \\\n" % (
            GetEventTypeName(event), GetTypeInfo(valueType).title())
    else:
        fieldMacroString += "        _IOHID%sGetSynthesizedFieldsAs%sMacro(event,field) \\\n" % (
            GetEventTypeName(event), GetTypeInfo(valueType).title())

    fieldMacroString += "    }\\\n"
    fieldMacroString += "    break;\\\n"
    fieldMacroString += "}\n"

    if not valid:
        if macroType == "setter":
            fieldMacroString = "#define _IOHID%sSetFieldsAs%sMacro(event, field) \\\n" % (
                GetEventTypeName(event), GetTypeInfo(valueType).title())
        else:
            fieldMacroString = "#define _IOHID%sGetFieldsAs%sMacro(event, field) \\\n" % (
                GetEventTypeName(event), GetTypeInfo(valueType).title())

    return fieldMacroString


def GenEventValueAccessMacro(event, valueType, macroType):
    fields = []
    fieldsWithSelector = {}
    GetEventFieldsList(event, fields)
    fieldMacroString = ""

    if macroType == "setter":
        fieldMacroString += "#define _IOHID%sSetFieldsAs%sMacro(event, field) \\\n" % (
            GetEventTypeName(event), GetTypeInfo(valueType).title())
    else:
        fieldMacroString += "#define _IOHID%sGetFieldsAs%sMacro(event, field) \\\n" % (
            GetEventTypeName(event), GetTypeInfo(valueType).title())

    fieldMacroString += "case kIOHIDEventType%s:\\\n" % (
        GetEventTypeName(event))
    fieldMacroString += "{\\\n"
    fieldMacroString += "    switch (field)\\\n"
    fieldMacroString += "    {\\\n"

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
            fieldMacroString += GenEventFieldAccessSetMacro(event, field,
                                                            valueType, 8)
        else:
            fieldMacroString += GenEventFieldAccessGetMacro(event, field,
                                                            valueType, 8)

    for selectorName in fieldsWithSelector.keys():
        fieldNameSet = set()
        for selectorValue in fieldsWithSelector[selectorName].keys():
            for field in fieldsWithSelector[selectorName][selectorValue]:
                fieldName = GetEventFieldNameString(field)
                fieldNameSet.add(fieldName)

        for fieldName in fieldNameSet:
            fieldMacroString += "%scase %s: \\\n" % (8 * ' ', fieldName)

        ind = 12 * ' '
        for selectorValue in fieldsWithSelector[selectorName].keys():
            selector = GetEventFieldSelector(
                fieldsWithSelector[selectorName][selectorValue][0])
            selectorAccess = GetEventFieldAccessString(selector)
            fieldMacroString += ind + "if (((IOHID%sEventData*)event)->%s == %s) {\\\n" % (
                event['name'], selectorAccess, selectorValue)
            fieldMacroString += ind + "    switch (field) \\\n"
            fieldMacroString += ind + "    {\\\n"
            for field in fieldsWithSelector[selectorName][selectorValue]:
                if macroType == "setter":
                    fieldMacroString += GenEventFieldAccessSetMacro(
                        event, field, valueType, 20)
                else:
                    fieldMacroString += GenEventFieldAccessGetMacro(
                        event, field, valueType, 20)
            fieldMacroString += ind + "    }\\\n"
            fieldMacroString += ind + "    break;\\\n"
            fieldMacroString += ind + "}\\\n"
        fieldMacroString += ind + 'break;\\\n'

    if macroType == "setter":
        fieldMacroString += "        _IOHID%sSetSynthesizedFieldsAs%sMacro(event,field) \\\n" % (
            GetEventTypeName(event), GetTypeInfo(valueType).title())
    else:
        fieldMacroString += "        _IOHID%sGetSynthesizedFieldsAs%sMacro(event,field) \\\n" % (
            GetEventTypeName(event), GetTypeInfo(valueType).title())

    fieldMacroString += "    }\\\n"
    fieldMacroString += "    break;\\\n"
    fieldMacroString += "}\n"
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
    for event in events:
        if event['name'] == name:
            return event
    return None

#
# create variable name for field
#
def GetVarName(field, fieldName, eventName):
    remStr = "kIOHIDEventField" + eventName
    nameString = ''
    if remStr in fieldName and "Length" not in fieldName:
        nameString = fieldName.replace(remStr, '')
    else:
        nameString = field['name']
    
    return nameString.lower()

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
    print "@interface HID%sEvent : HIDEvent\n" % (eventName)
    
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
    
    print "\n@end\n\n"

def GenObject(fieldDict, eventName):
    print "@implementation HID%sEvent\n" % (eventName)
	
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
            print "    return IOHIDEventGetDataValue(self->eventRef, %s);" % (fieldName)
        else:
            print "    return [NSNumber numberWith%s:IOHIDEventGet%sValue(self->eventRef, %s)];" % (
                nsSubType, nsSubType, fieldName)
        print "}\n"
        
        if nsSubType == "Array":
            print "- (NSString *)%sstr {" % (varName)
            print "    NSString *%sstr = [[NSString alloc] init];" % (varName)
            print "    uint8_t *%s = self.%s;\n" % (varName, varName)
            print "    for (uint32_t i = 0; i < self.length.unsignedIntValue; i++) {"
            print '        %sstr = [%sstr stringByAppendingString:[NSString stringWithFormat:@"%%02x ", %s[i]]];' % (
                    varName, varName, varName)
            print "    }"
            print "\n    return [%sstr substringToIndex:%sstr.length-1];" % (varName, varName)
            print "}\n"
        
        if "readonly" not in nsProperties:
            setName = varName[0].upper() + varName[1:]
            print "- (void)set%s:(%s *)%s {" % (setName, varType, varName)
            print "    IOHIDEventSet%sValue(self->eventRef, %s, %s.%sValue);" % (
                nsSubType, fieldName, varName, numType)
            print "}\n"
    
    print "- (NSString *)description {"
    print '    return [NSString stringWithFormat:@"%@ ',
    for fieldName in fieldDict.keys():
        varName         = fieldDict[fieldName]['var_name']
        
        if fieldName == fieldDict.keys()[-1]:
            sys.stdout.write("%s:%%@" % (varName))
        else:
            sys.stdout.write("%s:%%@ " % (varName))

    print '", [super description], ',
    for fieldName in fieldDict.keys():
        varName         = fieldDict[fieldName]['var_name']
        nsSubType       = fieldDict[fieldName]['ns_subtype']
        arrayStr        = ''
        
        if nsSubType == "Array":
            arrayStr = "str"
        
        if fieldName == fieldDict.keys()[-1]:
            sys.stdout.write("self.%s%s" % (varName, arrayStr))
        else:
            sys.stdout.write("self.%s%s, " % (varName, arrayStr))
    
    print "];\n}\n\n@end\n\n"

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

def GenHeaders(events):
    for event in events:
        objData = GenObjectData(event)
        if objData:
            GenHeader(objData, event['name'])

def GenObjects(events):
    for event in events:
        objData = GenObjectData(event)
        if objData:
            GenObject(objData, event['name'])

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
        f = open(file, 'r')
        j = f.read()
 
        events = json.loads(j, object_pairs_hook=OrderedDict)
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


if __name__ == "__main__":
    main(sys.argv[1:])
