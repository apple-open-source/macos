<?php
/**
* Looks for documented and undocumented functions within a block of php code
* @author	Ulf Wendel <ulf@redsys.de>
* @version 0.2
*/
class PhpdocFunctionParser extends PhpdocVariableParser {

	/**
	* Internal structur of a function.
	* @var	array	$emptyFunction
	* @see	functions, analyseFunctionParagraph()
	*/
	var $emptyFunction = array(
	
															"name"					=> "",
															"undoc"					=> true,
															
															"args"					=> array()
												);
		
	/**
	* Array of tags that are allowed in front of the function keyword
	* @var	array	$functionTags
	* @see	analyseFunctionParagraph()
	*/
	var $functionTags = array(
															"parameter"	 	=> true,
															"param"				=> true,
															
															"return"			=> true,
															
															"access"			=> true,
															"abstract"		=> true,
															"static"			=> true,
															
															"throws"			=> true,
															
															"see"					=> true,
															"link"				=> true,
															
															"global"			=> true,
															
															"version"			=> true,
															"since"				=> true,
															
															"deprecated"	=> true,
															"deprec"			=> true,
															
															"brother"			=> true,
															"sister"			=> true,
															
															"exclude"		=> true,
															"magic"				=> true,
															
															"author"			=> true,
															"copyright"		=> true,
															
															"todo"				=> true
											);
	
	/**
	* Analyses a function doc comment.
	* @param	array
	* @return array
	*/
	function analyseFunction($para) {
	
		$function = $this->emptyFunction;
		$function["name"] = $para["name"];		

		if (""!=$para["doc"]) {

			$function = $this->analyseTags($this->getTags($para["doc"]), $function, $this->functionTags);
			
			list($msg, $function) = $this->checkParserErrors($function, "function");
			if (""!=$msg) 
				$this->warn->addDocWarning($this->currentFile, "function", $function["name"], $msg, "mismatch");
			
			list($function["sdesc"], $function["desc"]) = $this->getDescription($para["doc"]);
			
			$function["undoc"] = false;
			
		} 

		$function["args"] = $this->getFunctionArgs($para["head"]);			
		return $function;
	} // end func analyseFunction
	
	/**
	* Analyses a function head and returns an array of arguments.
	* @param	string	PHP code to examine.
	* @return	array		Array of arguments: $args[] = array( optional, default, type, name ).
	* @see	getVariableTypeAndValue()
	*/				
	function getFunctionArgs($code) {

		$args = array();
		while (preg_match($this->PHP_COMPLEX["argument"], $code, $regs)) {

			$type 		= "";
			$value 		= "";
			$optional = false;
		
			if (!isset($regs[3])) {
				
				$len_of_value = strlen($regs[1]);
				
			} else if ("=" == $regs[3]) {
		
				$find 	= $regs[1].$regs[2];
				$code 	= substr($code, strpos($code, $find)+strlen($find) );
				
				list ($type, $value, $raw_value) = $this->getVariableTypeAndValue($code);
				$len_of_value = strlen($raw_value);
				$optional = true;
			
			} else {
				
				$len_of_value = strlen($regs[1].$regs[2]);
				
			}
			
			$code = substr($code, $len_of_value);
			$args[] = array(
												"optional" => $optional, 
												"default"  => $value,
												"type"		 => $type,
												"name"		 => $regs[1]
											);
														
		}

		return $args;
	} // end func getFunctionArgs
	
} // end class PhpdocFunctionParser
?>