#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys
import re

POSIX_LIST = [
    'NEWLINE', 'Alpha', 'Blank', 'Cntrl', 'Digit', 'Graph', 'Lower',
    'Print', 'Punct', 'Space', 'Upper', 'XDigit', 'Word', 'Alnum', 'ASCII'
]

MAX_CODE_POINT = 0x10ffff

UD_FIRST_REG = re.compile("<.+,\s*First>")
UD_LAST_REG  = re.compile("<.+,\s*Last>")
PR_TOTAL_REG = re.compile("#\s*Total\s+code\s+points:")
PR_LINE_REG  = re.compile("([0-9A-Fa-f]+)(?:..([0-9A-Fa-f]+))?\s*;\s*(\w+)")
PA_LINE_REG  = re.compile("(\w+)\s*;\s*(\w+)")
PVA_LINE_REG = re.compile("(sc|gc)\s*;\s*(\w+)\s*;\s*(\w+)(?:\s*;\s*(\w+))?")
BL_LINE_REG  = re.compile("([0-9A-Fa-f]+)\.\.([0-9A-Fa-f]+)\s*;\s*(.*)")
VERSION_REG  = re.compile("#\s*.*-(\d\.\d\.\d)\.txt")

VERSION_INFO = None
DIC  = { }
KDIC = { }
PropIndex = { }
PROPERTY_NAME_MAX_LEN = 0

def normalize_prop_name(name):
    name = re.sub(r'[ _]', '', name)
    name = name.lower()
    return name

def fix_block_name(name):
    s = re.sub(r'[- ]+', '_', name)
    return 'In_' + s

def check_version_info(s):
    global VERSION_INFO
    m = VERSION_REG.match(s)
    if m is not None:
        VERSION_INFO = m.group(1)


def print_ranges(ranges):
    for (start, end) in ranges:
        print "0x%06x, 0x%06x" % (start, end)

    print len(ranges)

def print_prop_and_index(prop, i):
    print "%-35s %3d" % (prop + ',', i)
    PropIndex[prop] = i

print_cache = { }

def print_property(prop, data, desc):
    print ''
    print "/* PROPERTY: '%s': %s */" % (prop, desc)

    prev_prop = dic_find_by_value(print_cache, data)
    if prev_prop is not None:
        print "#define CR_%s CR_%s" % (prop, prev_prop)
    else:
        print_cache[prop] = data
        print "static const OnigCodePoint"
        print "CR_%s[] = { %d," % (prop, len(data))
        for (start, end) in data:
            print "0x%04x, 0x%04x," % (start, end)

        print "}; /* END of CR_%s */" % prop


def dic_find_by_value(dic, v):
    for key, val in dic.items():
        if val == v:
            return key

    return None


def normalize_ranges(in_ranges, sort=False):
    if sort:
        ranges = sorted(in_ranges)
    else:
        ranges = in_ranges

    r = []
    prev = None
    for (start, end) in ranges:
        if prev >= start - 1:
            (pstart, pend) = r.pop()
            end = max(pend, end)
            start = pstart

        r.append((start, end))
        prev = end

    return r

def inverse_ranges(in_ranges):
    r = []
    prev = 0x000000
    for (start, end) in in_ranges:
        if prev < start:
            r.append((prev, start - 1))

        prev = end + 1

    if prev < MAX_CODE_POINT:
        r.append((prev, MAX_CODE_POINT))

    return r

def add_ranges(r1, r2):
    r = r1 + r2
    return normalize_ranges(r, True)

def sub_one_range(one_range, rs):
    r = []
    (s1, e1) = one_range
    n = len(rs)
    for i in range(0, n):
        (s2, e2) = rs[i]
        if s2 >= s1 and s2 <= e1:
            if s2 > s1:
                r.append((s1, s2 - 1))
            if e2 >= e1:
                return r

            s1 = e2 + 1
        elif s2 < s1 and e2 >= s1:
            if e2 < e1:
                s1 = e2 + 1
            else:
                return r

    r.append((s1, e1))
    return r

