<?php
/**
* Reads XML documents into a multi dimensional Array.
*
* @author	Ulf Wendel <ulf.wendel@phpdoc.de>
*/
class PhpdocXMLReader extends PhpdocObject {
	
	/**
	* PHPDocFileHandler object.
	* @var	object PhpdocFileHandler
	* @see	createFileHandler()
	*/
	var $filehandler; 

	/**
	* Values array from xml_parse_into_struct().
	* @var	array
	* @see	parse(), stripCloseFromStructvalues(), importXML()
	*/
	var $structvalues = array();
	
	/**
	* Parses a given XML file and returns the data as a hash.
	* 
	* Please do not ask me for a in detail explanation of how it is done,
	* the documentation is in the source...
	*
	* @param	string	$filename Name of the xml document
	* @access	public
	* @throws PhpdocError
	* @see	importXML()
	*/	
	function parse($filename) {
	
		if (""==$filename) {
			$this->err[] = new PhpdocError("No filename given.", __FILE__, __LINE__);
			return array();
		}
		
		$parser = @xml_parser_create();
		if (!$parser) {
			$this->err = PhpdocError("Can't create a XML Parser.", __FILE__, __LINE__);
			return array();
		}
		xml_parser_set_option($parser, XML_OPTION_CASE_FOLDING, 0);
		
		$this->createFileHandler();
		$xml = $this->filehandler->getFile($filename);

		$values = array();
		$index 	= array();
		xml_parse_into_struct($parser, $xml, &$values, &$index);
		
		xml_parser_free($parser);

		$this->structvalues = $values;
		$this->stripCloseFromStructvalues();
		list($data, $last) = $this->importXML();
		$this->structvalues = array();
		
		return $data;
	} // end func parse

	/**
	* Creates a PhpdocFileHandler object and saves it to $filehandler if it does not already exist.
	* @see	$filehandler
	*/	
	function createFilehandler() {
	
		if (!isset($this->filehandler))
			$this->filehandler = new PhpdocFileHandler;
			
	} // end func createFilehandler
	
	/**
	* Strips all values out of the xml_parse_intro_struct() values array with the type "open".
	*
	* @see	$structvalues 
	*/
	function stripCloseFromStructvalues() {
		
		$values = array();
		
		reset($this->structvalues);
		while (list($k, $v)=each($this->structvalues))
			if ("close"!=$v["type"])
				$values[] = $v;
				
		$this->structvalues = $values;
	} // end func stripCloseFromStructvalues
	 
	/**
	* Converts an xml_parse_into_struct value array to an array that's simmilar to phpdocs internal arrays.
	*
	* Well, don't ask me to explain this hack. Just take it as it. For those who want to unterstand and optimize
	* it:
	*  - PHP3 compatibility is a must
	*  - no XML DOM
	*  - no eval(), this can't be optimized by the compiler
	*
	* @param	int	
	* @param	int
	* @return	array	$data[0] = daten, $data[1] = some index value used for the recursion
	* @see		addToArray()
	*/
	function importXML($start = 0, $allowed_level = 1) {
		
		$data = array();
		$last = 0;
		
		for ($i=$start; $i<count($this->structvalues); $i++) {
			if ($allowed_level != $this->structvalues[$i]["level"]) 
				break;
			
			$value 		= (isset($this->structvalues[$i]["value"])) ? $this->structvalues[$i]["value"] : "";
			$attribs 	= (isset($this->structvalues[$i]["attributes"])) ? $this->structvalues[$i]["attributes"] : "";
			$tag			= $this->structvalues[$i]["tag"];

			if ("open" == $this->structvalues[$i]["type"]) {

				list($inner, $next) = $this->importXML($i+1, $this->structvalues[$i]["level"]+1);
				
				// append the inner data to the current one
				$data				= $this->addToArray($data, $tag, $value, $attribs, $inner);
				
				// skip some entries in $this->structvalues
				$i = $next;
				
			} else {
			
				// same level, append to the array
				$data = $this->addToArray($data, $tag, $value, $attribs);
				
			}
			
			// remember the last index in $this->structvalues we've worked on
			$last = $i;
		}
		
		return array($data, $last);
	} // end func importXML
	
	/**
	* Appends some values to an array
	* Well, don't ask me; just improve it with the remarks on buildXMLResult()
	* @param	array
	* @param	string
	* @param	string	
	* @param	array
	* @param	array
	* @return	array $target
	*/
	function addToArray($target, $key, $value="", $attributes = "", $inner="") {

		if (!isset($target[$key]["value"]) && !isset($target[$key][0])) {
	
			if (""!=$inner)
				$target[$key] = $inner;

			if (""!=$attributes) {
				reset($attributes);
				while (list($k, $v)=each($attributes))
					$target[$key][$k] = $this->xmldecode($v);
			}
				
			$target[$key]["value"] = $this->xmldecode($value);
		
		} else {
	
			if (!isset($target[$key][0])) {
		
				$oldvalue = $target[$key];
				$target[$key] = array();
				$target[$key][0] = $oldvalue;
			
				if (""!=$inner)
					$target[$key][1] = $inner;

				if (""!=$attributes) {
					reset($attributes);
					while (list($k, $v)=each($attributes))
						$target[$key][1][$k] = $this->xmldecode($v);
				}
				
				$target[$key][1]["value"] = $this->xmldecode($value);
				
			} else {
				
				$index = count($target[$key]);
				
				if (""!=$inner)
					$target[$key][$index] = $inner;

				if (""!=$attributes) {
					reset($attributes);
					while (list($k, $v)=each($attributes))
						$target[$key][$index][$k] = $this->xmldecode($v);
				}
				
				$target[$key][$index]["value"] = $this->xmldecode($value);

			}

		}
	
		return $target;
	} // end func addToArray
	
	function xmldecode($value) {
		#return preg_replace( array("@&lt;@", "@&gt;@", "@&apos;@", "@&quot;@", "@&amp;@"), array("<", ">", "'", '"', "&"), $value);
		return utf8_decode(preg_replace( array("@&lt;@", "@&gt;@", "@&apos;@", "@&quot;@", "@&amp;@"), array("<", ">", "'", '"', "&"), $value));
	} // end func xmldecode

} // end class PhpdocXMLReader
?>