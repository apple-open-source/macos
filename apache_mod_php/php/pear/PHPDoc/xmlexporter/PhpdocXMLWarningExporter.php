<?php
/**
* Exports a list of documentation warnings found by phpdoc
* 
*/ 
class PhpdocXMLWarningExporter extends PhpdocXMLExporter {
	
	/**
	* Attributes of a warning container.
	* @var	array
	*/
	var $warningAttributes = array(
																	"name"				=> "CDATA",
																	"type"				=> "CDATA",
																	"elementtype"	=> "CDATA"
																);
	
	var $fileprefix = "warnings_";
	
	function PhpdocXMLWarningExporter() {
		$this->PhpdocXMLExporter();
	} // end constructor
	
	function create() {

		reset($this->result);
		while (list($file, $warnings)=each($this->result)) {
			
			$this->xmlwriter->startElement("warnings", "", array("file"	=> array( "type"	=> "CDATA", "value"	=> $file)));
			
			reset($warnings);
			while (list($type, $warning)=each($warnings)) {
			
				reset($warning);
				while (list($k, $data)=each($warning)) {
					$data["elementtype"] = $type;
					$this->xmlwriter->addElement("warning", $data["msg"], $this->getAttributes($data, $this->warningAttributes));
				}
					
			}
			
			$this->xmlwriter->endElement("warnings");
			
		}
		
	} // end function create
	
} // end class PhpdocXMLWarningExporter
?>