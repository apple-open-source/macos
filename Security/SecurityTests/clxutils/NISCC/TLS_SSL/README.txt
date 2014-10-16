                      Testing the NISCC SSL Certs
				   Last Update 12 Nov 2003 by dmitch
				
Introduction
------------
Per 3479950, the British NISCC has discovered a vulnerability in almost
all existing SSL implementation which can result in a Denial of Service
attack when certain badly formatted DER elements are sent by the client
the the server. Specifically the badly formatted element is the client
cert. NISCC has provided a huge array of test certs to verify this 
problem. They've distrubuted this set of certs on a CD.

This directory contains some of that CD, plus some X-specific tools
to test and verify. The bulk of the CD is in the form of 6 large tarred
and zipped bundles of bad certs. Those .tar.gz files are not in the 
CVS repositoty because they are so big. When those files are expanded,
they result in hundreds of thousands of (bad) certs. Those certs are
not in the CVS repository either.

Details of the failures we're testing for
-----------------------------------------
As of this writing, there are three specific bugs in the SecurityNssAsn1
library which can all cause crashes when an app attempts to decode
certain badly formatted DER. All bugs are in 
SecurityNssAsn1/nssDER/Source/secasn1d.c.

First, when doing an SEC_ASN1_SAVE operation (e.g., saving the 
still-encoded subject and issuer names in NSS_TBSCertificate.derIssuer and
NSS_TBSCertificate.derSubject), the code allocates a SECItem for the
whole blob solely based upon the length of the blob indicated in its
enclosing DER sequence or set. However when traversing the actual bits
being saved, each element is copied to the pre-allocated buffer according
to the length field of that element. Corruption and crash can result if
those inner length fields are bad and end up adding up to a size larger than
the preallocated SECItem buffer. The solution is to track the allocated size
of the buffer in sec_asn1d_state_struct.dest_alloc_len, which gets inherited
from parent to child state as does the dest field itself. Whenever an item
is appended to the dest SECItem, possible overflow is checked and the op 
aborts if an overflow would result. 

Second, the sec_asn1d_reuse_encoding() routine is called after a 
(successful) SEC_ASN1_SAVE op to "back up" to the forked location and 
resume decoding it "for real", using the saved-off buffer - NOT the 
caller's input. There was a bug here in that a "needBytes" error in the 
sec_asn1d_reuse_encoding()'s call to SEC_ASN1DecoderUpdate() was ignored
and thrown out by the calling SEC_ASN1DecoderUpdate(), and processing 
proceeds, with possibly hazardous and unpredicatable results. However 
a "needBytes" error in the SEC_ASN1DecoderUpdate actually
called by sec_asn1d_reuse_encoding() must be fatal since all the data 
is already present - we're not streaming when that update is called 
because all of the data is already present in the saved-off buffer. The
solution is for sec_asn1d_reuse_encoding to detect the needBytes status and 
convert it into decodeError, thus aborting the caller immediately. (Note
that this generally did not result in a crash, but in undetected decoding
errors.) 

The third bug involved the behavior of the decoding engine if incoming
encoded data claimed to have a very large length. Two problems can occur
in sec_asn1d_prepare_for_contents() in such a case. First of course is the 
result of trying to malloc the large size. If state->contents_length is
2**32-1, for example, that malloc will almost certainly either fail or
take much longer than is appropriate. Then there is some arithmetic
involving appending subitems to the alloc_len which can result in 
integer overflow:

		for (subitem = state->subitems_head;
		     subitem != NULL; subitem = subitem->next)
		    alloc_len += subitem->len;
	    }

This bug is avoided by placing a somewhat arbitrary, but perfectly reasonable,
restriction in sec_asn1d_parse_more_length() - the routine which parses
such a huge length - that a 32-bit length value with the m.s. bit set is
invalid. 


Testing overview
----------------
There are two flavors of testing provided here. One uses a custom SSL 
client, nisccSimpleClient, which performs actual SSL transactions with 
an SSL server. The SSL server uses a good cert and requires client 
authentication. The SSL client uses a bad cert. Both are based on
SecureTransport. A failure is indicated by the server crashing and 
failing to respond to any more client requests. 

The other method of testing focusses exlusively on the failure mode, 
which is the decoding and parsing of the bad certs. (The gross failure
mode in an SSL server noted in the previous paragraph is always caused
by the server crashing during the decoding of the client cert.) This testing
is performed by a program called certDecode which simply attempts to decode
every cert in cwd. This is way, way faster than setting up actual SSL
clients and doing SSL transactions. As a result, the entire suite of 
"bad" client certs provided by NISCC (about 200,000 certs) has in fact 
been verified by this program. Resources to perform 200,000 SSL client 
trasnactions have not been marshalled as of this writing. In the opinion 
of this author, simply verifying that a process can attempt to decode the
bad certs, without crashing, is sufficient for verifying that the 
problem has been solved. 