def sub_ranges(r1, r2):
    r = []
    for one_range in r1:
        rs = sub_one_range(one_range, r2)
        r.extend(rs)

    return r

def add_ranges_in_dic(dic):
    r = []
    for k, v in dic.items():
        r = r + v

    return normalize_ranges(r, True)

def normalize_ranges_in_dic(dic, sort=False):
    for k, v in dic.items():
        r = normalize_ranges(v, sort)
        dic[k] = r

def merge_dic(to_dic, from_dic):
    to_keys   = to_dic.keys()
    from_keys = from_dic.keys()
    common = list(set(to_keys) & set(from_keys))
    if len(common) != 0:
        print >> sys.stderr, "merge_dic: collision: %s" % sorted(common)

    to_dic.update(from_dic)

def merge_props(to_props, from_props):
    common = list(set(to_props) & set(from_props))
    if len(common) != 0:
        print >> sys.stderr, "merge_props: collision: %s" % sorted(common)

    to_props.extend(from_props)

def add_range_into_dic(dic, name, start, end):
    d = dic.get(name, None)
    if d is None:
        d = [(start, end)]
        dic[name] = d
    else:
        d.append((start, end))

def list_sub(a, b):
    x = set(a) - set(b)
    return list(x)


def parse_unicode_data_file(f):
    dic = { }
    assigned = []
    for line in f:
        s = line.strip()
        if len(s) == 0:
            continue
        if s[0] == '#':
            continue

        a = s.split(';')
        code = int(a[0], 16)
        desc = a[1]
        prop = a[2]
        if UD_FIRST_REG.match(desc) is not None:
            start = code
            end   = None
        elif UD_LAST_REG.match(desc) is not None:
            end = code
        else:
            start = end = code

        if end is not None:
            assigned.append((start, end))
            add_range_into_dic(dic, prop, start, end)
            if len(prop) == 2:
                add_range_into_dic(dic, prop[0:1], start, end)

    normalize_ranges_in_dic(dic)
    return dic, assigned

def parse_properties(path, klass):
    with open(path, 'r') as f:
        dic = { }
        prop = None
        props = []
        for line in f:
            s = line.strip()
            if len(s) == 0:
                continue

            if s[0] == '#':
                if VERSION_INFO is None:
                    check_version_info(s)

            m = PR_LINE_REG.match(s)
            if m:
                prop = m.group(3)
                if m.group(2):
                    start = int(m.group(1), 16)
                    end   = int(m.group(2), 16)
                    add_range_into_dic(dic, prop, start, end)
                else:
                    start = int(m.group(1), 16)
                    add_range_into_dic(dic, prop, start, start)

            elif PR_TOTAL_REG.match(s) is not None:
                KDIC[prop] = klass
                props.append(prop)

    normalize_ranges_in_dic(dic)
    return (dic, props)

def parse_property_aliases(path):
    a = { }
    with open(path, 'r') as f:
        for line in f:
            s = line.strip()
            if len(s) == 0:
                continue

            m = PA_LINE_REG.match(s)
            if not(m):
                continue

            if m.group(1) == m.group(2):
                continue

            a[m.group(1)] = m.group(2)

    return a

def parse_property_value_aliases(path):
    a = { }
    with open(path, 'r') as f:
        for line in f:
            s = line.strip()
            if len(s) == 0:
                continue

            m = PVA_LINE_REG.match(s)
            if not(m):
                continue

            cat = m.group(1)
            x2  = m.group(2)
            x3  = m.group(3)
            x4  = m.group(4)
            if cat == 'sc':
                if x2 != x3:
                    a[x2] = x3
                if x4 and x4 != x3:
                    a[x4] = x3
            else:
                if x2 != x3:
                    a[x3] = x2
                if x4 and x4 != x2:
                    a[x4] = x2

    return a

