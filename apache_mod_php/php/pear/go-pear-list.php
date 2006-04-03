<?php
/* This is a list of packages and versions
 * that will be used to create the PEAR folder
 * in the windows snapshot.
 * See win32/build/mkdist.php for more details
 * $Id: go-pear-list.php,v 1.12.2.3.2.4 2005/08/22 22:13:57 cellog Exp $
 */
$packages  = array(
// required packages for the installer
"PEAR"                  =>    "1.3.6",
"XML_RPC"               =>    "1.4.0",
"Console_Getopt"        =>    "1.2",
"Archive_Tar"           =>    "1.3.1",

// required packages for the web frontend
"PEAR_Frontend_Web"     =>    "0.4",
"HTML_Template_IT"      =>    "1.1",
"Net_UserAgent_Detect"  =>    "2.0.1",
);

?>
