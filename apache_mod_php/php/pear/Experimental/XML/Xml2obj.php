<?php
//
// +----------------------------------------------------------------------+
// | PHP version 4.0                                                      |
// +----------------------------------------------------------------------+
// | Copyright (c) 1997-2001 The PHP Group                                |
// +----------------------------------------------------------------------+
// | This source file is subject to version 2.02 of the PHP license,      |
// | that is bundled with this package in the file LICENSE, and is        |
// | available at through the world-wide-web at                           |
// | http://www.php.net/license/2_02.txt.                                 |
// | If you did not receive a copy of the PHP license and are unable to   |
// | obtain it through the world-wide-web, please send a note to          |
// | license@php.net so we can mail you a copy immediately.               |
// +----------------------------------------------------------------------+
// | Authors: Tomas V.V.Cox <cox@idecnet.com>                             |
// |                                                                      |
// +----------------------------------------------------------------------+
//

require 'XML/Parser.php';

/*
* This class takes an xml file and maps it to an objects tree. It doesn't need
* DOM support, and the resultant tree is very similar to a DOM Doc.
* Example of use:
*
*<Package>
*	<Name>XML Render</Name>
*	<Maintainer>
*		<Name>John Smith</Name>
*		<Initials Type1="foo" Type2="bar">js</Initials>
*		<Email>js@mail.com</Email>
*	</Maintainer>
*</Package>
*
* require './Xml2obj.php';
* PEAR::setErrorHandling(PEAR_ERROR_DIE);
* $parser = new Xml2obj('./Pear_Package_Definition.xml');
* $tree = $parser->getTree();
* // View the resultant tree
* print_r($tree);
*
* Current limitations:
*
* @todo - Ability to parse file descriptors and strings, besides files
*		- Ability to seek for a value of a child node attribute (wish)
*
* @version 0.1
* @author Tomas V.V.Cox <cox@idecnet.com>
*/

class Xml2obj extends XML_Parser
{
	/*
	* Constructor
	* @param $file string The xml file to parse
	*/
	function Xml2obj ($file)
	{
		$this->folding = true;
		$this->XML_Parser(null, 'event');
		$this->xmlfile = $file;
		$GLOBALS['_XML_Xml2obj_root_elem'] = null;
		$this->obj1  = &$GLOBALS['_XML_Xml2obj_root_elem'];
	}

	/*
	* Parses the xml file and returns the objects tree
	*
	* @return object The object tree or an object error
	*/
	function &getTree ()
	{
		$err = $this->setInputFile($this->xmlfile);
		if (PEAR::isError($err)) {
			return $err;
		}
		$err = $this->parse();
		if (PEAR::isError($err)) {
			return $err;
		}
		return $this->obj1;
	}

	function StartHandler($xp, $elem, &$attribs)
	{
		$new = & new Xml2obj_branch;
		if (sizeof($attribs) > 0) {
			$new->atts = $attribs;
		}
		$new->name = $elem;
		// root element
		if (!isset($this->i)) {
			$this->obj1 = $new;
			$this->i = 2;
		} else {
			$obj_id = 'obj' . $this->i++;
			$this->$obj_id = $new;
		}
		return NULL;
	}

	function EndHandler($xp, $elem)
	{
		$this->i--;
		if ($this->i > 1) {
			$obj_id = 'obj' . $this->i;
			$new = & $this->$obj_id;
			if ($data = trim($this->cdata)) {
				$new->value = $data;
			}
			$parent_id = 'obj' . ($this->i - 1);
			$parent = & $this->$parent_id;
			$parent->children[] = $new;
		}
		$this->cdata = '';
		return NULL;
	}

	/*
	* The xml character data handler
	*/
	function cdataHandler($xp, $data)
	{
		$this->cdata .= $data;
	}
}

/*
* All objects of the Xml2obj tree are of this type.
*
* @author Tomas V.V.Cox <cox@idecnet.com>
*/
class Xml2obj_branch
{
	/*
	* The attributes of this element
	*/
	//var $atts; // type array
	/*
	* The name of the element itself.
	*/
	//var $name = null;
	/*
	* Elements with value will have also this property
	*/
	//var $value; // type string

	/*
	* Elemts with childrens will have this property
	*/
	//var $children; // type array

	/*
	* Returns the array of attributes of this node or null
	*/
	function attributes()
	{
		return (isset($this->atts)) ? $this->atts : null;
	}

	/*
	* Returns the array of child nodes or null
	*/
	function &children()
	{
		return (isset($this->children)) ? $this->children : null;
	}

	/*
	* Returns the value of an especific attribute or null if not found
	* @param $att string Attribute name
	*/
	function get_attribute ($att)
	{
		return (isset($this->atts[$att])) ? $this->atts[$att] : null;
	}
}

?>