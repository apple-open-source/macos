# cldr-inheritance

a tool to graph CLDR inheritance

## GraphViz

This script uses Graphviz.

- [GraphViz documentation](https://graphviz.org/documentation/)
- [Python wrapper documentation](https://graphviz.readthedocs.io/en/latest/manual.html)

## legend for output images

- solid line edge means explicit inheritance (defined in `supplementalData.xml`)
- dotted line edge means implicit inheritance (tokenize on `_`, remove last token)
- solid line node is explicitly defined in XML (in `icu/cldr/common/main`)
- dotted line node has no XML file (in `icu/cldr/common/main`)
- `∅` means there's no value for the key in the XML file

## example commands

Show the inheritance of all locales that contain `en`:

```sh
python3 ~/bin/cldr-inheritance.py icu --token en
```

Showing any locale with the token GB in it with the values for abbreviated width day period pm:

```sh
python3 ~/bin/cldr-inheritance.py icu --token GB --values
```

Note that for now I've got the key hardcoded (see To Do section below).

Show the results for a specific locale like this:

```sh
python3 ~/bin/cldr-inheritance.py icu --locale en_GB --values
```

Show the results for locales that contain `hi`:

```sh
python3 ~/bin/cldr-inheritance.py icu --token hi --values
```

## To Do

### add shortcuts for other values

For now I've got the key hardcoded because I really don't feel like typing:

```txt
'dates/calendars/calendar[@type="gregorian"]/dayPeriods/dayPeriodContext[@type="format"]/dayPeriodWidth[@type="abbreviated"]/dayPeriod[@type="pm"]'
```

on the command line. I need to think of a better way to handle this. Maybe short codes for keys we frequently need.

### sideways inheritance
This tool does not support sideways inheritance yet.

Rich wrote:
> If I remember right, you'll find <alias> elements in some of the resources.  I think they're all in root.  So a resource might inherit all the way to root and hit an alias, which causes the search to start over in a new resource in the original locale.  Worse, this can happen multiple times.

Here's an example I found in `root.xml`:

```xml
<dayPeriodWidth type="narrow">
    <alias source="locale" path="../dayPeriodWidth[@type='abbreviated']"/>
</dayPeriodWidth>
``````
