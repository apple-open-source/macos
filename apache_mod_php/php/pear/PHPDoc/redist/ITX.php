<?php
/**
* Integrated Template Extension - ITX
*
* With this class you get the full power of the phplib template class. 
* You may have one file with blocks in it but you have as well one main file 
* and multiple files one for each block. This is quite usefull when you have
* user configurable websites. Using blocks not in the main template allows
* you to some parts of your layout easily. 
*
* Note that you can replace an existing block and add new blocks add runtime.
* Adding new blocks means changing a variable placeholder to a block.
*
* @author 	Ulf Wendel <uw@netuse.de>
* @access		public
* @version 	1.0 22/09/00
*/
class IntegratedTemplateExtension extends IntegratedTemplate {

	/**
	* Array with all warnings.
	* @var		array
	* @access	public
	* @see		$printWarning, $haltOnWarning, warning()
	*/
	var $warn = array();
	
	/**
	* Print warnings?
	* @var		array
	* @access	public
	* @see		$haltOnWarning, $warn, warning()
	*/
	var $printWarning = false;
	
	/**
	* Call die() on warning?
	* @var 		boolean
	* @access	public
	* @see		$warn, $printWarning, warning()
	*/
	var $haltOnWarning = false;
		
	/**
	* RegExp used to test for a valid blockname.
	* @var	string
	*/	
	var $checkblocknameRegExp = "";
	
	/**
	* Builds some complex regexps and calls the constructor of the parent class.
	*
	* Make sure that you call this constructor if you derive you own 
	* template class from this one.
	*
	* @see	IntegratedTemplate()
	*/
	function IntegratedTemplateExtension() {
	
		$this->checkblocknameRegExp = "@".$this->blocknameRegExp."@";
		$this->IntegratedTemplate();
																							
	} // end func IntegratedTemplateExtension
	
	/**
	* Replaces an existing block with new content. Warning: not implemented yet.
	* 
	* The Replacement does not affect previously added variables. All data is cached.
	* In case the new block does contain less or other variable placeholder the previously
	* passed data that is no longer referenced will be deleted. The internal list 
	* of allowed variables gets updated as well.
	*
	* In case the original block contains other blocks it must eighter have placeholder
	* for the inner blocks or contain them. If you want to use placeholder the placeholder must
	* look like openingDelimiter."__".blockname."__".closingDelimiter .
	*
	* Due to the cache updates replaceBlock() and replaceBlockfile() are "expensive" operations 
	* which means extensive usage will slow down your script. So try to avoid them and if 
	* you can't do so try to use them before you pass lots of variables to the block you're 
	* replacing.
	* 
	* @param	string	Blockname
	* @param	string	Blockcontent
	* @return	boolean	
	* @see		replaceBlockfile(), addBlock(), addBlockfile()
	* @access	public
	*/
	function replaceBlock($block, $template) {
		if (!isset($this->blocklist[$block])) {
			$this->halt("The block '$block' does not exist in the template and thus it can't be replaced.", __FILE__, __LINE__);
			return false;
		}
		if (""==$template) {
			$this->halt("No block content given.", __FILE__, __LINE__);
			return false;
		}
		
		print "This function has not been coded yet.";
		
		// find inner blocks
		// add to variablelist
		// compare variable list
		// update caches
		
		return true;
	} // end func replaceBlock
	
	/**
	* Replaces an existing block with new content from a file. Warning: not implemented yet.
	* @brother replaceBlock()
	* @param	string	Blockname
	* @param	string	Name of the file that contains the blockcontent
	*/
	function replaceBlockfile($block, $filename) {
		return $this->replaceBlock($block, $this->getFile($filename));	
	} // end func replaceBlockfile
	
