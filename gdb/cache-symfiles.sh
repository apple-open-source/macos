#! /bin/sh

if [ `id -u` != "0" ]; then
    echo "This program must be run as root."
    exit 1
fi

dir=/usr/libexec/gdb/symfiles

echo -n "Removing current cache ... "
rm -rf "$dir"
mkdir -p "$dir"
echo "done"

cat > /tmp/syms_$$.c <<EOF
int main ()
{
}
EOF

libs="-F/System/Library/PrivateFrameworks"

for i in `ls -d /System/Library/Frameworks/*.framework | grep -v Kernel`; do
    name=`basename $i .framework`
    libs="$libs -framework $name"
done 

for i in `ls -d /System/Library/PrivateFrameworks/*.framework | grep -v AppSupport | grep -v LogViewerAPI | grep -v PBRuntime | grep -v PrintService | grep -v SMBDefines`; do
    name=`basename $i .framework`
    libs="$libs -framework $name"
done 

for i in `ls /usr/lib/lib*.dylib | grep -v _profile | grep -v _debug`; do
    name=`basename $i .dylib`
    name=`echo $name | sed -e 's/\.[ABC]$//' -e 's/^lib//'`
    libs="$libs -l$name"
done 

cc -g -o /tmp/syms_$$ /tmp/syms_$$.c $libs

cat > /tmp/syms_$$.gdb <<EOF
file /tmp/syms_$$
run
sharedlibrary cache-symfiles $dir
quit
EOF

cat /tmp/syms_$$.gdb | /usr/bin/gdb

rm -f "$dir/syms_$$.syms"
rm -f "/tmp/syms_$$.*"
