<?php

/**
* DisableAdvancedUI
* Includes stylesheets and JavaScripts which turn off "advanced" UI.
*/
class disable_advanced_ui extends rcube_plugin
{
    
    function init()
    {
        $this->include_stylesheet('disable_advanced_ui.css');
    }
    
}


?>