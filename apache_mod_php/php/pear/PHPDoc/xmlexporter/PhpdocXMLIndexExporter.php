<?php
/**
* Creates XML index files
*/
class PhpdocXMLIndexExporter extends PhpdocXMLExporter {

	/**
	* Chapter tag attributes
	* @var	array
	*/
	var $chapterAttributes = array(	"name"	=> "CDATA" );
	
	/**
	* Element tag attributes.
	* @var	array()
	*/
	var $elementAttributes	= array(
															"type"				=> "CDATA",
															"source"			=> "CDATA", 
															"sourcetype"	=> "CDATA" 
														);
	/**
	* Just call the parent class constructor
	*/
	function PhpdocXMLIndexExporter() {
		$this->PhpdocXMLExporter();
	} // end constructor
	
	/**
	* Exports a modulegroup.
	*
	* @param	array
	* @access	public
	*/
	function exportModulegroup(&$modulegroup) {
	
		$this->xmlwriter->free();
		
		$this->xmlwriter->addXML('<?xml version="1.0"?>');
		$this->xmlwriter->startElement("phpdoc");

		reset($modulegroup);
		list($group, $modules)=each($modulegroup);
		$attribs = array( "name" => array( "type"	=> "CDATA", "value"	=> $group) );
		$this->xmlwriter->startElement("modulegroup", "", $attribs);
		
		reset($modules);
		while (list($k, $module)=each($modules))
			$this->xmlwriter->addElement("module", "", array( "name" => array( "type" => "CDATA", "value" => $module )) );
		
		$this->xmlwriter->endElement("modulegroup");
		$this->xmlwriter->endElement("phpdoc");
		
		$group = $this->nameToUrl($group);
		$this->xmlwriter->export($this->path."modulegroup_$group.xml");
		$this->xmlwriter->free();
		
	} // end func exportModulegroup
	
	/**
	* Exports a packagelist
	* 
	* @param	array
	* @access	public
	*/
	function exportPackagelist(&$packagelist) {
	
		$this->xmlwriter->free();
		
		$this->xmlwriter->addXML('<?xml version="1.0"?>');
		$this->xmlwriter->startElement("phpdoc");
		$this->xmlwriter->startElement("packagelist");
		
		reset($packagelist);
		while (list($package, $elementlist)=each($packagelist)) {
		
			$attribs = array( "name" => array("type" => "CDATA", "value" => $package) );
			$this->xmlwriter->startElement("package", "", $attribs);
			
			reset($elementlist);
			while (list($type, $elements)=each($elementlist)) {
				
				$container = ("classes"==$type) ? "class" : "module";
				while (list($k, $element)=each($elements)) {
					
					$attribs = array( "name"	=> array("type" => "CDATA", "value"	=> $element));
					$this->xmlwriter->addElement($container, "", $attribs);
										
				}
					
			}
			
			$this->xmlwriter->endElement("package");
			
		} 
		
		$this->xmlwriter->endElement("packagelist");
		$this->xmlwriter->endElement("phpdoc");
		$this->xmlwriter->export($this->path."packagelist.xml");
		$this->xmlwriter->free();
		
	} // end func exportPackagelist
	
	/**
	* Exports a classtree
	* 
	* @param	array
	* @access	public
	*/
	function exportClasstree(&$classtree, $baseclass) {
	
		$this->xmlwriter->free();
		
		$this->xmlwriter->addXML('<?xml version="1.0"?>');
		$this->xmlwriter->startElement("phpdoc");
		
		$attribs = array("baseclass"	=> array("type"	=> "CDATA", "value"	=> $baseclass));
		$this->xmlwriter->startElement("classtree", "", $attribs);			
		
		reset($classtree);
		while (list($parentclass, $subclasses)=each($classtree)) {
		
			$attribs = array("name"	=> array("type"	=> "CDATA", "value"	=> $parentclass));
			$this->xmlwriter->startElement("class", "", $attribs);
			
			reset($subclasses);
			while (list($subclass, $v)=each($subclasses)) 
				$this->xmlwriter->addElement("subclass", $subclass);
				
			$this->xmlwriter->endElement("class");
			
		}
		
		$this->xmlwriter->endElement("classtree");
		$this->xmlwriter->endElement("phpdoc");
		
		$baseclass = $this->nameToURL($baseclass);
		$this->xmlwriter->export($this->path."classtree_$baseclass.xml");
		$this->xmlwriter->free();
		
	} // end func exportClasstree
	
	/**
	* Exports a list of all elements
	* 
	* @param	array
	* @access	public
	*/
	function exportElementlist(&$elementlist) {
	
		$this->xmlwriter->free();
		
		$this->xmlwriter->addXML('<?xml version="1.0"?>');
		$this->xmlwriter->startElement("phpdoc");
		$this->xmlwriter->startElement("index");			
		
		reset($elementlist);
		while (list($index, $elements)=each($elementlist)) {
		
			$attrib = array( "char"	=> array( "type" => "CDATA", "value" => $index ) );
			$this->xmlwriter->startElement("chapter", "", $attrib);

			reset($elements);
			while (list($name, $element)=each($elements)) {
				
				reset($element);
				while (list($k, $eldata)=each($element)) {
					$attribs = $this->getAttributes($eldata, $this->elementAttributes);
					$attribs["name"] = array( "type"	=> "CDATA", "value"	=> $name );
					$this->xmlwriter->addElement("element", $eldata["sdesc"], $attribs);
				}
				
			}

			$this->xmlwriter->endElement("chapter");
		}
		
		$this->xmlwriter->endElement("index");
		$this->xmlwriter->endElement("phpdoc");
		
		$this->xmlwriter->export($this->path."elementlist.xml");
		$this->xmlwriter->free();

	} // end func exportElementlist
	
} // end class PhpdocXMLIndexExporter
?>