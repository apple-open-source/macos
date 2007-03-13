<?php

/* $Id: skipif.inc,v 1.1.2.1.8.1 2005/12/11 22:13:52 helly Exp $ */
/* used in tests */

if (!extension_loaded("interbase")) print "skip"; 
if (!is_executable("isql") && !is_executable("/opt/interbase/bin/isql")) print "skip isql not found or not executable";

?>
