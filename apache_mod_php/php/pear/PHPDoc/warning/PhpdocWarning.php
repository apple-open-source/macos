<?php
/**
* Container for all kind of Warnings the parser/analyser recognizes
* 
* The base of the report generator module is this container. It's currently 
* pretty simple and will change later on...
*/
class PhpdocWarning extends PhpdocObject {
	
	/**
	* Hash of documentation failures.
	* @var	array	
	*/
	var $doc_warnings = array();
	
	/**
	* Counter containing the number of documentation warnings.
	* @var	int
	* @see	getNumDocWarnings(), getNumWarnings()
	*/
	var $num_doc_warnings = 0;
	
	/**
	* Adds a warning to the list of class documentation failures.
	* @param	string	Name of the file that containts the error
	* @param	string	Kind of the element that caused the error: module, class, function, variable, use, const
	* @param	string	Name of the class/function/... that caused the warning
	* @param	string	Warning message itself
	* @para,	string	Type of the error: missing, mismatch, syntax, ...
	* @access	public
	* @see	addDocWarning()
	*/
	function addDocWarning($file, $elementtype, $elementname, $warning, $type="missing") {

		$this->doc_warnings[$file][$elementtype][] =	array(
																												"name"	=> $elementname,
																												"type"	=> $type,
																												"msg"		=> $warning
																												);
		$this->num_doc_warnings++;
		
	} // end func addDocWarning

	/**
	* Returns a list of warnings.
	*
	* @return	array	$warnings
	* @access	public
	*/		
	function getWarnings() {
		return $this->doc_warnings;
	} // end func getParserWarnings
	
	/**
	* Returns the total number of documentation warnings.
	* @access	public
	* @see	getNumParserWarnings(), getNumWarnings()
	*/
	function getNumDocWarnings() {
		return $this->num_doc_warnings;
	} // end func getNumDocWarnings
	
} // end class PhpdocWarning
?>