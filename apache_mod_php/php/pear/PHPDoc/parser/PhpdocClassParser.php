<?php
/**
* Parses phpcode to extract classes and their documentation.
*
* @version	0.2
* @author		Ulf Wendel, ulf@redsys.de
*/
class PhpdocClassParser extends PhpdocFunctionParser {

  /**
  * Array of all classes in the given code
	* 
	* The array is indexed by the classname.
	* See $emptyClass to see the internal structure.
	* 
  * @var    array $classes
  * @see		emptyClass
  */ 
  var $classes = array();
	
	/**
	* Default values of a class
	* @var	array	$emptyClass
	* @see	classes, analyseClassParagraph
	*
	*/
	var $emptyClass = array (
															"name"		=> "",
															"extends"	=> "",
															"undoc"		=> true
												);	
	
	/**
	* Array of tags that are allowed in front of the class keyword
	* @var	array	$classTags
	* @see	analyseClassParagraph()
	*/
	var $classTags = array(
															"access"			=> true,
															"abstract"		=> true,
															"static"			=> true,
															"final"				=> true,
															
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
	* Analyse a class
	* 
	* Calls all neccessary analyse functions.
	* 
	* @param	array
	* @return	array
	*/
	function analyseClass($para) {

		$class = $this->analyseClassDoc($para["classes"][0]);
		
		reset($para["functions"]);
		while (list($k, $data)=each($para["functions"]))
			$class["functions"][strtolower($data["name"])] = $this->analyseFunction($data);
		unset($para["functions"]);
			
		reset($para["variables"]);
		while (list($k, $data)=each($para["variables"]))
			$class["variables"][strtolower($data["name"])] = $this->analyseVariable($data);
		unset($para["variables"]);

		reset($para["consts"]);
		while (list($k, $data)=each($para["consts"]))
			$class["consts"][strtolower($data["name"])] = $this->analyseConstant($data);
		unset($para["consts"]);
		
		reset($para["uses"]);
		while (list($k, $data)=each($para["uses"]))
			$class["uses"][strtolower($data["file"])] = $this->analyseUse($data);
		
		return $class;
	} // end func analyseClass

	/**
	* Analyses a class doc comment.
	* @param	array	Hash returned by getPhpdocParagraph()
	* @return	array
	*/	
	function analyseClassDoc($para) {
	
		$class 	= $this->emptyClass;
		$class["name"] 		= $para["name"];
		$class["extends"] = $para["extends"];
		
		if (""!=$para["doc"]) {
			
			$class = $this->analyseTags($this->getTags($para["doc"]), $class, $this->classTags);
			
			list($msg, $class) = $this->checkParserErrors($class, "class");
			if (""!=$msg)
				$this->warn->addDocWarning($this->currentFile, "class", $class["name"], $msg, "mismatch");
				
			list($class["sdesc"], $class["desc"]) = $this->getDescription($para["doc"]);
			
			$class["undoc"] = false;
		}
		
		return $class;
	} // end func analyseClassDoc
	
} // end class PhpdocClassParser
?>