def parse_blocks(path):
    dic = { }
    blocks = []
    with open(path, 'r') as f:
        for line in f:
            s = line.strip()
            if len(s) == 0:
                continue

            m = BL_LINE_REG.match(s)
            if not(m):
                continue

            start = int(m.group(1), 16)
            end   = int(m.group(2), 16)
            block = fix_block_name(m.group(3))
            add_range_into_dic(dic, block, start, end)
            blocks.append(block)

    noblock = fix_block_name('No_Block')
    dic[noblock] = inverse_ranges(add_ranges_in_dic(dic))
    blocks.append(noblock)
    return dic, blocks

def add_primitive_props(assigned):
    DIC['Assigned'] = normalize_ranges(assigned)
    DIC['Any']     = [(0x000000, 0x10ffff)]
    DIC['ASCII']   = [(0x000000, 0x00007f)]
    DIC['NEWLINE'] = [(0x00000a, 0x00000a)]
    DIC['Cn'] = inverse_ranges(DIC['Assigned'])
    DIC['C'].extend(DIC['Cn'])
    DIC['C'] = normalize_ranges(DIC['C'], True)

    d = []
    d.extend(DIC['Ll'])
    d.extend(DIC['Lt'])
    d.extend(DIC['Lu'])
    DIC['LC'] = normalize_ranges(d, True)

def add_posix_props(dic):
    alnum = []
    alnum.extend(dic['Alphabetic'])
    alnum.extend(dic['Nd'])  # Nd == Decimal_Number
    alnum = normalize_ranges(alnum, True)

    blank = [(0x0009, 0x0009)]
    blank.extend(dic['Zs'])  # Zs == Space_Separator
    blank = normalize_ranges(blank, True)

    word = []
    word.extend(dic['Alphabetic'])
    word.extend(dic['M'])   # M == Mark
    word.extend(dic['Nd'])
    word.extend(dic['Pc'])  # Pc == Connector_Punctuation
    word = normalize_ranges(word, True)

    graph = sub_ranges(dic['Any'], dic['White_Space'])
    graph = sub_ranges(graph, dic['Cc'])
    graph = sub_ranges(graph, dic['Cs'])  # Cs == Surrogate
    graph = sub_ranges(graph, dic['Cn'])  # Cn == Unassigned
    graph = normalize_ranges(graph, True)

    p = []
    p.extend(graph)
    p.extend(dic['Zs'])
    p = normalize_ranges(p, True)

    dic['Alpha']  = dic['Alphabetic']
    dic['Upper']  = dic['Uppercase']
    dic['Lower']  = dic['Lowercase']
    dic['Punct']  = dic['P']  # P == Punctuation
    dic['Digit']  = dic['Nd']
    dic['XDigit'] = [(0x0030, 0x0039), (0x0041, 0x0046), (0x0061, 0x0066)]
    dic['Alnum']  = alnum
    dic['Space']  = dic['White_Space']
    dic['Blank']  = blank
    dic['Cntrl']  = dic['Cc']
    dic['Word']   = word
    dic['Graph']  = graph
    dic['Print']  = p


def set_max_prop_name(name):
    global PROPERTY_NAME_MAX_LEN
    n = len(name)
    if n > PROPERTY_NAME_MAX_LEN:
        PROPERTY_NAME_MAX_LEN = n

LIST_COUNTER = 1
def entry_prop_name(name, index):
    global LIST_COUNTER
    set_max_prop_name(name)
    if OUTPUT_LIST and index >= len(POSIX_LIST):
        print >> UPF, "%3d: %s" % (LIST_COUNTER, name)
        LIST_COUNTER += 1


### main ###
argv = sys.argv
argc = len(argv)

POSIX_ONLY = False
if argc >= 2:
    if argv[1] == '-posix':
        POSIX_ONLY = True

OUTPUT_LIST = not(POSIX_ONLY)

with open('UnicodeData.txt', 'r') as f:
    dic, assigned = parse_unicode_data_file(f)
    DIC = dic
    add_primitive_props(assigned)

