<?php
/**
* Base of the class and module accessor.
*/
class PhpdocDocumentAccessor extends PhpdocAccessor {

	/**
	* Kind of top-level container in the xml document.
	* 
	* Must be set by all derived classes.
	*
	* @var	string	
	*/
	var $xmlkey = "";

	/**
	* Returns an array with all functions.
	* @return array	$functions
	* @access	public
	* @see	getFunctionsByAccess()
	*/
	function getFunctions() {
		return $this->getElements("functions", "functionsaccess");
	} // end func getFunctions
	
	/**
	* Returns an array with all functions with a certain access (public, private) attribute.
	* @param	string	Requested access attribute.
	* @return	array	$functions
	* @access	public
	* @see	getFunctions()
	*/
	function getFunctionsByAccess($access) {
		return $this->getElementsByAccess($access, "functions", "functionsaccess");
	} // end func getFunctionByAccess
	
	/**
	* Returns an array with all variables.
	* @return	array $variables
	* @access	public
	* @see	getVariablesByAccess()
	*/
	function getVariables() {
		return $this->getElements("variables", "variablesaccess");
	} // end func getVariables

	/**
	* Returns an array with all variables with a certain access (public, private) attribute.
	* @param	string	Requested access attribute.
	* @return	array	$variables
	* @access	public
	* @see	getVariables()
	*/	
	function getVariablesByAccess($access) {
		return $this->getElementsByAccess($access, "variables", "variablesaccess");
	} // end func getVariablesByAccess
	
	/**
	* Returns an array of all constants.
	* @return	array	$constants
	* @access	public
	* @see	getConstantsByAccess()
	*/
	function getConstants() {
		return $this->getElements("constants", "constantsaccess");
	} // end func getConstants
	
	/**
	* Returns an array of all constants with a certain access (public, private) attribute.
	* @param	string	Requested access attribute.
	* @return	array	$constants
	* @see		getConstants()
	* @access	public
	*/
	function getConstantsByAccess($access) {
		return $this->getElementsByAccess($access, "constants", "constantsaccess");
	} // end func getConstantsByAccess
	
	/**
	* Returns an array of all included files.
	* @return	array	$uses
	* @see		getUsesByType()
	* @access	public
	*/
	function getUses() {
		return $this->getElements("uses", "usestype");
	} // end func getUses

	/**
	* Returns an array of all included files with a certain type (include, require...) attribute.
	* @param	string	Requested type: include, include_once, require, require_once
	* @return	array		$uses
	* @access	public
	*/	
	function getUsesByType($type) {
		
		$data = array();
		
		if (!isset($this->data["usestype"][$type])) 
			return $data;
			
		reset($this->data["usestype"][$type]);
		while (list($k, $file)=each($this->data["usestype"][$type])) {
		
			$data[$file] = $this->data["uses"][$file];
			if ($this->freeOnGet)
				unset($this->data["uses"][$file]);
				
		}
		
		if ($this->freeOnGet)
			unset($this->data["usestype"][$type]);
			
		return $data;
	} // end func getUsesByType
	
	/**
	* Returns elements from the internal $data array.
	* 
	* The object uses this function to extract functions, variables, uses and 
	* constants from an internal array. Note that this is not a public function,
	* future version might access internal data structures different.
	*
	* @param	string	Name of the element you need: functions, variables,...
	* @param	string	Name of internal element access table
	* @see		$data
	*/
	function getElements($element, $elementaccess) {
		
		if ($this->freeOnGet) {
			
			$data = $this->data[$element];
			unset($this->data[$element]);
			unset($this->data[$elementaccess]);
			return $data;
			
		} else {
		
			$this->data[$element];
			
		}
		
	} // end func getElements

	/**
	* Returns elements with a certain access type from the internal data.
	* @param	string	Accesstype
	* @param	string	element name
	* @param	string	access type
	* @brother	getElements()
	*/	
	function getElementsByAccess($access, $element, $elementaccess) {
		
		$data = array();
		
		if (!isset($this->data[$elementaccess][$access]))
			return $data;
		
		reset($this->data[$elementaccess][$access]);
		while (list($k, $name)=each($this->data[$elementaccess][$access])) {
			
			$data[$name] = $this->data[$element][$name];
			if ($this->freeOnGet)
				unset($this->data[$element][$name]);
				
		}
		
		if ($this->freeOnGet)
			unset($this->data[$elementaccess][$access]);
			
		return $data;
	} // end func getElementsByAccess

	/**
	* Adds a list of @included files to the internal data array.
	*/
	function buildUseslist() {

		$this->data["uses"] = array();
		$this->data["usestype"] = array();
		
		if (isset($this->xml[$this->xmlkey]["uses"])) {

			if (isset($this->xml[$this->xmlkey]["uses"][0])) {		
	
				reset($this->xml[$this->xmlkey]["uses"]);
				while (list($k, $data)=each($this->xml[$this->xmlkey]["uses"])) {
					$this->data["uses"][$data["file"]] = $data;
					$this->data["usestype"][$data["type"]][] = $data["file"];
				} 
				
			} else {
			
				$data = $this->xml[$this->xmlkey]["uses"];
				$this->data["uses"][$data["file"]] = $data;
				$this->data["usestype"][$data["type"]][] = $data["file"];

			}
			
			unset($this->xml[$this->xmlkey]["uses"]);			
		}
		
	} // end func buildUseslist
	
	/**
	* Adds a list of a certain element to the internal data array.
	* @param	string	name of the element to add: function, variable, constant.
	*/
	function getElementlist($element) {
	
		$elements = array();
		$elementaccess = array();
		
		if (isset($this->xml[$this->xmlkey][$element])) {
															
			if (isset($this->xml[$this->xmlkey][$element][0])) {

				reset($this->xml[$this->xmlkey][$element]);
				while (list($k, $data)=each($this->xml[$this->xmlkey][$element])) {
					$elements[$data["name"]] = $data;
					$elementaccess[$data["access"]][] = $data["name"];	
				}	
				
			} else {
				
				$data = $this->xml[$this->xmlkey][$element];
				$elements[$data["name"]] = $data;
				$elementaccess[$data["access"]][] = $data["name"];
				
			}
			
			unset($this->xml[$this->xmlkey][$element]);
			
		}
		
		return array($elements, $elementaccess);
	} // end func getElementlist

} // end class PhpdocDocumentAccessor
?>