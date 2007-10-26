The ReleaseNotes.xml file is some markup language used by techpubs
to generate the HTML and PDFs that go to customers.  If you hand
modify this file, make sure the case of your open and close tags
agrees or their tools will have problems with it.  

Use xmllint(1) to verify that the file is properly formed.  xmllint
doesn't know the markup language so it will not detect improper tag
usage or improper tag nesting.  (e.g. not all tags are allowed to
be children of other tags)

Sweet is the techpubs tool that you can use to update this file in
a GUI fashion.  If you save the file from within Sweet, it will
remove all of the hand-written whitespace formatting of ReleaseNotes.xml
that makes it easy to edit in vi/emacs right now.

Gutenberg is the techpubs tool that formats this XML file into HTML
or whatever delivery format is desired.

Rob Hammond was the contact at techpubs that asked us to switch to 
using Sweet/Gutenberg instead of sending HTML/plain text.  His
instructions included the document,

http://pubsbuild.apple.com/Messier/Releases/InternalDrafts/documentation/AppleInternal/Conceptual/Using_Sweet_for_RN/index.html

which is a Sweet User Guide for writing release notes.

I don't know what book.xml and gdb.gutenberg are, or if they are required.
Sweet spit them out when I started the initial document so I'm keeping
them for now.

Ideally, if you modify ReleaseNotes.xml, run it through xmllint,
open it in Sweet and generate HTML in Gutenberg.  If there are 
any mistakes, one of these tools should point it out.
