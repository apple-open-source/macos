# makepro.sed by Mark Weaver <Mark_Weaver@brown.edu>
/^\/\*\*\/$/{
n
N
s/\n\([_a-zA-Z][_0-9a-zA-Z]* *\)\((.*\)$/ \1 _(\2);/
p
}
