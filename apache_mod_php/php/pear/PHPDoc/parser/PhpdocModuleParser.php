<?php
/**
* Extracts modules and their documentation from php code.
* @author	Ulf Wendel <ulf.wendel@redsys.de>
* @version 0.1alpha
*/
class PhpdocModuleParser extends PhpdocConstantParser {

	/**
	* Empty hash that shows the structure of a module.
	* @var	array
	*/
	var $emptyModule = array(
	
														"name"				=> "",
														"group"				=> "",
														"undoc"				=> true,
														
														"functions"		=> array(),
														"consts"			=> array(),
														"uses"				=> array()
												);

	/**
	* List of tags allowed within a module doc comment.
	* @var	array	tagname => true
	*/													
	var $moduleTags = array(
														"module"			=> true,
														"modulegroup"	=> true,
														
														"access"			=> true,
														
														"see"					=> true,
														"link"				=> true,
														
														"author"			=> true,
														"copyright"		=> true,
														
														"version"			=> true,
														"since"				=> true,
														
														"deprecated"	=> true,
														"deprec"			=> true,
														
														"brother"			=> true,
														"sister"			=> true,
														
														"exclude"		=> true,
														
														"package"			=> true,
														
														"magic"				=> true,
														"todo"				=> true
												);

	/**
	* Hash of all module groups
	* @var	array
	*/
	var $moduleGroups = array();
	
	/**
	* Central module parsing function.
	*
	* @param	array		Array of parsing data
	* @return	array		
	* @see	analyseModuleDoc()
	*/
	function analyseModule($para) {
		
		$module = $this->analyseModuleDoc($para["modules"]);			
		unset($para["modules"]);

		$this->moduleGroups[$module["group"]][] = $module["name"];

		reset($para["functions"]);
		while (list($k, $data)=each($para["functions"]))
			$module["functions"][strtolower($data["name"])] = $this->analyseFunction($data);
		unset($para["functions"]);
			
		reset($para["consts"]);
		while (list($k, $data)=each($para["consts"]))
			$module["consts"][strtolower($data["name"])] = $this->analyseConstant($data);
		unset($para["const"]);
		
		reset($para["uses"]);
		while (list($k, $data)=each($para["uses"]))
			$module["uses"][strtolower($data["file"])] = $this->analyseUse($data);
		
		return $module;
	} // end func analyseModule
	
	/**
	* Extracts the allowed documentation tags out of a module doc comment.
	* 
	* @param	array	Module paragraph
	* @return	array		
	*/
	function analyseModuleDoc($para) {
	
		$module = $this->emptyModule;
		$module["name"] = (""!=$para["name"]) ? $para["name"] : $this->currentFile;
		$module["group"] = (""!=$para["group"]) ? $para["group"] : $this->currentFile;
		
		if ("missing" == $para["status"]) {
			
			$msg = "The file '$this->currentFile' does not contain any classes and seems to lack a module doc comment.";
			$this->warn->addDocWarning($this->currentFile, "module", $module["name"], $msg, "missing");
			
		} else if ("tags missing" == $para["status"]) {
		
			$msg = "The module doc comment does not contain a @module or @modulegroup tag, the module gets names: '$this->currentFile'";
			$this->warn->addDocWarning($this->currentFile, "module", $module["name"], $msg, "missing");
		
		}
		
		if (""!=$para["doc"]) {		
			
			$tags 	= $this->getTags($para["doc"]);
			$module = $this->analyseTags($tags, $module, $this->moduleTags);
			
			list($msg, $module) = $this->checkParserErrors($module, "module");
			if (""!=$msg) 
				$this->warn->addDocWarning($this->currentFile, "module", $module["name"], $msg, "mismatch");
			
			list($shortdesc, $fulldesc) = $this->getDescription($para["doc"]);			
			$module["sdesc"] = $shortdesc;
			$module["desc"]  = $fulldesc;
		
			$module["undoc"] = false;
		}

		unset($module["module"]);
		unset($module["modulegroup"]);
		
		return $module;		
	} // end analyseModuleDoc
	
} // end class PhpdocModuleParser
?>