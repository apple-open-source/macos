<?php
/**
* Analyses a modulegroup.
*/
class PhpdocModuleAnalyser extends PhpdocAnalyser {

	/**
	* Module data
	* @var	array
	*/
	var $modulegroup		= array();

	/**
	* List of all modules in the modulegroup
	* @var	array
	*/ 
	var $modulelist = array();
	
	/**
	* Puuuh - findUndocumented() needs this.
	* @var	array
	* @see	findUndocumented()
	*/														
	var $undocumentedFields = array(
															"functions"	=> "function",
															"uses"			=> "included file",
															"consts"		=> "constant"
													);

	/**
	* Sets the data of the modulegroup to analyse.
	* 
	* @param	array	Raw modulegroup data from the parser.
	* @access	public
	*/
	function setModulegroup($modulegroup) {
	
		$this->modulegroup = $modulegroup;	
		
	} // end func setModulegroup
	
	function analyse() {

		$this->flag_get = false;
		
		$this->buildModulelist();
		
		$this->updateAccessReturn();
		$this->updateBrothersSisters();
		
		$this->checkFunctionArgs();
		$this->findUndocumented();
		
	} // end func analyse
	
	/**
	* Returns a module from the modulegroup or false if there are no more modules.
	*
	* @return	mixed		False if there no more modules in the modulegroup otherwise
	* 								an array with the data of a module.
	* @access	public
	*/
	function getModule() {
	
		if (!$this->flag_get) {
			reset($this->modulelist);
			$this->flag_get = true;
		}
			
		if (list($modulename, $group)=each($this->modulelist)) {
			
			$module = $this->modulegroup[$group][$modulename];
			unset($this->modulegroup[$group][$modulename]);			
			return $module;
			
		} else {
		
			return false;
			
		}
		
	} // end func getModule
	
	function findUndocumented() {

		reset($this->modulegroup);
		while (list($group, $modules)=each($this->modulegroup)) {
			
			reset($modules);
			while (list($name, $module)=each($modules)) {
				
				reset($this->undocumentedFields);
				while (list($index, $eltype)=each($this->undocumentedFields)) {
					if (!isset($module[$index]))
						continue;
						
					$file = $module["filename"];
					
					reset($module[$index]);
					while (list($elname, $data)=each($module[$index]))
						if (isset($data["undoc"]) && $data["undoc"])
							$this->warn->addDocWarning($file, $eltype, $elname, "Undocumented element.", "missing");
				}
				
			}
			
		}		

	} // end func findUndocumented
	
	function checkFunctionArgs() {
	
		reset($this->modulegroup);
		while (list($group, $modules)=each($this->modulegroup)) {

			reset($modules);
			while (list($name, $module)=each($modules)) {
				if (!isset($module["functions"]))
					continue;

				$file = $module["filename"];
								
				reset($module["functions"]);
				while (list($fname, $function)=each($module["functions"])) {
					$this->modulegroup[$group][$name]["functions"][$fname]["params"] = $this->checkArgDocs($function["args"], $function["params"], $fname, $file, false);
					unset($this->modulegroup[$group][$name]["functions"][$fname]["args"]);
				}
				
			}
			
		}

	} // end func checkFunctionArgs
	
	/**
	* Builds an internal list of all modules in the modulegroup.
	* @see	$modulelist, $modulegroup
	*/
	function buildModulelist() {
	
		$this->modulelist = array();
		
		reset($this->modulegroup);
		while (list($group, $modules)=each($this->modulegroup)) {
		
			reset($modules);
			while (list($modulename, $data)=each($modules))
				$this->modulelist[$modulename] = $group;
				
		}
		
	}

		
	function updateBrothersSisters() {
	
		reset($this->modulelist);
		while (list($modulename, $group)=each($this->modulelist)) {
			$this->updateBrotherSisterElements($group, $modulename, "functions");
			$this->updateBrotherSisterElements($group, $modulename, "variables");
		}	
		
	} // end func updateBrothersSisters
	
	function updateBrotherSisterElements($group, $modulename, $type) {
		
		if (!isset($this->modulegroup[$group][$modulename][$type])) 
			return false;
			
		reset($this->modulegroup[$group][$modulename][$type]);
		while (list($elementname, $data)=each($this->modulegroup[$group][$modulename][$type])) {
			
			if (isset($data["brother"])) {

				$name = ( "functions" == $type ) ? substr($data["brother"], 0, -2) : substr($data["brother"], 1);
				$name = strtolower($name);

				if (!isset($this->modulegroup[$group][$modulename][$type][$name])) {
					$this->warn->addDocWarning($this->modulegroup[$group][$modulename]["filename"], $type, $elementname, "Brother '$name' is unknown. Tags gets ignored.", "mismatch");
					unset($this->modulegroup[$group][$modulename][$type][$elementname]["brother"]);
				} else {
					$this->modulegroup[$group][$modulename][$type][$elementname]["brother"] = $name;
				}

			}
			
		}
		
	} // end func updateBrotherSistersElements
	
	function updateAccessReturn() {
		
		reset($this->modulelist);
		while (list($modulename, $group)=each($this->modulelist)) {
		
			if (!isset($this->modulegroup[$group][$modulename]["access"]))
				$this->modulegroup[$group][$modulename]["access"] = "private";
				
			$this->updateAccessReturnElements($group, $modulename, "functions");
			$this->updateAccessReturnElements($group, $modulename, "variables");
			$this->updateAccessElements($group, $modulename, "consts");		
			
		}
				
	} // end func updateAccessReturn
	
	function updateAccessReturnElements($group, $modulename, $type) {
		
		if (!isset($this->modulegroup[$group][$modulename][$type]))
			return false;

		reset($this->modulegroup[$group][$modulename][$type]);
		while (list($elementname, $data)=each($this->modulegroup[$group][$modulename][$type])) {
		
			if (!isset($data["access"])) 
				$this->modulegroup[$group][$modulename][$type][$elementname]["access"] = "private";
				
			if (!isset($data["return"]))
				$this->modulegroup[$group][$modulename][$type][$elementname]["return"] = "void";
				
		}
				
	} // end func updateAccessReturnElements
	
	function updateAccessElements($group, $modulename, $type) {
		
		if (!isset($this->modulegroup[$group][$modulename][$type]))
			return false;
			
		reset($this->modulegroup[$group][$modulename][$type]);
		while (list($elementname, $data)=each($this->modulegroup[$group][$modulename][$type])) {
			
			if (!isset($data["access"])) 
				$this->modulegroup[$group][$modulename][$type][$elementname]["access"] = "private";
		
		}
		
	} // end func updateAccessElements
	
} // end class PhpdocModuleAnalyser
?>