<?php
/**
* Provides functions to access phpdoc xml documents that contain classes.
*
* @author		Ulf Wendel <ulf.wendel@phpdoc.de>
* @version 	1.0	
* @package	PHPDoc
*/
class PhpdocClassAccessor extends PhpdocDocumentAccessor {

	var $xmlkey = "class";
	
	/**
	* Array of inherited elements
	* @var	array	$inherited
	*/	
	var $inherited = array();
	
	/**
	* Returns an array with the data of a class (no functions etc, just the class docs).
	* @return	array	$class
	* @access	public
	*/
	function getClassdata() {
		
		$class = $this->xml["class"];

		unset($class["variable"]);
		unset($class["function"]);
		unset($class["uses"]);
		unset($class["constant"]);
		unset($class["inherited"]);
		unset($class["overriden"]);
		unset($class["path"]);
		
		return $class;
	} // end func getClassdata
	
	/**
	* Returns an array of inherited functions.
	* @return	array	
	* @access	public
	* @see	getInheritedVariables(), getInheritedUses(), getInheritedConstants()
	*/
	function getInheritedFunctions() {
		return $this->inherited["functions"];
	} // end func getInheritedFunctions

	/**
	* Returns an array of inherited variables.
	* @return	array
	* @access	public
	* @see	getInheritedFunctions(), getInheritedUses(), getInheritedConstants()
	*/	
	function getInheritedVariables() {
		return $this->inherited["variables"];
	} // end func getInheritedVariables
	
	/**
	* Returns an array of inherited included files.
	* @return	array
	* @access	public
	* @see		getInheritedFunctions(), getInheritedUses(), getInheritedConstants()
	*/
	function getInheritedUses() {
		return $this->inherited["uses"];
	} // end func getInheritedUses()
	
	/**
	* Returns an array of inherited constants.
	* @return	array
	* @access	public
	* @see		getInheritedFunctions(), getInheritedVariables(), getInheritedUses()
	*/
	function getInheritedConstants() {
		return $this->inherited["constants"];
	} // end func getInheritedConstants
	
	/**
	* Returns an array with the "path" of a class.
	* @return array $path
	* @access	public
	* @see		getSubclasses()
	*/	
	function getClasstree() {
		
		if (isset($this->xml["class"]["path"]))
			return $this->convertPath($this->xml["class"]["path"]);
		else 
			return array();
			
	} // end func getClasstree
	
	/**
	* Returns an array with all subclasses of a class.
	* @return	array
	* @access	public
	* @see		getClasstree()
	*/
	function getSubclasses() {
		return $this->data["subclasses"];
	} // end func getSubclasses
	

	/**
	* Converts a xml path array to a path that can be passed to the user.
	* 
	* The path is an array like path[0..n] = classname where path[0] is the 
	* directs parent (extends path[0]) and path[n] is the baseclass.
	*
	* @param	array	$xmlpath
	* @return	array	$path
	*/
	function convertPath($xmlpath) {

		$path = array();
		
		if (!isset($xmlpath["parent"][0])) {
			
			$path[0] = $xmlpath["parent"]["value"];		
				
		} else {
		
			reset($xmlpath["parent"]);
			while (list($k, $parent)=each($xmlpath["parent"]))
				$path[] = $parent["value"];
				
		}

		return $path;
	} // end func convertPath
	
	/**
	* Builds a list of inherited elements.
	* @see	$inherited
	*/
	function buildInheritedlist() {
		
		$this->inherited = array(
															"functions"	=> array(),
															"variables"	=> array(),
															"constants"	=> array(),
															"uses"			=> array()
														);
			
		if (isset($this->xml["class"]["inherited"])) {

			if (isset($this->xml["class"]["inherited"][0])) {
			
				reset($this->xml["class"]["inherited"]);
				while (list($k, $inherited)=each($this->xml["class"]["inherited"])) {
						
					$type = $inherited["type"];
					$src	= $inherited["src"];
		
					if (isset($inherited["element"][0])) {
					
						reset($inherited["element"]);
						while (list($k2, $element)=each($inherited["element"])) 
							$this->inherited[$type][$src][] = $element["value"];
							
					}	else {
					
						$this->inherited[$type][$src][] = $inherited["element"]["value"];
						
					}
					
				}
			
			}	else {
				
				$inherited = $this->xml["class"]["inherited"];
				$type			 = $inherited["type"];
				$src			 = $inherited["src"];
				
				if (isset($inherited["element"][0])) {
					
					reset($inherited["element"]);
					while (list($k, $element)=each($inherited["element"])) 
						$this->inherited[$type][$src][] = $element["value"];
						
				} else {
				
					$this->inherited[$type][$src][] = $inherited["element"]["value"];
					
				}

			}
			
			unset($this->xml["class"]["inherited"]);
			
		}
			
	} // end func buildInheritedlist
	
	/**
	* Builds a list of subclasses
	*/
	function buildSubclasslist() {
		
		$this->data["subclasses"] = array();
		
		if (isset($this->xml["class"]["subclasses"])) {
		
			if (isset($this->xml["class"]["subclasses"]["subclass"][0])) {

				reset($this->xml["class"]["subclasses"]["subclass"]);
				while (list($k, $subclass)=each($this->xml["class"]["subclasses"]["subclass"]))
					$this->data["subclasses"][] = $subclass["value"];

			} else {

				$this->data["subclasses"][] = $this->xml["class"]["subclasses"]["subclass"]["value"];
				
			}

		}
		
	} // end func buildSubclasslist
	
	function init() {

		#$this->introspection("xml", $this->xml);
		
		$this->buildInheritedlist();
		$this->buildSubclasslist();
		
		list($this->data["functions"], $this->data["functionsaccess"]) = $this->getElementlist("function");		
		list($this->data["variables"], $this->data["variablesaccess"]) = $this->getElementlist("variable");
		list($this->data["constants"], $this->data["constantsaccess"]) = $this->getElementlist("constant");
		
		$this->buildUseslist();		
		
		#$this->introspection("data", $this->data);

	} // end func Init
	
} // end class PhpdocClassAccessor
?>