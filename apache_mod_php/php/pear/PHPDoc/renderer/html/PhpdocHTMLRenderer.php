<?php
/**
* Default HTML Renderer based on templates.
*/
class PhpdocHTMLRenderer extends PhpdocRendererObject {
	
	/**
	* Template object
	* @var	object	IntegratedTemplates	$tpl
	*/	
	var $tpl;
	
	/**
	* XML data accessor object.
	* @var	object	PhpdocAccessor
	*/
	var $accessor;
	
	/**
	* Rootpath for Templatefiles.
	* @var	string	$templateRoot
	* @see	setTemplateRoot()
	*/
	var $templateRoot = "";
	
	/**
	* Directory path prefix.
	* @var	string	$path
	*/
	var $path = "";
	
	/**
	* Sets a directory path prefix.
	*
	* @param	string	
	*/
	function setPath($path) {
	
		if (""!=$path && "/"!=substr($path, -1))
			$path.="/";
			
		$this->path = $path;
	} // end func path
	
	/**
	* Sets the template directory.
	*
	* @param	string
	*/
	function setTemplateRoot($templateRoot) {
		
		if (""!=$path && "/"!=substr($templateRoot, -1))
			$templateRoot.="/";
			
		$this->templateRoot = $templateRoot;
	} // end func setTemplateRoot
	
	/**
	* Encodes the given string.
	* 
	* This function gets used to encode all userdependend 
	* elements of the phpdoc xml files. Use it to 
	* customize your rendering result: beware newlines (nl2br()),
	* strip tags etc.
	*
	* @param	string	String to encode
	* @return	string	$string	Encoded string
	*/
	function encode($string) {
		return nl2br(htmlspecialchars($string));
	} // end func encode
	
} // end class PhpdocHTMLRenderer
?>