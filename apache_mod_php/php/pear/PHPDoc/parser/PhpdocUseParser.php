<?php
/**
* Extracts use statements (include and friends) an thheir documentation from php code.
* @author	Ulf Wendel <ulf.wendel@redsys.de>
* @version 0.1alpha
*/
class PhpdocUseParser extends PhpdocParserCore {

	/**
	* Structure of an empty use entry.
	* @var	array
	*/
	var $emptyUse = array(
													"type"	=> "",
													"file"	=> "",
													"undoc"	=> true
											);
											

	/**
	* List of allowed tags in use doc comments.
	* @var	array
	*/												
	var $useTags = array(
												"return"			=> true,
												
												"see"					=> true,
												"link"				=> true,
												
												"authhor"			=> true,
												"copyright"		=> true,
												
												"version"			=> true,
												"since"				=> true,
												
												"deprecated"	=> true,
												"deprec"			=> true,
												
												"include"			=> true,

												"exclude"			=> true,												
												"magic"				=> true,
												"todo"				=> true
											);

	/**
	* Takes the result from getPhpdocParagraphs() and interprets it.
	* @param	array
	*/											
	function analyseUse($para) {
		
		$use = $this->emptyUse;
		$use["file"] = $para["file"];
		
		if (""!=$para["doc"]) {
		
			$use = $this->analyseTags($this->getTags($para["doc"]), $use, $this->useTags);
			
			list($msg, $use) = $this->checkParserErrors($use, "use (include and friends)");
			if (""!=$msg)
				$this->warn->addDocWarning($this->currentFile, "use", $use["file"], $msg, "mismatch");
				
			list($use["sdesc"], $use["desc"]) = $this->getDescription($para["doc"]);
			
			$use["undoc"] = false;
		}
		
		$use["type"] = $para["type"];

		return $use;
	} // end func analyseUse
	
} // end class PhpdocUseParser
?>