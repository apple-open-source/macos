<?php
/**
* Creates XML documents.
* 
* PHPDoc uses this helper class to generate xml documents. It's 
* not much what this class can do but it provides some simple
* functions to handle attributes and hides file handling tasks.
* 
* @author		Ulf Wendel <ulf.wendel@phpdoc.de>
* @version	1.0
*/
class PhpdocXMLWriter extends PhpdocObject {
	
	/**
	* Generated XML document.
	*
	* @var	string	$xml
	*/
	var $xml = "";
	
	/**
	* PHPDoc Warning object
	*
	* @var	object 	PhpdocWarning
	*/
	var $warn;
	
	/**
	* Filehandler used for IO operations
	*
	* @var	object	PhpdocFilehandler
	* @see	PhpdocXMLWriter
	*/
	var $fileHandler;
	
	/**
	* Creates a new PhpdocFileHandler
	*
	* @see	$filehandler
	*/
	function PhpdocXMLWriter() {
		$this->fileHandler = new PhpdocFileHandler;
	} // end constructor
	
	/**
	* Clears the internal xml data buffer so that a new document can be passed to the object.
	* @access	public
	*/
	function free() {
		$this->xml = "";
	} // end func free
	
	/**
	* Adds xml to the generated xml.
	*
	* @param	string	xml to append
	* @access	public
	*/
	function addXML($xml) {
	
		$this->xml.= $xml;
			
	} // end func addXML

	/**
	* Saves the xml to the specified file.
	*
	* @param	string	Name of the target file
	* @access	public
	*/	
	function export($filename) {
		return $this->fileHandler->createFile($filename, $this->xml);
	} // end func export
	
	/**
	* Adds an open (or single) xml tag to the generated xml.
	*
	* Use this function to add new elements/tags to the xml document. 
	* The tagname and all attributenames will be converted to lowercase.
	*
	* @param	string	elementname (tagname)
	*	@param	string	value of the container: <name>value
	* @param	array		Array of attributes: $attribs[n][type] = boolean|cdata, $attribs[n][value] = value
	* @param	boolean	Flag indication that you want an empty tag like <name/>.
	* @access	public
	* @see		endElement()
	*/
	function startElement($name, $value="", $attribs="", $close = false) {
	
		$xml = "<".strtolower($name);
		
		if (is_array($attribs)) {
			
			reset($attribs);
			while (list($attrib, $data)=each($attribs)) {
			
				$attrib = strtolower($attrib);
				$type = strtolower($data["type"]);
				
				switch($type) {
					case "boolean":
						$xml.= sprintf(' %s="%s"', $attrib, ($data["value"]) ? "true" : "false");
						break;
						
					case "cdata":
						$xml.= sprintf(' %s="%s"', $attrib, $this->xmlencode($data["value"]) );
						break;
				}
			}
			
		} 
		
		if ($close) {
		
			$xml.= "/>";
			
		} else {
		
			$xml.= ">";
			if (""!=$value)
				$xml.= $this->xmlencode($value);
				
		}
		
		$this->xml.= $xml;
		
	} // end func startElement
	
	/**
	* Adds a closing xml tag to the generated xml document.
	*
	* @param	string	Elementname (tagname)
	* @access	public
	* @see	startElement()
	*/
	function endElement($name) {
		$this->xml.= sprintf("</%s>",	strtolower($name)	);
	} // end func endElement
	
	/**
	* Adds a complete xml container to the generated xml document.
	*
	* @param	string	Elementname (tagname)
	* @param	string	Value
	* @param	array		Attributes
	* @access	public
	* @see	startElement(), endElement()
	*/
	function addElement($name, $value="", $attribs="") {
		
		if (""==$value) {
		
			$this->startElement($name, $value, $attribs, true);
			
		} else {
		
			$this->startElement($name, $value, $attribs, false);
			$this->endElement($name);
			
		}
			
	} // end func addElement
	
	/**
	* Encodes XML values.
	* @param	string	$value
	* @return	string	$value
	*/
	function xmlencode($value) {
#		return preg_replace( array("@<@", "@>@", "@'@", '@"@', "@&@", "@".LINEBREAK."@", "@\n@", "@\r@"), array("&lt;", "&gt;", "&apos;", "&quot;", "&amp;", '&#x0a;', '&#x0a;', '&#x0a;'), $value);
		return utf8_encode(preg_replace( array("@<@", "@>@", "@'@", '@"@', "@&@", "@".LINEBREAK."@", "@\n@", "@\r@"), array("&lt;", "&gt;", "&apos;", "&quot;", "&amp;", '&#x0a;', '&#x0a;', '&#x0a;'), $value));
	} // end func xmlencode
	
} // end class PhpdocXMLWriter
?>