<?php

/* $Id: skipif.inc,v 1.1.2.1 2003/04/25 15:43:35 sniper Exp $ */
/* used in tests */

if (!extension_loaded("interbase")) print "skip"; 
if (!is_executable("isql") && !is_executable("/opt/interbase/bin/isql")) print "skip";

?>
