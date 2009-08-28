# silab.tcl - Copyright (C) 2001 Pat Thoyts <Pat.Thoyts@bigfoot.com>
#
# Run the SOAP Interoperability Test Suite Round 2: Base tests.
# Generates a html page of the results.
#
# $Id: silab.tcl,v 1.8 2003/01/26 01:57:33 patthoyts Exp $

package require soapinterop::base
package require soapinterop::B
package require soapinterop::C

set logdir   {../doc/silab}
set logcount 0
set logfile  {}

#      toolkit          endpoint  action   namespace
# {{Apache 2.1}      http://nagoya.apache.org:5089/soap/servlet/rpcrouter  urn:soapinterop http://soapinterop.org/}
# {{SOAP.py}         http://208.177.157.221:9595/xmethodsInterop           urn:soapinterop http://soapinterop.org/}
set round1 {
    {{TclSOAP 1.6}     http://tclsoap.sourceforge.net/cgi-bin/rpc            urn:soapinterop http://soapinterop.org/}
    {{SOAP::Lite}      http://services.soaplite.com/interop.cgi              urn:soapinterop http://soapinterop.org/}
    {{4s4c}            http://soap.4s4c.com/interop/soap.asp                 urn:soapinterop http://soapinterop.org/}
    {{Frontier 7.0}    http://www.soapware.org/xmethodsInterop              /xmethodsInterop http://soapinterop.org/}
    {{ActiveState}     http://soaptest.activestate.com:8080/PerlEx/soap.plex urn:soapinterop http://soapinterop.org/}
    {{Dolphin Harbor}  http://www.dolphinharbor.org/services/interop         urn:soapinterop http://soapinterop.org/}
    {{EasySoap++}      http://www.xmethods.net/c/easysoap.cgi                urn:soapinterop http://soapinterop.org/}
    {{eSoapServer}     http://www.connecttel.com/cgi-bin/esoapserver.cgi     urn:soapinterop http://soapinterop.org/}
}

# round 2 http://www.whitemesa.com/interop.htm
#{{eSOAP}            http://212.177.97.8:8080/rpcrouter http://soapinterop.org/ http://soapinterop.org/}
#{{SOAP RMI}         http://www.extreme.indiana.edu:1568  http://soapinterop.org/ http://soapinterop.org/}
#{{kSOAP 0.8}        http://kissen.cs.uni-dortmind.de:8008               http://soapinterop.org/ http://soapinterop.org/}
set round2base {
    {{TclSOAP 1.6}      http://tclsoap.sourceforge.net/cgi-bin/rpc          http://soapinterop.org/ http://soapinterop.org/}
    {{SOAP::Lite}       http://services.soaplite.com/interop.cgi            http://soapinterop.org/ http://soapinterop.org/}
    {{ASP.NET}          http://www.mssoapinterop.org/asmx/simple.asmx       http://soapinterop.org/ http://soapinterop.org/}
    {{MS .NET Remoting} http://www.mssoapinterop.org/remoting/ServiceA.soap http://soapinterop.org/ http://soapinterop.org/}
    {{Spray}            http://www.dolphinharbor.org/services/interop       http://soapinterop.org/ http://soapinterop.org/}
    {{SQLData SOAP}     http://soapclient.com/interop/sqldatainterop.wsdl   http://soapinterop.org/ http://soapinterop.org/}
    {{SOAP4R}           http://www.jin.gr.jp/~nahi/Ruby/SOAP4R/SOAPBuildersInterop/ http://soapinterop.org/ http://soapinterop.org/}
    {{EasySoap++}       http://easysoap.sourceforge.net/cgi-bin/interopserver       http://soapinterop.org/ http://soapinterop.org/}
    {{SOAPx4 (PHP) .5}  http://dietrich.ganx4.com/soapx4/soap.php           http://soapinterop.org/ http://soapinterop.org/}
    {{White Mesa SOAP}  http://www.whitemesa.net/interop/std                http://soapinterop.org/ http://soapinterop.org/ {-encoding SOAP1.2}}
    {{SIM}              http://soapinterop.simdb.com/round2                 http://soapinterop.org/ http://soapinterop.org/}
    {{Spray 2001}      http://www.dolphinharbor.org/services/interop2001    http://soapinterop.org/ http://soapinterop.org/ {-encoding SOAP1.2}}
    {{4s4c}             http://soap.4s4c.com/ilab/soap.asp                  http://soapinterop.org/ http://soapinterop.org/}
    {{Apache Axis}      http://nagoya.apache.org:5049/axis/servlet/AxisServlet  http://soapinterop.org/ http://soapinterop.org/}
    {{Apache SOAP}      http://nagoya.apache.org:5049/soap/servlet/rpcrouter    http://soapinterop.org/ http://soapinterop.org/}
}

set round2B {
    {{TclSOAP 1.6}      http://tclsoap.sourceforge.net/cgi-bin/rpc          http://soapinterop.org/ http://soapinterop.org/}
    {{SOAP::Lite}      http://services.soaplite.com/interop.cgi             http://soapinterop.org/ http://soapinterop.org/}
    {{SOAP4R}          http://www.jin.gr.jp/~nahi/Ruby/SOAP4R/SOAPBuildersInterop/ http://soapinterop.org/ http://soapinterop.org/}
    {{SIM}             http://soapinterop.simdb.com/round2B                 http://soapinterop.org/ http://soapinterop.org/}
    {{Spray 2001}      http://www.dolphinharbor.org/services/interopB2001   http://soapinterop.org/ http://soapinterop.org/ {-encoding SOAP1.2}}
    {{EasySoap++}       http://easysoap.sourceforge.net/cgi-bin/interopserver  http://soapinterop.org/ http://soapinterop.org/}
    {{White Mesa SOAP}  http://www.whitemesa.net/interop/std/groupB         http://soapinterop.org/ http://soapinterop.org/ {-encoding SOAP1.2}}
    {{MS .NET Remoting} http://www.mssoapinterop.org/remoting/ServiceB.soap http://soapinterop.org/ http://soapinterop.org/ {-encoding SOAP1.2}}
    {{MS SOAP Toolkit} http://www.mssoapinterop.org/stk/InteropB.wsdl       http://soapinterop.org/ http://soapinterop.org/ {-encoding SOAP1.2}}
    {{ASP.NET}          http://www.mssoapinterop.org/asmx/simpleB.asmx      http://soapinterop.org/ http://soapinterop.org/ {-encoding SOAP1.2}}
    {{4s4c}            http://soap.4s4c.com/ilab/soap.asp                   http://soapinterop.org/ http://soapinterop.org/}
}

set round2C {
    {{TclSOAP 1.6}      http://tclsoap.sourceforge.net/cgi-bin/rpc          http://soapinterop.org/ http://soapinterop.org/}
    {{SOAP::Lite}       http://services.soaplite.com/interopC.cgi              http://soapinterop.org/ http://soapinterop.org/}
    {{White Mesa SOAP}  http://www.whitemesa.net/interop/std/echohdr           http://soapinterop.org/ http://soapinterop.org/ {-encoding SOAP1.2}}
    {{EasySoap++}       http://easysoap.sourceforge.net/cgi-bin/interopserver  http://soapinterop.org/ http://soapinterop.org/}
    {{MS SOAP Toolkit}  http://mssoapinterop.org/stk/InteropC.wsdl             http://soapinterop.org/ http://soapinterop.org/ {-encoding SOAP1.2}}
    {{Spray 2001}      http://www.dolphinharbor.org/services/interopC          http://soapinterop.org/ http://soapinterop.org/ {-encoding SOAP1.2}}
    {{ASP.NET}          http://www.mssoapinterop.org/asmx/header.asmx          http://soapinterop.org/ http://soapinterop.org/ {-encoding SOAP1.2}}
}

# -------------------------------------------------------------------------

proc silab:round1 {} {
    set title {SILAB Round 1 Tests called from TclSOAP}
    set info {
	This page is generated by a Tcl script that uses the TclSOAP client code
	against a list of SOAP servers implementing the SOAP Interoperability Lab
	Round 1 service. The results are printed as OK or the error is
	shown. The raw XML for the reqyest and the reply is available via links in
	each table. If there is no XML then the error occurred in the TclSOAP client
	code or the server is down.}
    set filename "round1.html"
    silab:execute $filename $title $info $::round1 proposalA
}

proc silab:round2 {} {
    set title {SILAB Round 2 Base Tests called via TclSOAP}
    set info {
	This page is generated by a Tcl script that uses the TclSOAP client code
	against a list of SOAP servers implementing the SOAP Interoperability Lab
	Round 2 proposal A service. The results are printed as OK or the error is
	shown. The raw XML for the request and the reply is available via links in
	each table. If there is no XML then the error occurred in the TclSOAP client
	code or the server is down.}
    set filename "round2base.html"
    silab:execute $filename $title $info $::round2base proposalA
}

# -------------------------------------------------------------------------

proc silab:round2B {} {
    set title {SILAB Round 2 Proposal B Tests called via TclSOAP}
    set info {
	This page is generated by a Tcl script that uses the TclSOAP client code
	against a list of SOAP servers implementing the SOAP Interoperability Lab
	Round 2 proposal B service. The results are printed as OK or the error is
	shown. The raw XML for the request and the reply is available via links in
	each table. If there is no XML then the error occurred in the TclSOAP client
	code or the server is down.}
    set filename "round2B.html"
    silab:execute $filename $title $info $::round2B proposalB
}

# -------------------------------------------------------------------------

proc silab:round2C {} {
    set title {SILAB Round 2 Proposal C Tests called via TclSOAP}
    set info {
	This page is generated by a Tcl script that uses the TclSOAP client code
	against a list of SOAP servers implementing the SOAP Interoperability Lab
	Round 2 proposal C service. The results are printed as OK or the error is
	shown. The raw XML for the request and the reply is available via links in
	each table. If there is no XML then the error occurred in the TclSOAP client
	code or the server is down.}
    set filename "round2C.html"
    silab:execute $filename $title $info $::round2C proposalC
}

# -------------------------------------------------------------------------

# procname - procedure to execute to perform the tests.

proc silab:execute {filename title info servers procname} {
    global logdir
    global logfile

    # set a global http timeout to avoid overruns of 30 seconds
    SOAP::configure -transport http -timeout 30000

    if {$logdir != {} && ![file isdirectory $logdir]} {
	file mkdir $logdir
    }
    if {$filename != {}} {
        set logfile [open [file join $logdir $filename] w]
    } else {
        set logfile stdout
    }

    puts $logfile "<!doctype HTML public \"-//W3O//DTD W3 HTML 2.0//EN\">\
	    <html><head><link href=\"../tclsoap.css\" rel=\"stylesheet\" type=\"text/css\">\
            <title>$title</title></head>"
    puts $logfile {
<body>
<table class="globaltable">
  <tr><td class="header" width="15%">
     <a href="http://sourceforge.net/">
        <img align="middle" alt="SourceForge Logo" border="0" height="31" width="88"
             src="http://sourceforge.net/sflogo.php?group_id=25970"></a>
 </td>
 <td class="header" width="70%">
    }
    puts $logfile "<h1>$title</h1>"
    puts $logfile {
      </td>
      <td class="logo" width="15%">
        <img src="../tclsoap.gif" alt="TclSOAP Logo" align="middle"
        border="0" height="84" width="57" />
      </td>
  </tr>

  <tr><td class="sidebar">
    <table>
      <tr><td class="sidehead">Results</td></tr>
    }
    
    # Draw the sidebar stuff
    foreach server $servers {
	puts $logfile "<tr><td class=\"sideelt\"><a href=\"#[lindex $server 0]\">[lindex $server 0]</a></td></tr>"
    }

    puts $logfile {
<tr><td class="sidehead">Project</td></tr>
<tr><td class="sideelt"><a href="http://sourceforge.net/projects/tclsoap">Project Page</a></td></tr>
<tr><td class="sideelt"><a href="http://sourceforge.net/project/showfiles.php?group_id=25970">File Releases</a></td></tr>
<tr><td class="sideelt"><a href="../TclSOAP.html">WebPage</a></td></tr>
<tr><td class="sideelt"><a href="http://sourceforge.net/docman/?group_id=25970">Documentation</a></td></tr>
<tr><td class="sideelt"><a href="http://sourceforge.net/forum/?group_id=25970">Forums</a></td></tr>
<tr><td class="sideelt"><a href="http://cvs.sourceforge.net/cgi-bin/viewcvs.cgi/tclsoap/tclsoap">Browse CVS</a></td></tr>
<tr><td class="sidehead">Interop</td></tr>
<tr><td class="sideelt"><a href="round1.html">Round 1 Tests</a></td></tr>
<tr><td class="sideelt"><a href="round2base.html">Round 2 Base</a></td></tr>
<tr><td class="sideelt"><a href="round2B.html">Round 2B</a></td></tr>
<tr><td class="sideelt"><a href="round2C.html">Round 2C</a></td></tr>
<tr><td class="sidehead">Support</td></tr>
<tr><td class="sideelt"><a href="http://sourceforge.net/tracker/?aid=385859&group_id=25970&func=browse">Bugs</a></td></tr>
<tr><td class="sideelt"><a href="http://sourceforge.net/tracker/?aid=385860&group_id=25970&func=browse">Support Requests</a></td></tr>
<tr><td class="sideelt"><A href="http://sourceforge.net/tracker/?aid=385861&group_id=25970&func=browse">Patches</a></td></tr>
<tr><td class="sideelt"><A href="http://sourceforge.net/tracker/?aid=385862&group_id=25970&func=browse">Feature Requests</a></td></tr>
    </table>
  </td>
  <td class="body" colspan="2">
  <div class="body">
    }

    puts $logfile "<p>$info</p><hr>"
    flush $logfile
    
    foreach server $servers {
	if {[catch {eval $procname \
			[list [lindex $server 0]] \
                        [list [lindex $server 1]] \
                        [list [lindex $server 2]] \
                        [list [lindex $server 3]] \
                        [lindex $server 4] \
                    } msg ]} {
	    puts $logfile "<font color=\"red\">\
                    <h3>An Error occurred processing [lindex $server 0]</h3>\
		    <p>$msg</p></font>"
	}
	flush $logfile
    }

    puts $logfile {
</div>
</td></tr>
<tr class="footer">
<td class="footer" colspan="3">
    }
    puts $logfile "Last Updated: [clock format [clock seconds]]"
    puts $logfile {
</td></tr>

    </table>
  </body>
</html>
    }

    close $logfile
}
    
# -------------------------------------------------------------------------
# Description:
#  Setup and run the base SOAP Interop Lab Client tests.
#
proc proposalA {toolkit where action xmlns args} {
    global logfile

    set soapinterop::uri $xmlns
    set soapinterop::action $action
    eval soapinterop::create:base [list $where] $args

    puts $logfile "<h2><a name=\"$toolkit\">$toolkit $where</a></h2>\n<table>\n"

    perform $toolkit soapinterop::validate.echoVoid
    perform $toolkit soapinterop::validate.echoDate
    perform $toolkit soapinterop::validate.echoBase64
    perform $toolkit soapinterop::validate.echoInteger
    perform $toolkit soapinterop::validate.echoFloat
    perform $toolkit soapinterop::validate.echoString
    perform $toolkit soapinterop::validate.echoIntegerArray
    perform $toolkit soapinterop::validate.echoFloatArray
    perform $toolkit soapinterop::validate.echoStringArray
    perform $toolkit soapinterop::validate.echoStruct
    perform $toolkit soapinterop::validate.echoStructArray

    puts $logfile "</table><hr>\n\n"
    return {}
}

# -------------------------------------------------------------------------
# Description:
#  Setup and run the base SOAP Interop Lab Client proposal B tests.
#
proc proposalB {toolkit where action xmlns args} {
    global logfile

    set soapinterop::uri $xmlns
    set soapinterop::action $action
    eval soapinterop::create:proposalB [list $where] $args

    puts $logfile "<h2><a name=\"$toolkit\">$toolkit $where</a></h2>\n<table>\n"

    perform $toolkit soapinterop::validate.echoStructAsSimpleTypes
    perform $toolkit soapinterop::validate.echoSimpleTypesAsStruct
    perform $toolkit soapinterop::validate.echoNestedArray
    perform $toolkit soapinterop::validate.echoNestedStruct
    perform $toolkit soapinterop::validate.echo2DStringArray

    puts $logfile "</table><hr>\n\n"
    return {}
}

# -------------------------------------------------------------------------
# Description:
#  Setup and run the Round 2 Proposal C tests (found in soapinteropC.tcl)
#
proc proposalC {toolkit where action xmlns args} {
    global logfile

    eval SOAP::create soapinterop::echoVoid \
        -name echoVoid \
	-proxy  [list $where] \
        -uri    [list $xmlns] \
        -action [list $action] $args

    puts $logfile "<h2><a name=\"$toolkit\">$toolkit $where</a></h2>\n<table>\n"

    perform $toolkit soapinterop::validate.emsr:A echoVoid propc
    perform $toolkit soapinterop::validate.emsr:B echoVoid propc
    perform $toolkit soapinterop::validate.emsr:C echoVoid propc
    perform $toolkit soapinterop::validate.emsr:D echoVoid propc
    perform $toolkit soapinterop::validate.emsr:E echoVoid propc
    perform $toolkit soapinterop::validate.emsr:F echoVoid propc
    perform $toolkit soapinterop::validate.emtr:A echoVoid propc
    perform $toolkit soapinterop::validate.emtr:B echoVoid propc
    perform $toolkit soapinterop::validate.emtr:C echoVoid propc
    perform $toolkit soapinterop::validate.emtr:D echoVoid propc
    perform $toolkit soapinterop::validate.emtr:E echoVoid propc
    perform $toolkit soapinterop::validate.emtr:F echoVoid propc

    puts $logfile "</table><hr>\n\n"
    return {}
}
    
# -------------------------------------------------------------------------
# Description:
#  Perform a SOAP call. Capture the request and reply XML packets and save
#  to unique files under `logdir'. Appends a suitable log message to the
#  `logfile'
#
proc perform {toolkit procname {methodname {}} {prefix dump}} {
    global logfile
    global logdir
    global logcount

    set name [lindex [split $procname .] end]
    if {$methodname != {}} {set name $methodname}
    set failed [catch [list $procname] msg]
    if {$failed} {
	set msg [string map {< &lt; > &gt; & &amp;} $msg]
	set msg "<font color=\"red\">$msg</font>"
    } else {
	set msg ok
    }
    
    puts $logfile "<tr><td>$name</td><td>$msg</td>"

    if {$logdir != {}} {
        set request [file join $logdir "${prefix}${logcount}.xml"]
        incr logcount
        set reply   [file join $logdir "${prefix}${logcount}.xml"]
        incr logcount

        set err [catch {SOAP::dump -req soapinterop::$name} xml]
        set f [open $request w]
        puts -nonewline $f $xml
        close $f
        
        catch {SOAP::dump soapinterop::$name} xml
        set f [open $reply w]
        puts -nonewline $f $xml
        close $f

        puts $logfile "<td><a href=\"[file tail $request]\">request</a></td>\
	               <td><a href=\"[file tail $reply]\">reply</a></td></tr>"
    }
}

# Should we go ahead and run this...
if {!$::tcl_interactive} {
    set logdir   [file join [pwd] interop-results]
    silab:round1
    silab:round2
    silab:round2B
    silab:round2C
}
