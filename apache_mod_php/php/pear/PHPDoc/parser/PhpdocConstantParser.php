<?php
/**
* Extracts define statements and their documentation from php code.
* @author	Ulf Wendel <ulf.wendel@redsys.de>
* @version 0.1alpha
*/
class PhpdocConstantParser extends PhpdocUseParser {

	var $emptyConstant = array( 
															"name"						=> "",
															"value"						=> "",
															"undoc"						=> true
														);
														
	var $constantTags = array(
															"access"		=> true,
															"see"				=> true,
															"link"			=> true,
															
															"constant"	=> true,
															"const"			=> true,
															
															"author"		=> true,
															"copyright"	=> true,
															
															"exclude"	=> true,
															"magic"			=> true,
															"todo"			=> true
													);

	function analyseConstant($para) {
	
		$constant = $this->emptyConstant;
		$constant["name"] = $para["name"];
		$constant["value"] = $para["value"];
		
		if (""!=$para["doc"]) {
		
			$constant = $this->analyseTags( $this->getTags($para["doc"]), $constant, $this->constantTags);
		
			list($msg, $constant) = $this->checkParserErrors($constant, "constant (define() keyword)");
			if (""!=$msg)
				$this->warn->addDocWarning($this->currentFile, "constant", $constant["name"], $msg, "mismatch");
				
			list($constant["sdesc"], $constant["desc"])=$this->getDescription($para["doc"]);

			$constant["undoc"] = false;			
		}
		
		$constant = $this->checkConstantDoc($constant);
		
		if (isset($para["case"]))
			$constant["case"] = $para["case"];
		
		return $constant;
	} // end func analyseConstant
	
	/**
	* Compares the data from the parser with the optional @const[ant] tags
	* @param	array	Hash with the data of the current constant paragraph
	* @return	array $constant
	*/
	function checkConstantDoc($constant) {
	
		if (!isset($constant["const"])) {
		
			$msg = "The @const[ant] tag is missing. Add '@const ".$constant["name"]." [description]' to the tag list at the end of the doc comment.";
			$this->warn->addDocWarning($this->currentFile, "constant", $constant["name"], $msg, "missing");

		} else {
			
			if ($constant["name"] != $constant["const"]["name"]) {
				
				$msg = sprintf("The name of the constant '%s' does not match the documented name '%s', update the tag to '@const %s %s'.",
												$constant["name"],
												$constant["const"]["name"],
												$constant["name"],
												$constant["const"]["desc"]
											);
				$this->warn->addDocWarning($this->currentFile, "constant", $constant["name"], $msg, "mismatch");
				
			}

			if (""!=$constant["const"]["desc"])		
				$constant["const"] = $constant["const"]["desc"];
			else 
				unset($constant["const"]);
		}
		
		return $constant;
	} // end func checkConstantDoc
														
} // end class PhpdocConstantParser
?>