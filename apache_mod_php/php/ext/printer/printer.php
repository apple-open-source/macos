<?
 /* open connection to the std printer */
 $handle = printer_open();
 
 /* build a string which containtains the devicename and driverversion */
 $string = sprintf("device name: \"%s\"\ndriver version: \"%s\"",
	printer_get_option($handle, "devicename"),
	printer_get_option($handle, "driverversion"));

 /* handle data to the printer */
 printer_write($handle, $string);

 /* close connection */
 printer_close($handle);
?>