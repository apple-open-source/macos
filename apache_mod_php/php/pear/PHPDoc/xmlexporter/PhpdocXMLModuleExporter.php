<?php
/**
* Exports the data of a module as an xml document
*
* @author		Ulf Wendel <ulf.wendel@phpdoc.de>
* @version	1.0
*/
class PhpdocXMLModuleExporter extends PhpdocXMLDocumentExporter {
	
	/**
	* Module container attributes
	* @var	array	$moduleAttributes
	*/
	var $moduleAttributes = array(
																"name"		=> "CDATA",
																"group"		=> "CDATA",
																"undoc"		=> "Boolean",
																"access"	=> "CDATA",
																"package"	=> "CDATA"
															);

	var $fileprefix = "module_";
	
	function PhpdocXMLModuleExporter() {
		$this->PhpdocXMLExporter();	
	} // end constructor
	
	function create() {
		
		$attribs = $this->getAttributes($this->result, $this->moduleAttributes);										
		$this->xmlwriter->startElement("module", "", $attribs);
		
		$this->filenameXML($this->result["filename"]);
		
		$this->docXML($this->result);	
		
		if (isset($this->result["functions"]))
			$this->functionsXML($this->result["functions"]);
			
		if (isset($this->result["uses"]))
			$this->usesXML($this->result["uses"]);
			
		if (isset($this->result["consts"]))
			$this->constsXML($this->result["consts"]);
		
		$this->xmlwriter->endElement("module", true);
		
	} // end func create
	
} // end class PhpdocXMLModuleExporter
?>