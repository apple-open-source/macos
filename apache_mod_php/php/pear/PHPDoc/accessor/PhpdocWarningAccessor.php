<?php
/**
* Extracts the warnings from PHPDoc warnings_* files.
* 
*/
class PhpdocWarningAccessor extends PhpdocAccessor {

	/**
	* If set to true all get_xy() functions will free their resources.
	* @var		boolean
	* @access	public
	*/
	var $freeOnGet = true;	
	
	/**
	* Array of warnings.
	* @var	array
	*/
	var $warnings = array();
	
	/**
	* Flag used to detect if get_xy() was called.s
	* @var	boolean
	*/
	var $flag_build = false;
	
	/**
	* Returns a hash of warnings in of the given XML file.
	*
	* @param 	string	XML file
	* @return	array
	* @access	public
	* @see	$freeOnGet
	*/
	function getWarnings($xmlfile) {
		
		$this->buildWarnings($xmlfile);
		
		if ($this->freeOnGet) {
			
			$data = $this->warnings; 
			$this->warnings = array();
			return $data;
			
		} else {
			
			return $this->warnings;
			
		}
		
	} // end func getWarnings
	
	/**
	* Build the internal list of warnings.
	*
	* @param	string	XML file to load
	*/
	function buildWarnings($xmlfile) {
		
		if ($this->flag_build)
			return;
			
		$this->flag_build = true;
		$this->warnings		= array();
		$this->loadXMLFile($xmlfile);

		if(!isset($this->xml["warnings"][0]))
			$this->xml["warnings"] = array( $this->xml["warnings"] );
		
		reset($this->xml["warnings"]);
		while (list($k, $warnings)=each($this->xml["warnings"])) {
		
			$file = $warnings["file"];
			if (!isset($warnings["warning"][0])) 
				$warnings["warning"] = array($warnings["warning"]);
			$this->warnings[$file] = $warnings["warning"];
					
		}

		$this->xml = "";
		
	} // end func buildWarnings
	
	function init() {
		$this->flag_build = false;
	} // end func init
	
} // end class PhpdocWarningAccess
?>