Building the test programs
--------------------------
The nisccSimpleClient program requires the presence of both the 
clxutils/clAppUtils directory and the library it builds. The other 
programs - certDecode and skipThisNisccCert - just link against
libCdsaUtils.a. None of these build in a way which is compatible with 
Jasper Makefiles or PB project files. All you need to do is to set the 
LOCAL_BUILD_DIR environment variable to point to the place where the 
executables go. YOu can build each program individually by cd'ing to 
the program directory and typing 'make', or just do a 'make' from 
here (which is assumed to be clxutils/NISCC/TLS_SSL), which builds
all three. 

Testing using certDecode
------------------------
The certDecode program is a standalone executable which uses the 
SecurityNssAsn1 library to simply attempt to decode either every cert
in cwd, or the single cert specified in the cmd line. Build it by 
doing 'make' in its directory. You must have the $LOCAL_BUILD_DIR 
environment set.

You need to obtain and untar one of the NISCC cert bundles. The two
of main concern are simple_client.tar.gz and resigned_client.tar.gz.
Each one of these explodes into about 100,000 certs taking up about
200 MB of disk space. To run the test, cd to the directory containing 
100,000 certs and just run "certDecode" with no args. THe result will look
something like this:

  % tower.local:simple_client> certDecode
   ....00000001...00000002...00000003...00000004...00000005...
   00000006...00000007...00000008...00000009...00000010...00000011...
   00000012...00000013...00000014...00000015...00000016...00000017...
   ...etc....
   
It takes about 30 minutes to run thru all 100000 certs. Two things 
will happen: either the program will crash, or you'll see this at the 
end:

   00106148...00106149...00106150...00106151...00106152...00106153...
   00106154...00106155...00106156...00106157...00106158...00106159...
   00106160
   certDecode did not crash.
  %
  
Test using nisccSimpleClient
----------------------------

WARNING this hasn't been tested in a long time (as of 7/18/06). The 
nisccSimpleClient builds as of this date but the status of rest of this
is unknown. Stick to the certDecode test, above, which was verified as
of 7/18/06.

This is much more complicated and takes way longer than the certDecode
test - so long that I still haven't run it with all 200,000 NISCC certs. 
But to get started here's what you need to do.

First you need to build the sslServer program, in clxutils/sslServer. See
the README in clxutils for build instructions. 

You also need the cspxutils/dbTool program. Your PATH variable needs
to include the directory where its executable lives (generally, this is 
the same as your LOCAL_BUILD_DIR env var.)

Then you need to build a custom Security.framework because certain errors
introduced by this test will cause the stock SecureTransport library to
(properly) abort client-side transactions before you get started due to 
badly formatted certs. The tag for this Security.framework NISCC_ASN1; 
that's a branch off of the PantherGM Security tree. Build the tree and 
either install it or make sure your DYLD_FRAMEWORK_PATH env var points 
to it for all subsequent testing. 

Now set up the SSL server keychain using the (good) NISCC-supplied server 
cert and key. In the NISCC/TLS_SSL/testcases directory do the following:

   % rm -f ~/Library/Keychains/nisccServer
   % certtool i server_crt.pem k=nisccServer r=server_key.pem c

Run the sslServer app from the NISCC/TLS_SSL/testcases directory:

   % sslServer l k=nisccServer P=1200 a rootca.crt u=t
   
The "a rootca.crt" tells SecureTransport that the cert which signed the
server's cert is a trusted root cert. The "u=t" tells the server to 
request client authentication. If all is well, this program just keeps
on running, serving SSL requests, spewing forth to stdout (do not have
an unlimited scrollback buffer in your Terminal window or your root disk
will fill up.) 

Assuming that you'cve cd'd to the directory containing the nisccSslTest 
script (clxutils/NISCC/TLS_SSL/) and that the directory containing the 
untarred simple_client certs is in ./testcases/simple_client, just run 
the nisccSslTest script with one argument, the port number you supplied
to the sslServer program above:

   % nisccSslTest 1200
   
This assumes that the following executables are accessible via your 
PATH variable: dbTool, nisccSimpleClient, and skipThisNisccCert. 

When this is running OK you'll see an endless spew on stdout like this:

   cert 00001012...
   ...DB /Volumes/Data_and_Apps/home/dmitch/Library/Keychains/nisccClient wiped clean
   Starting nisccSimpleClient; args: localhost 1200 nisccClient 
   ===== nisccSimpleClient test PASSED =====
   cert 00001013...
   ...DB /Volumes/Data_and_Apps/home/dmitch/Library/Keychains/nisccClient wiped clean
   Starting nisccSimpleClient; args: localhost 1200 nisccClient 
   ===== nisccSimpleClient test PASSED =====
   cert 00001014...
   ...DB /Volumes/Data_and_Apps/home/dmitch/Library/Keychains/nisccClient wiped clean
   Starting nisccSimpleClient; args: localhost 13 1200 01 nisccClient 

...again, do not have an unlimited scrollback buffer in your Terminal 
window or your root disk will fill up.
