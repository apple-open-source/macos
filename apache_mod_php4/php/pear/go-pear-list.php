<?php
/* This is a list of packages and versions
 * that will be used to create the PEAR folder
 * in the windows snapshot.
 * See win32/build/mkdist.php for more details
 * $Id: go-pear-list.php,v 1.12.2.3.2.6 2006/08/25 13:37:10 pajoye Exp $
 */
$packages  = array(
// required packages for the installer
"PEAR"                  =>    "1.4.11",
"XML_RPC"               =>    "1.4.8",
"Console_Getopt"        =>    "1.2",
"Archive_Tar"           =>    "1.3.1",

// required packages for the web frontend
"PEAR_Frontend_Web"     =>    "0.5.1",
"HTML_Template_IT"      =>    "1.1.4",
"Net_UserAgent_Detect"  =>    "2.0.1",
);
