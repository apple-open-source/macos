from xml.etree import ElementTree
import argparse
import os
import graphviz

# legend
# ------
# solid line edge means explicit inheritance (defined in supplementalData.xml)
# dotted line edge means implicit inheritance (i.e. remove last token)
# solid line node is explicitly defined in XML (in icu/cldr/common/main)
# dotted line node has no XML file (in icu/cldr/common/main)
# '∅' means there's no value for the key in the XML file

# GraphViz documentation
# https://graphviz.org/documentation/
# https://graphviz.readthedocs.io/en/latest/manual.html
# https://graphviz.readthedocs.io/en/stable/examples.html


def get_locales_with_xml_files(path):
    # list all the locales in icu/cldr/common/main
    locales = set()
    for f in os.listdir(path + '/cldr/common/main'):
        if f.endswith('.xml'):
            locale = f[:-4]
            locales.add(locale)
    return locales


def get_explicit_parents(path):
    explicit_parents = dict()
    # parse supplementalData.xml
    with open(path + '/cldr/common/supplemental/supplementalData.xml') as f:
        tree = ElementTree.parse(f)
        root = tree.getroot()
        # parse parentLocales
        for parentLocales in root.iter('parentLocales'):
            # make sure component is not 'segmentations' or 'collations'
            component = parentLocales.attrib.get('component')
            if component in {'segmentations', 'collations'}:
                continue
            for parentLocale in parentLocales.iter('parentLocale'):
                parent = parentLocale.attrib['parent']
                locales = parentLocale.attrib['locales'].split(' ')
                for locale in locales:
                    explicit_parents[locale] = parent
    return explicit_parents


def get_implicit_parents(locales_with_xml_files, explicit_parents):
    implicit_parents = dict()
    for locale in locales_with_xml_files:
        if locale not in explicit_parents:
            if '_' in locale:
                parent = '_'.join(locale.split('_')[:-1])
            else:
                parent = 'root'
            implicit_parents[locale] = parent
    return implicit_parents


def get_missing_locales(locales_with_xml_files, explicit_parents, implicit_parents):
    missing_locales = set()
    referenced_locales = (
            set(explicit_parents.keys())
            | set(explicit_parents.values())
            | set(implicit_parents.keys())
            | set(implicit_parents.values()))
    for locale in referenced_locales:
        if locale not in locales_with_xml_files:
            missing_locales.add(locale)
    return missing_locales


def graph_everything(explicit_parents, implicit_parents, missing_locales):
    g = graphviz.Digraph('G', filename='CLDR_inheritance', engine='dot', format='png')
    g.attr(rankdir='LR')
    g.attr(overlap='false')

    g.attr('node', style='dotted')
    for locale in missing_locales:
        g.node(locale)
    g.attr('node', style='solid')

    for child in explicit_parents:
        parent = explicit_parents[child]
        g.edge(parent, child)

    for child in implicit_parents:
        parent = implicit_parents[child]
        g.edge(parent, child, style='dotted')

    g.view()


def graph_locale(explicit_parents, implicit_parents, missing_locales, locale, values):
    g = graphviz.Digraph('G', filename='CLDR_inheritance', engine='dot', format='png')
    g.attr(rankdir='LR')
    g.attr(overlap='false')

    parent = None
    child = locale
    edge_style = 'solid'
    while child != 'root':
        if child in explicit_parents:
            parent = explicit_parents[child]
            edge_style = 'solid'
        if child in implicit_parents:
            parent = implicit_parents[child]
            edge_style = 'dotted'
        if child in missing_locales:
            g.node(child, style='dotted')
        if parent in missing_locales:
            g.node(parent, style='dotted')
        if child in values:
            g.node(child, label=child + ': ' + values[child])
        if parent == 'root' and 'root' in values:
            g.node(parent, label=parent + ': ' + values[parent])
        g.edge(parent, child, style=edge_style)
        child = parent
    g.view()


def graph_token(explicit_parents, implicit_parents, missing_locales, token, values):
    g = graphviz.Digraph('G', filename='CLDR_inheritance', engine='dot', format='png')
    g.attr(rankdir='LR')
    g.attr(overlap='false')

    parent = None
    matching_locales = set()
    seen = set()
    all_locales = (
            set(explicit_parents.keys())
            | set(explicit_parents.values())
            | set(implicit_parents.keys())
            | set(implicit_parents.values())
            | missing_locales)
    for locale in all_locales:
        if token in locale.split('_'):
            matching_locales.add(locale)

    for child in matching_locales:
        edge_style = 'solid'
        while child != 'root':
            if child in seen:
                break
            if child in explicit_parents:
                parent = explicit_parents[child]
                edge_style = 'solid'
            if child in implicit_parents:
                parent = implicit_parents[child]
                edge_style = 'dotted'
            if child in missing_locales:
                g.node(child, style='dotted')
            if parent in missing_locales:
                g.node(parent, style='dotted')
            if child in values:
                g.node(child, label=child + ': ' + values[child])
            if parent == 'root' and 'root' in values:
                g.node(parent, label=parent + ': ' + values[parent])
            g.edge(parent, child, style=edge_style)
            seen.add(child)
            child = parent
    g.view()


def get_value(path, locale, key):
    file_path = path + '/cldr/common/main/' + locale + '.xml'
    with open(file_path) as f:
        tree = ElementTree.parse(f)
        root = tree.getroot()
        x = root.find(key)
        if x is None:
            return '∅'
        else:
            return x.text


def get_values(path, key):
    values = dict()
    for f in os.listdir(path + '/cldr/common/main'):
        if f.endswith('.xml'):
            locale = f[:-4]
            value = get_value(path, locale, key)
            values[locale] = value
            # print(locale, value)
    return values


def main():
    parser = argparse.ArgumentParser(description='check inheritance in CLDR')
    parser.add_argument('path', help='path to ICU')
    parser.add_argument('--locale', help='locale to check')
    parser.add_argument('--token', help='token to match')
    parser.add_argument('--values', help='show values (for hard-coded key)', action='store_true')
    args = parser.parse_args()
    path = args.path

    locales_with_xml_files = get_locales_with_xml_files(path)

    if args.values:
        key = ('dates/calendars/calendar[@type="gregorian"]/dayPeriods/dayPeriodContext[@type="format"]'
               '/dayPeriodWidth[@type="abbreviated"]/dayPeriod[@type="pm"]')
        values = get_values(path, key)
    else:
        values = {}

    explicit_parents = get_explicit_parents(path)
    implicit_parents = get_implicit_parents(locales_with_xml_files, explicit_parents)
    missing_locales = get_missing_locales(locales_with_xml_files, explicit_parents, implicit_parents)
    if args.locale is None and args.token is None:
        graph_everything(explicit_parents, implicit_parents, missing_locales)
    elif args.locale:
        graph_locale(explicit_parents, implicit_parents, missing_locales, args.locale, values)
    elif args.token:
        graph_token(explicit_parents, implicit_parents, missing_locales, args.token, values)


if __name__ == "__main__":
    main()
