<?php
//
// +----------------------------------------------------------------------+
// | PHP version 4.0                                                      |
// +----------------------------------------------------------------------+
// | Copyright (c) 1997-2001 The PHP Group                                |
// +----------------------------------------------------------------------+
// | This source file is subject to version 2.0 of the PHP license,       |
// | that is bundled with this package in the file LICENSE, and is        |
// | available at through the world-wide-web at                           |
// | http://www.php.net/license/2_02.txt.                                 |
// | If you did not receive a copy of the PHP license and are unable to   |
// | obtain it through the world-wide-web, please send a note to          |
// | license@php.net so we can mail you a copy immediately.               |
// +----------------------------------------------------------------------+
// | Authors: Sterling Hughes <sterling@php.net>                          |
// +----------------------------------------------------------------------+
//
// $Id: Processor.php,v 1.1.1.1 2001/07/19 00:20:49 zarzycki Exp $
//
// HTML processing utility functions.
//

/**
 * TODO:
 *   -  Extend the XML_Parser module to provide HTML parsing abilities
 */

require_once('PEAR.php');

// {{{ HTML_Processor

$GLOBALS['_HTML_Processor_translation_table'] = array();

/**
 * The HTML_Processor class facilitates the parsing and processing of
 * HTML.  Currently only some basic functionality to process HTML is 
 * provided..
 *
 * @access public
 * @author Sterling Hughes <sterling@php.net>
 * @since  PHP 4.0.5
 */
class HTML_Processor extends XML_Parser
{
	// {{{ HTML_Processor()

	function HTML_Processor()
	{
		global $_HTML_Processor_translation_table;
		$_HTML_Processor_translation_table = get_html_translation_table();
	}

	// }}}
	// {{{ ConvertSpecial()

	/**
	 * Convert special HTML characters (like &copy;) into their ASCII
	 * equivalents.
	 *
	 * @param  string &$text The text to convert
	 *
	 * @access public
	 * @author Sterling Hughes <sterling@php.net>
	 * @since  PHP 4.0.5
	 */
	function ConvertSpecial(&$text)
	{
		global $_HTML_Processor_translation_table;

		$text = strtr($text, 
		              array_keys($_HTML_Processor_translation_table), 
		              array_flip(array_values($_HTML_Processor_translation_table)));
	}

	// }}}
	// {{{ ConvertASCII()

	/**
	 * Convert ASCII characters into their HTML equivalents (ie, ' to 
	 * &quot;).
	 *
	 * @param  string &$text The text to convert
	 *
	 * @access public
	 * @author Sterling Hughes <sterling@php.net>
	 * @since  PHP 4.0.5
	 */
	function ConvertASCII(&$text)
	{
		global $_HTML_Processor_translation_table;

		$text = strtr($text,
		              array_flip(array_values($_HTML_Processor_translation_table)), 
		              array_keys($_HTML_Processor_translation_table));
	}

	// }}}
}

// }}}
?>