	/**
	* Adds a block to the template changing a variable placeholder to a block placeholder.
	*
	* Add means "replace a variable placeholder by a new block". 
	* This is different to PHPLibs templates. The function loads a 
	* block, creates a handle for it and assigns it to a certain 
	* variable placeholder. To to the same with PHPLibs templates you would 
	* call set_file() to create the handle and parse() to assign the
	* parsed block to a variable. By this PHPLibs templates assume that you tend
	* to assign a block to more than one one placeholder. To assign a parsed block
	* to more than only the placeholder you specify in this function you have
	* to use a combination of getBlock() and setVariable().
	*
	* As no updates to cached data is necessary addBlock() and addBlockfile() 
	* are rather "cheap" meaning quick operations.
	*
	* The block content must not start with <!-- BEGIN blockname --> and end with 
	* <!-- END blockname --> this would cause overhead and produce an error.
	* 
	* @param	string	Name of the variable placeholder, the name must be unique within the template.
	* @param	string	Name of the block to be added
	* @param	string	Content of the block
	* @return	boolean
	* @see		addBlockfile()
	* @access	public
	*/	
	function addBlock($placeholder, $blockname, $template) {
	
		// Don't trust any user even if it's a programmer or yourself...
		if (""==$placeholder) {
		
			$this->halt("No variable placeholder given.", __FILE__, __LINE__);
			return false;
			
		}	else if (""==$blockname || !preg_match($this->checkblocknameRegExp, $blockname) ) {
			
			print $this->checkblocknameRegExp;
			$this->halt("No or invalid blockname '$blockname' given.", __FILE__, __LINE__);
			return false;
			
		} else if (""==$template) {
		
			$this->halt("No block content given.", __FILE__, __LINE__);
			return false;
			
		} else if (isset($this->blocklist[$blockname])) {
		
			$this->halt("The block already exists.", __FILE__, __LINE__);
			return false;
			
		}
		
		// Hmm, we should do some more tests.
		$parents = $this->findPlaceholderBlocks($placeholder);
		if (0==count($parents)) {
		
			$this->halt("The variable placeholder '$placeholder' was not found in the template.", __FILE__, __LINE__);
			return false;
			
		} else if ( count($parents)>1 ) {
			
			reset($parents);
			while (list($k, $parent)=each($parents)) 
				$msg.= "$parent, ";
			$msg = substr($parent, -2);
			
			$this->halt("The variable placeholder '$placeholder' must be unique, found in multiple blocks '$msg'.", __FILE__, __LINE__);
			return false;
						
		}
		
		$template = "<!-- BEGIN $blockname -->".$template."<!-- END $blockname -->";
		$this->findBlocks($template);
		if ($this->flagBlocktrouble) 
			return false;	// findBlocks() already throws an exception
		
		$this->blockinner[$parents[0]][] = $blockname;
		$this->blocklist[$parents[0]] = preg_replace(	"@".$this->openingDelimiter.$placeholder.$this->closingDelimiter."@", 
																									$this->openingDelimiter."__".$blockname."__".$this->closingDelimiter, 
																									$this->blocklist[$parents[0]]
																								);
																								
		$this->deleteFromBlockvariablelist($parents[0], $placeholder);
		$this->updateBlockvariablelist($blockname);
		
		return true;
	} // end func addBlock
	
	/**
	* Adds a block taken from a file to the template changing a variable placeholder to a block placeholder. 
	* 
	* @param		string	Name of the variable placeholder to be converted
	* @param		string	Name of the block to be added
	* @param		string	File that contains the block
	* @brother	addBlock()
	*/
	function addBlockfile($placeholder, $blockname, $filename) {
		return $this->addBlock($placeholder, $blockname, $this->getFile($filename));
	} // end func addBlockfile
	
	/**
	* Deletes one or many variables from the block variable list.
	* @param	string	Blockname
	* @param	mixed		Name of one variable or array of variables ( array ( name => true ) ) to be stripped.
	*/
	function deleteFromBlockvariablelist($block, $variables) {
	
		if (!is_array($variables))
			$variables = array( $variables => true);
			
		reset($this->blockvariables[$block]);
		while (list($k, $varname)=each($this->blockvariables[$block])) 
			if (isset($variables[$varname])) 
				unset($this->blockvariables[$block][$k]);
					
	} // end deleteFromBlockvariablelist

	/**
	* Updates the variable list of a block.
	* @param	string	Blockname
	*/	
	function updateBlockvariablelist($block) {
		
		preg_match_all( $this->variablesRegExp, $this->blocklist[$block], $regs );
		$this->blockvariables[$block] = $regs[1];
			
	} // end func updateBlockvariablelist
	
	/**
	* Returns an array of blocknames where the given variable placeholder is used.
	* @param	string	Variable placeholder
	* @return	array	$parents	parents[0..n] = blockname
	*/
	function findPlaceholderBlocks($variable) {
		
		$parents = array();
		
		reset($this->blocklist);
		while (list($blockname, $content)=each($this->blocklist)) {
			
			reset($this->blockvariables[$blockname]);
			while (list($k, $varname)=each($this->blockvariables[$blockname]))
				if ($variable == $varname) 
					$parents[] = $blockname;
		}
			
		return $parents;
	} // end func findPlaceholderBlocks

	/**
	* Handles warnings, saves them to $warn and prints them or calls die() depending on the flags
	* @param	string	Warning
	* @param	string	File where the warning occured
	* @param	int			Linenumber where thr warning occured
	* @see		$warn, $printWarning, $haltOnWarning
	*/
	function warning($message, $file="", $line=0) {
		
		$message = sprintf("IntegratedTemplateExtension Warning: %s [File: %s, Line: %d]",
													$message,
													$file, 
													$line );

		$this->warn[] = $message;
		
		if ($this->printWarning)
			print $message;
			
		if ($this->haltOnError) 
			die($message);
		
	} // end func warning
	
} // end class IntegratedTemplateExtension
?>