use strict;

print "1..1\n";

if (!eval { require Module::Signature; 1 })
{
    print "ok 1 # skip ",
          "Next time around, consider installing Module::Signature, ",
          "so you can verify the integrity of this distribution.\n";
} 
elsif (!eval { require Socket; Socket::inet_aton('pgp.mit.edu') })
{
    print "ok 1 # skip ",
          "Cannot connect to the keyserver\n";
}
else
{
   print "not " unless Module::Signature::verify() == Module::Signature::SIGNATURE_OK();
   print "ok 1 # Valid signature\n";
}
