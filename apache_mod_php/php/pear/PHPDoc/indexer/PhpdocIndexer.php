<?php
/**
* Builds Indexlists with the result from the  
*
* @author		Ulf Wendel	<ulf.wendel@redsys.de>
* @version	0.3alpha
*/
class PhpdocIndexer extends PhpdocObject {

	/**
	* Array of all packages.
	* @var $packages
	*/
	var $packages = array();
	
	/**
	* Current classtree.
	* @var	array
	*/
	var $classtree = array();
	
	/**
	* Current modulegroup.
	* @var	array
	*/
	var $modulegroup = array();
	
	/**
	* Array of all elements (functions, variables, constant, included files, classes, packages).
	* @var
	*/
	var $elements = array();
	
	/**
	* Array of fields that get added to the elementlist
	* @var	array
	*/ 
	var $elementFields = array("functions", "variables", "consts", "uses");
	
	/**
	* Adds a class to the index lists (elements, packages, classtree).
	*
	* @param	array	
	* @access	public
	* @see	addModule()
	*/
	function addClass(&$class) {
		
		$package = isset($class["package"]) ? $class["package"] : "No Package specified";
		$this->packages[$package]["classes"][] = $class["name"];
		$this->classtree[$class["name"]] = (isset($class["subclasses"])) ? $class["subclasses"] : array();
		$this->addElements($class, "class");
		
	} // end func addClass
	
	/**
	* Adds a module to the index lists (elements, packages, classtree).
	* 
	* @param	array
	* @access	public
	* @see addClass()
	*/
	function addModule(&$module) {
		
		$package = isset($module["package"]) ? $module["package"] : "No Package specified";
		$this->packages[$package]["modules"][] = $module["name"];
		$this->modulegroup[$module["group"]][] = $module["name"];
		$this->addElements($module, "module");
		
	}	// end func addModule

	/**
	* Returns the current classtree and resets the internal classtree field.
	*
	* @access	public
	* @return	array	$classtree
	*/
	function getClasstree() {
		
		$data = $this->classtree;
		$this->classtree = array();
		return $data;
		
	} // end func getClasstree	
	
	/**
	* Returns the current modulegroup and resets the internal modulegroup field.
	* 
	* @access	public
	* @return	array	$modulegroup
	*/
	function getModulegroup() {
		
		$data = $this->modulegroup;
		$this->modulegroup = array();
		return $data;
		
	} // end func getModulegroup

	/**
	* Returns the package list and resets the internal package field.
	*
	* @access	public
	* @return	array	$packages
	*/
	function getPackages() {
	
		reset($this->packages);
		while (list($package, )=each($this->packages))
			$this->elements[substr($package, 0, 1)][$package][] = array(
																															"type"				=> "package",
																															"sdesc"				=> "",
																															"source"			=> "",
																															"sourcetype"	=> ""
																														);								
		$data = $this->packages;
		$this->packages = array();
		return $data;
	} // end func getPackages
	
	/**
	* Returns the element index list and resets the internal elements field.
	* 
	* @access	public
	* @return array	$elements
	*/
	function getElementlist() {
		
		$data = $this->elements;
		$this->elements = array();
		return $data;
	} // end func getElementlist
	
	/**
	* Adds an element to the elementlist.
	* @param	array	
	* @param	string	Element type: class, module.
	*/
	function addElements(&$elements, $type) {
		
		$index = substr($elements["name"], 0, 1);
		$elname = $elements["name"];
		$this->elements[$index][$elname][] = array(
																							"type"				=> $type, 
																							"sdesc"				=> (isset($elements["sdesc"])) ? $elements["sdesc"] : "",
																							"source"			=> "",
																							"sourcetype"	=> $type
																						);

		reset($this->elementFields);
		while (list($k, $field)=each($this->elementFields)) {
			if (!isset($elements[$field])) 
				continue;
			
			reset($elements[$field]);
			while (list($name, $data)=each($elements[$field])) {
				
				
				if ("variables" == $field) {
				
					$index = substr($data["name"], 1, 1);
					$name = $data["name"];
					
				} else if ("uses" == $field) {

					$index = substr($data["file"], 0, 1);				
					$name = $data["file"];
						
				} else {
					
					$index = substr($data["name"], 0, 1);
					$name = $data["name"];
					
				}
				
				$this->elements[$index][$name][] = array(
																									"type"				=> $field,
																									"sdesc"				=> (isset($data["sdesc"])) ? $data["sdesc"] : "",
																									"source"			=> $elname,
																									"sourcetype"	=> $type
																								);
				
			}
			
		}
		
	} // end func addElements
	
} // end class PhpdocIndexer
?>