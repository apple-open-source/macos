<?php
/**
* Provides functions to access phpdoc xml documents that contain modules.
*/
class PhpdocModuleAccessor extends PhpdocDocumentAccessor {
	
	var $xmlkey = "module";
	
	/**
	* Returns an array with the data of a module (no functions etc, just the module docs).
	* @return	array	$class
	* @access	public
	*/
	function getModuledata() {
		
		$module = $this->xml["module"];

		unset($module["function"]);
		unset($module["uses"]);
		unset($module["constant"]);
		
		return $module;
	} // end func getModuledata
	
	function init() {

		list($this->data["functions"], $this->data["functionsaccess"]) = $this->getElementlist("function");		
		list($this->data["variables"], $this->data["variablesaccess"]) = $this->getElementlist("variable");
		list($this->data["constants"], $this->data["constantsaccess"]) = $this->getElementlist("constant");
		$this->buildUseslist();		

	} // end func Init
	
} // end class PhpdocModuleAccessor
?>