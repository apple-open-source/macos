<?php
/**
* Provides an API to access PHPDoc XML files.
* 
* It's up to you eigther to use this class to access 
* the phpdoc xml files or to write your own parser.
*/
class PhpdocAccessor extends PhpdocObject {

	/**
	* Instance of PhpdocXMLReader
	* @var	object 	PhpdocXMLReader	$xmlreader
	*/	
	var $xmlreader;
	
	/**
	* Result of the PhpdocXMLReader
	* @var	array	$xml
	*/
	var $xml = array();
	
	/**
	* Free xml resources on calling a getXY() function?
	* 
	* One of the design goals was to minimize the memory consumption of PHPdoc.
	* So PHPdoc tries to save data as soon as possible to the disk, reuse objects
	* and free resources of an object when they are no longer needed. The default 
	* value of true will cause the object to free the memory used by the 
	* xml data as soon as possible.
	* 
	* @var	boolean
	*/	
	var $freeOnGet = true;

	/**
	* Reformatted PhpdocXMLReader result array
	* @var	array
	*/
	var $data = array();
	
	/**
	* Loads the specified xml file. 
	*
	* @param	string	Name of the xml file
	* @return	boolean	False if the given xml file was not 
	*									found or is empty otherwise true.
	* @access	public
	* @see		init()
	*/
	function loadXMLFile($filename) {
	
		$this->xmlreader = new PhpdocXMLReader;
		
		$this->xml = $this->xmlreader->parse($filename);
		$this->xml = $this->xml["phpdoc"];
		$ok = (!is_array($this->xml) || 0==count($this->xml)) ? false : true;
		
		$this->init();
		
		return $ok;		
	} // end func loadXMLFile
		
	/**
	* Reformats the xml result array from the PhpdocXMLReader.
	* 
	* Every derived class must override this function to call the functions
	* it needs to reorganize the data from the PhpdocXMLReader in a 
	* way that it needs. 
	*
	* @abstract
	* @see	$xml, $data
	*/
	function init() {
	} // end func init

} // end class PhpdocAccessor
?>