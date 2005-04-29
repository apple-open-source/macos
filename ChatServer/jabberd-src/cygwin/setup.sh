# just copy all the cygwin-specifc files into place... ugly, but it works 

# copy in a whole new dnsrv
cp cygwin/dnsrv/* dnsrv/

# copy dll files into everywhere
#cp cygwin/dllfixup.c jabberd/

# hmm, I guess that's it?
# Not quite, create a new config file using .dll's instead of .so's
mv jabber.xml jabber.xml.orig
cat jabber.xml.orig | sed 's/\.so/\.dll/g' > jabber.xml