PROPS = DIC.keys()
PROPS = list_sub(PROPS, POSIX_LIST)
PROPS = sorted(PROPS)

dic, props = parse_properties('DerivedCoreProperties.txt', 'Derived Property')
merge_dic(DIC, dic)
merge_props(PROPS, props)

dic, props = parse_properties('Scripts.txt', 'Script')
merge_dic(DIC, dic)
merge_props(PROPS, props)
DIC['Unknown'] = inverse_ranges(add_ranges_in_dic(dic))

dic, props = parse_properties('PropList.txt', 'Binary Property')
merge_dic(DIC, dic)
merge_props(PROPS, props)
PROPS.append('Unknown')
KDIC['Unknown'] = 'Script'

ALIASES = parse_property_aliases('PropertyAliases.txt')
a = parse_property_value_aliases('PropertyValueAliases.txt')
merge_dic(ALIASES, a)

dic, BLOCKS = parse_blocks('Blocks.txt')
merge_dic(DIC, dic)

add_posix_props(DIC)

s = '''%{
/* Generated by make_unicode_property_data.py. */
'''
print s
for prop in POSIX_LIST:
    print_property(prop, DIC[prop], "POSIX [[:%s:]]" % prop)

print ''

if not(POSIX_ONLY):
    for prop in PROPS:
        klass = KDIC.get(prop, None)
        if klass is None:
            n = len(prop)
            if n == 1:
                klass = 'Major Category'
            elif n == 2:
                klass = 'General Category'
            else:
                klass = '-'

        print_property(prop, DIC[prop], klass)

    for block in BLOCKS:
        print_property(block, DIC[block], 'Block')


print ''
print "static const OnigCodePoint*\nconst CodeRanges[] = {"

for prop in POSIX_LIST:
    print "  CR_%s," % prop

if not(POSIX_ONLY):
    for prop in PROPS:
        print "  CR_%s," % prop

    for prop in BLOCKS:
        print "  CR_%s," % prop

s = '''};
%}
struct PropertyNameCtype {
  char* name:
  int ctype;
};
%%
'''
sys.stdout.write(s)

if OUTPUT_LIST:
    UPF = open("UNICODE_PROPERTIES", "w")
    if VERSION_INFO is not None:
        print >> UPF, "Unicode Properties (from Unicode Version: %s)" % VERSION_INFO
        print >> UPF, ''

index = -1
for prop in POSIX_LIST:
  index += 1
  entry_prop_name(prop, index)
  prop = normalize_prop_name(prop)
  print_prop_and_index(prop, index)

if not(POSIX_ONLY):
    for prop in PROPS:
        index += 1
        entry_prop_name(prop, index)
        prop = normalize_prop_name(prop)
        print_prop_and_index(prop, index)

    NALIASES = map(lambda (k,v):(normalize_prop_name(k), k, v), ALIASES.items())
    NALIASES = sorted(NALIASES)
    for (nk, k, v) in NALIASES:
        nv = normalize_prop_name(v)
        if PropIndex.get(nk, None) is not None:
            print >> sys.stderr, "ALIASES: already exists: %s => %s" % (k, v)
            continue
        index = PropIndex.get(nv, None)
        if index is None:
            #print >> sys.stderr, "ALIASES: value is not exist: %s => %s" % (k, v)
            continue

        entry_prop_name(k, index)
        print_prop_and_index(nk, index)

    for name in BLOCKS:
        index += 1
        entry_prop_name(name, index)
        name = normalize_prop_name(name)
        print_prop_and_index(name, index)

print '%%'
print ''
if VERSION_INFO is not None:
    print "#define PROPERTY_VERSION  %s" % re.sub(r'[\.-]', '_', VERSION_INFO)
    print ''

print "#define PROPERTY_NAME_MAX_SIZE  %d" % (PROPERTY_NAME_MAX_LEN + 10)
print "#define CODE_RANGES_NUM         %d" % (index + 1)

if OUTPUT_LIST:
    UPF.close()

sys.exit(0)
