<?php
/* vim: set expandtab tabstop=4 shiftwidth=4; */
// +---------------------------------------------------------------------+
// |  PHP version 4.0                                                    |
// +---------------------------------------------------------------------+
// |  Copyright (c) 1997-2001 The PHP Group                              |
// +---------------------------------------------------------------------+
// |  This source file is subject to version 2.0 of the PHP license,     |
// |  that is bundled with this package in the file LICENSE, and is      |
// |  available through the world-wide-web at                            |
// |  http://www.php.net/license/2_02.txt.                               |
// |  If you did not receive a copy of the PHP license and are unable to |
// |  obtain it through the world-wide-web, please send a note to        |
// |  license@php.net so we can mail you a copy immediately.             |
// +---------------------------------------------------------------------+
// |  Authors:  Sean Grimes <metallic@noworlater.net>                    |
// +---------------------------------------------------------------------+
// 
// $Id: Render.php,v 1.1.1.1 2001/07/19 00:20:52 zarzycki Exp $

/**
* Render class for rendering from XML. 
*
* This class should render documents from xml.
* The intended rendering modes will be HTML and
* the Adobe PDF format. Maybe at some point I 
* will make it possible to take a document from
* HTML to PDF, but this is unlikely. 
* 
* @author   Sean Grimes <metallic@noworlater.net>
* @version  $Id: Render.php,v 1.1.1.1 2001/07/19 00:20:52 zarzycki Exp $
* @todo     - Implement the HTML and PDF rendering modes
*           - Extend the parse() function to what is needed
*           - Implement filesystem commands
*           - Come up with the XML language syntax
*           - Provide a better class interface 
*          - Do some debugging    
*/ 

require_once "Parser.php";

class XML_Render extends XML_Parser {

	/**
	* Renders the XML document.
	*
	* This function really isnt implemented yet. 
	* Basically, I just added the calls for the HTML
	* and PDF subclass rendering modes. I'm hoping this
	* class will be easily built onto over PEAR's lifetime.
	*
	* @param $mode Rendering mode. Defaults to HTML.
	* @author Sean Grimes <metallic@noworlater.net>
	*/
 
	function render($mode = 'HTML') {
		if($mode == 'HTML') {
			$html = new XML_Render_HTML();
			$html->render();
		}
		if($mode == 'PDF') {
			$pdf = new XML_Render_PDF();
			$pdf->render();
		} else {
			$message = "Error. Unsupported rendering mode.";
			new PEAR_Error($message, 0, PEAR_ERROR_RETURN, E_USER_NOTIFY);
		}
	}
    
}
