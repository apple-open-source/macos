<?php
/**
* Exporter used to export phpdoc internals data structures as xml documents.
* @author		Ulf Wendel	<ulf.wendel@phpdoc.de>
* @version	1.0
*/
class PhpdocXMLExporter extends PhpdocObject {
	
	/**
	* Filename prefix for the generated xml document.
	* 
	* This class variable must be overriden by all derived classes.
	* PHPDoc uses the filename prefix to detect the content of 
	* the file.
	* 
	* @var	string	$fileprefix
	*/
	var $fileprefix = "";

	/**
	* Target directory where the xml documents get saved.
	* @var	string	$path
	* @see	setPath()
	*/	
	var $path = "";
	
	/**
	* Data to save as a xml document.
	* @var	array	$result
	* @see	setResult(), export()
	*/
	var $result = array();
	
	/**
	* Instance of PhpdocXMLWriter used to generate the xml document.
	* @var	object PhpdocXMLWriter
	* @see	PhpdocXMLExporter()
	*/
	var $xmlwriter;
	
	/**
	* Creates a PhpdocXMLWriter object.
	*
	* Make sure that all derived classes call this constructor.
	* 
	* @see	$xmlwriter
	*/															
	function PhpdocXMLExporter() {
	
		$this->xmlwriter = new PhpdocXMLWriter;
		
	} // end constructor											

	/**
	* Sets the target path for the generated xml documents.
	*  
	* @param	string
	* @see		$path
	* @access	public
	*/	
	function setPath($path) {
		$this->path = $path;
	} // end func setPath
	
	/**
	* Exports the given result array as xml document.
	*
	* @param	array	
	* @param	string	name of the target xml file
	* @access	public
	* @see		create(), $result
	*/
	function export($result, $xmlfile="") {
		
		if (0==count($result))
			return;

		$this->result = $result;
		
		$this->xmlwriter->addXML('<?xml version="1.0"?>');
		$this->xmlwriter->startElement("phpdoc", "", "", false, true);

		$this->create();

		$this->xmlwriter->endElement("phpdoc", true);
		
		if (""==$xmlfile)
			$xmlfile = $this->result["name"];
		
		/*
		if (file_exists($this->path.$xmlfile)) {
			$i = 1;
			while (file_exists($this->path.$name."_".$i.".xml"))
				$i++;
				
			$xmlfile =	$name."_".$i.".xml";
		}
		*/
		
		$xmlfile = $this->nameToURL($xmlfile);
		$xmlfile = $this->path.$this->fileprefix.$xmlfile.".xml";
		
		$this->xmlwriter->export($xmlfile);
		$this->xmlwriter->free();
		
	} // end func export
	
	function setResult($result) {
		$this->result = $result;
		$this->create();
	} // end func setResult
	
	/**
	* Kind of array_intersect for xml attributes.
	* 
	* This functions takes a data array and a list of allowed fields in the data
	* array. All of the allowed fields that exists in the data array will be 
	* copied to returned array which looks like:
	* $attribs[name] = array ( type => allowed[name], value => data[name] ). 
	* This structure is used by PhpdocXMLWriter->addElement().
	*
	* @param	array	data array
	* @param	array	array of allowed fields and their attribute type 
	* @return	array	$attribs
	*/
	function getAttributes($data, $allowed) {
		
		$attribs = array();
		
		reset($allowed);
		while (list($tag, $type)=each($allowed)) 
			if (isset($data[$tag])) 
				$attribs[$tag] = array( "type"	=> $type, "value" => $data[$tag] );
				
		return $attribs;
	} // end func getAttributes

} // end PhpdocXMLExporter
?>