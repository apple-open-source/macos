<?php
 $handle = printer_open();
 
 printer_start_doc($handle, "PHP Test");
 printer_start_page($handle);

 $pen = printer_create_pen(PRINTER_PEN_SOLID, 2, "000000");
 printer_select_pen($handle, $pen);
 
 printer_draw_rectangle($handle, 10, 10, 1000, 175);
 
 printer_delete_pen($pen);
 
 $font = printer_create_font("Arial", 72, 48, PRINTER_FW_NORMAL, false, true, false,0);
 printer_select_font($handle, $font);
 
 printer_draw_text($handle, "Printing with PHP 4", 20, 50);
 
 printer_delete_font($font);
 
 
 $font = printer_create_font("Arial", 24, 12, PRINTER_FW_MEDIUM, false, false, false,0);
 printer_select_font($handle, $font);

 printer_set_option($handle, PRINTER_TEXT_COLOR, "000000"); 
 printer_draw_text($handle, "PHP is simply the coolest scripting language!", 20, 200);
 
 printer_delete_font($font);  
 
 printer_end_page($handle);
 printer_end_doc($handle);
 
 printer_close($handle);
?>