<?php
/**
* PHPDoc Error Handling class
*
* PHPDoc "throws" an error class derived from this class whenever
* an error occurs. PHPDoc saves the error object to the public array
* $this->err[] which exists in every PHPDoc class and tries to return 
* a useful value that indicates that something might have gone wrong.
*
* The class is widely equal to the PEAR error handling class PEAR_ERROR.
*
* @author		Ulf Wendel <ulf.wendel@phpdoc.de>
* @version 	1.0
* @package	PHPDoc
*/
class PhpdocError {
	
	/**
	* Name of the error object class used to construct the error message
	* @var		string	$classname
	*/
	var $classname 						= "PhpdocError";
	
	/**
	* Error message prefix.
	* @var		string	$error_message_prefix
	*/
	var $error_message_prefix	= "";

	/**
	* Error prepend, used for HTML formatting.
	* @var	string	$error_prepend
	*/	
	var $error_prepend = "<b>";
	
	/**
	* Error append, used for HTML formatting.
	* @var	string	$error_append
	*/
	var $error_append = "</b>";
	
	/**
	* The error message itself.
	*
	*	Use getMessage() to access it.
	*
	* @var	string	$message
	* @see	PhpdocError
	*/
	var $message = "";
	
	/**
	* File where the error occured.
	* @var	string	$file
	* @see	PhpdocError()
	*/
	var $file = "";
	
	/**
	* Line number where the error occured.
	* @var	int	$line
	* @see	PhpdocError()
	*/
	var $line = 0;
	
	/**
	* Array that describes how an error gets handled. 
	* @var	array	$errorHandling
	* @see	PhpdocError()
	*/
	var $errorHandling = array(
															"print"		=> false, 
															"trigger"	=> false,
															"die"			=> false
														);
	
	/**
	* Sets the error message, filename and linenumber.
	* @param	string	Errormessage
	* @param	string	Name of the file where the error occured, use __FILE__ for this
	* @param	string 	Linenumber where the error occured, use __LINE__ for this
	*/
	function PhpdocError($message, $file, $line) {
	
		$this->message = $message;
		$this->file = $file;
		$this->line = $line;

		if ($this->errorHandling["print"])
			$this->printMessage();
		
		if ($this->errorHandling["trigger"])
			trigger_error($this->getMessage(), "E_USER_NOTICE");
			
		if ($this->errorHandling["die"])
			die($this->getMessage);
		
	} // end func PhpdocError

	/**
	* Returns a string with the error message.
	* @access	public
	*/	
	function getMessage() {
	
		return sprintf("%s%s: %s [File: %s, Line: %s]%s",
										$this->error_prepend,
										$this->error_message_prefix,
										$this->message,
										$this->file,
										$this->line, 
										$this->error_append);
										
	} // end func getMessage
	
	/**
	* Prints the error message.
	* @brother	getMessage()
	*/
	function printMessage() {
		print $this->getMessage();
	} // end func printMessage
	
} // end class PhpdocError
?>