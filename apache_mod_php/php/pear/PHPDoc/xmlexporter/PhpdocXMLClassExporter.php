<?php
/**
* Exports the data of a class as an xml file.
*
* @author		Ulf Wendel	<ulf.wendel@phpdoc.de>
* @version 	1.0
*/
class PhpdocXMLClassExporter extends PhpdocXMLDocumentExporter {

	function PhpdocXMLClassExporter() {
		$this->PHPDocXMLExporter();
	} // end constructor
	
	/**
	* Variable container attributes.
	* @var	array	$variableAttriutes
	*/														
	var $variableAttributes = array(
																	"name"			=> "CDATA",
																	"access"		=> "CDATA",
																	"type"			=> "CDATA",
																	"abstract"	=> "Boolean",
																	"static"		=> "Boolean",
																	"final"			=> "Boolean"
																);		
	/**
	* Class container attributes.
	* @var	array	$classAttributes
	*/
	var $classAttributes = array( 	
																"name"			=> "CDATA",
																"extends"		=> "CDATA",
																"undoc"			=> "Boolean",
																"access"		=> "CDATA",
																"abstract"	=> "Boolean",
																"static"		=> "Boolean",
																"final"			=> "Boolean",
																"package"		=> "CDATA"
													);

	var	$fileprefix = "class_";
	
	function create() {
		
		$attribs = $this->getAttributes($this->result, $this->classAttributes);										
		$this->xmlwriter->startElement("class", "", $attribs, false);
		
		$this->filenameXML($this->result["filename"]);
		
		$this->docXML($this->result);	
		
		if (isset($this->result["functions"]))
			$this->functionsXML($this->result["functions"]);
			
		if (isset($this->result["variables"]))
			$this->variablesXML($this->result["variables"]);
			
		if (isset($this->result["uses"]))
			$this->usesXML($this->result["uses"]);
			
		if (isset($this->result["consts"]))
			$this->constsXML($this->result["consts"]);
			
		if (isset($this->result["inherited"]))
			$this->inheritedOverridenXML($this->result["inherited"], "inherited");
			
		if (isset($this->result["overriden"]))
			$this->inheritedOverridenXML($this->result["overriden"], "overriden");
			
		if (isset($this->result["path"]))
			$this->pathXML($this->result["path"]);
		
		if (isset($this->result["baseclass"]))
			$this->baseclassXML($this->result["baseclass"]);
		
		if (isset($this->result["subclasses"]))
			$this->subclassesXML($this->result["subclasses"]);
			
		$this->xmlwriter->endElement("class", true);
		
	} // end func create
	
	/**
	* Handles inherited and overriden elements.
	* 
	* @param	array		Array of inherited or overriden elements
	* @param	string	Container used when saving the elements
	*/
	function inheritedOverridenXML($data, $tag) {
		
		reset($data);
		while (list($type, $elements)=each($data)) {
		
			reset($elements);
			while (list($from, $data2)=each($elements)) {

				$attribs = $this->getAttributes( array ("type" => $type, "src" => $from), $this->inheritedOverridenAttributes);				
				$this->xmlwriter->startElement($tag, "", $attribs, false);
				
				reset($data2);
				while (list($name, $v)=each($data2))
					$this->xmlwriter->addElement("element", $name);
					
				$this->xmlwriter->endElement($tag, true);
					
			}
			
		}
		
	} // end func inheritedOverridenXML

	/**
	* Writes the "path" (inheritance chain) of an element.
	* @param	array
	*/	
	function pathXML($path) {
		if (0==count($path))
			return;
			
		$this->xmlwriter->startElement("path", "", "", false);			
		
		reset($path);
		while (list($k, $parent)=each($path)) 
			$this->xmlwriter->addElement("parent", $parent);
		
		$this->xmlwriter->endElement("path", true);
		
	} // end func pathXML
	
	/**
	* Adds a baseclass container to the generated xml.
	* @param	string	Name of the baseclass
	*/
	function baseclassXML($base) {
	
		if (""!=$base)
			$this->xmlwriter->addElement("baseclass", $base);
			
	} // end func baseclassXML
	
	/**
	* Adds a list of subclasses to the generated xml.
	* @param	array	
	*/
	function subclassesXML($subclasses) {
		if (0==count($subclasses))
			return;
		
		$this->xmlwriter->startElement("subclasses", "", "", false, true);	
		
		reset($subclasses);
		while(list($subclass, $v)=each($subclasses)) 
			$this->xmlwriter->addElement("subclass", $subclass);
		
		$this->xmlwriter->endElement("subclasses", true);
		
	} // end func subclassesXML
	
	/**
	* Writes class variables to 
	*/
	function variablesXML($variables) {
															
		reset($variables);
		while (list($variable, $data)=each($variables)) {
		
			$attribs = $this->getAttributes($data, $this->variableAttributes);
			$this->xmlwriter->startElement("variable", $data["value"], $attribs, false);
			$this->docXML($data);
			$this->xmlwriter->endElement("variable", true);
			
		}
		
	} // end func variablesXML
	
} // end class PhpdocXMLClassExporter
?>