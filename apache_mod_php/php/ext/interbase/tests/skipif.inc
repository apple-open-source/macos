<?php

/* $Id: skipif.inc,v 1.1.1.1 2003/07/18 18:07:34 zarzycki Exp $ */
/* used in tests */

if (!extension_loaded("interbase")) print "skip"; 
if (!is_executable("isql") && !is_executable("/opt/interbase/bin/isql")) print "skip";

?>
