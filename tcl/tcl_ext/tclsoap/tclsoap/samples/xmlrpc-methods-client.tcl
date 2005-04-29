# Create the commands for my XMLRPC-domain package

package require XMLRPC
package require SOAP::http
set methods {}

lappend methods [ XMLRPC::create tclsoapTest1.rcsid \
		      -proxy http://localhost:8015/rpc/rcsid \
		      -params {} ]

lappend methods [ XMLRPC::create tclsoapTest1.base64 \
		      -proxy http://localhost:8015/rpc/base64 \
		      -params {msg string} ]

lappend methods [ XMLRPC::create tclsoapTest1.time \
		      -proxy http://localhost:8015/rpc/time \
		      -params {} ]

lappend methods [ XMLRPC::create tclsoapTest1.square \
		      -proxy http://localhost:8015/rpc/square \
		      -params {num integer} ]

lappend methods [ XMLRPC::create tclsoapTest1.sort \
		      -proxy http://localhost:8015/rpc/sort \
		      -params { list string } ]

lappend methods [ XMLRPC::create tclsoapTest1.platform \
		      -proxy http://localhost:8015/rpc/platform \
		      -params {} ]

puts "$methods"
unset methods
