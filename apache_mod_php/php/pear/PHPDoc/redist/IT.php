<?php
/**
* Integrated Template - IT
* 
* Well there's not much to say about it. I needed a template class that
* supports a single template file with multiple (nested) blocks inside.
* 
* Usage:
* $tpl = new IntegratedTemplate( [string filerootdir] );
* 
* // load a template or set it with setTemplate()
* $tpl->loadTemplatefile( string filename [, boolean removeUnknownVariables, boolean removeEmptyBlocks] )
*
* // set "global" Variables meaning variables not beeing within a (inner) block
* $tpl->setVariable( string variablename, mixed value );
* 
* // like with the Isotopp Templates there's a second way to use setVariable()
* $tpl->setVariable( array ( string varname => mixed value ) );
* 
* // Let's use any block, even a deeply nested one
* $tpl->setCurrentBlock( string blockname );
*
* // repeat this as often as you neer. 
* $tpl->setVariable( array ( string varname => mixed value ) );
* $tpl->parseCurrentBlock();
*
* // get the parsed template or print it: $tpl->show()
* $tpl->get();
* 
* @author	  Ulf Wendel <uw@netuse.de>
* @version  1.1 28/09/00
* @access		public
*/
class IntegratedTemplate {

	/**
	* Contains the error objects
	* @var		array
	* @access	public
	* @see		halt(), $printError, $haltOnError
	*/
	var $err = array();
	
	/**
	* Print error messages?
	* @var		boolean
	* @access	public
	* @see		halt(), $haltOnError, $err
	*/
	var $printError = false;
	
	/**
	* Call die() on error?
	* @var		boolean
	* @access	public
	* @see		halt(), $printError, $err
	*/
	var $haltOnError = false;
	
	/**
	* Clear cache on get()? 
	* @var	boolean
	*/ 
	var $clearCache = false;
		
	/**
	* First character of a variable placeholder ( _{_VARIABLE} ).
	* @var		string
	* @access	public
	* @see		$closingDelimiter, $blocknameRegExp, $variablenameRegExp
	*/
	var $openingDelimiter = "{";
	
	/**
	* Last character of a variable placeholder ( {VARIABLE_}_ ).
	* @var		string
	* @access	public
	* @see		$openingDelimiter, $blocknameRegExp, $variablenameRegExp
	*/
	var $closingDelimiter 	= "}";
	
	/**
	* RegExp matching a block in the template. 
	* Per default "sm" is used as the regexp modifier, "i" is missing.
	* That means a case sensitive search is done.
	* @var		string
	* @access	public
	* @see		$variablenameRegExp, $openingDelimiter, $closingDelimiter
	*/
	var $blocknameRegExp	= "[0-9A-Za-z_-]+";
	
	/**
	* RegExp matching a variable placeholder in the template.
	* Per default "sm" is used as the regexp modifier, "i" is missing.
	* That means a case sensitive search is done.
	* @var		string	
	* @access	public
	* @see		$blocknameRegExp, $openingDelimiter, $closingDelimiter
	*/
	var $variablenameRegExp	= "[0-9A-Za-z_-]+";
	
	/**
	* Full RegExp used to find variable placeholder, filled by the constructor.
	* @var	string	Looks somewhat like @(delimiter varname delimiter)@
	* @see	IntegratedTemplate()
	*/
	var $variablesRegExp = "";
	
	/**
	* Full RegExp used to find blocks an their content, filled by the constructor.
	* @var	string
	* @see	IntegratedTemplate()
	*/
	var $blockRegExp = "";
	
	/**
	* Name of the current block.
	* @var		string
	*/
	var $currentBlock = "__global__";

	/**
	* Content of the template.
	* @var		string
	*/	
	var $template = "";
	
	/**
	* Array of all blocks and their content.
	* 
	* @var	array
	* @see	findBlocks()
	*/	
	var $blocklist 			= array();
  
  /**
  * Array with the parsed content of a block.
  *
  * @var  array
  */
  var $blockdata      = array();
	
	/**
	* Array of variables in a block.
	* @var	array
	*/
	var $blockvariables = array();

	/**
	* Array of inner blocks of a block.
	* @var	array
	*/	
	var $blockinner 		= array();
	
  /**
  * Future versions will use this...
  * @var  array
  */
  var $blocktypes = array();
  
	/**
	* Variable cache.
	*
	* Variables get cached before any replacement is done.
	* Advantage: empty blocks can be removed automatically.
	* Disadvantage: might take some more memory
	* 
	* @var	array
	* @see	setVariable(), $clearCacheOnParse
	*/
	var $variableCache = array();
	
	/**
	* @var	boolean
	*/
	var $clearCacheOnParse = true;
	
	/**
	* Controls the handling of unknown variables, default is remove.
	* @var	boolean
	*/
	var $removeUnknownVariables = true;
	
	/**
	* Controls the handling of empty blocks, default is remove.
	* @var	boolean
	*/
	var $removeEmptyBlocks = true;
	
	/**
	* Root directory for all file operations. 
	* The string gets prefixed to all filenames given.
	* @var	string
	* @see	IntegratedTemplate(), setRoot()
	*/
	var $fileRoot = "";
	
	/**
	* Internal flag indicating that a blockname was used multiple times.
	* @var	boolean
	*/
	var $flagBlocktrouble = false;
	
	/**
	* Flag indicating that the global block was parsed.
	* @var	boolean
	*/
	var $flagGlobalParsed = false;
	
	/**
	* Builds some complex regular expressions and optinally sets the file root directory.
	*
	* Make sure that you call this constructor if you derive your template 
	* class from this one. 
	*
	* @param	string	File root directory, prefix for all filenames given to the object.
	* @see	setRoot()
	*/
	function IntegratedTemplate($root = "") {
	
		$this->variablesRegExp = "@".$this->openingDelimiter."(".$this->variablenameRegExp.")".$this->closingDelimiter."@sm";
		$this->blockRegExp = '@!--\s+BEGIN\s+('.$this->blocknameRegExp.')\s+-->(.*)<!--\s+END\s+\1\s+-->@sm';

		$this->setRoot($root);		
		
	} // end constructor
	
	/**
	* Print a certain block with all replacements done.
	* @brother get()
	*/
	function show($block = "__global__") {
		print $this->get($block);
	} // end func show
	
	/**
	* Returns a block with all replacements done.
	* 
	* @param	string 	name of the block
	* @return	string  	
	* @access	public
	* @see	show()
	*/
	function get($block = "__global__") {

		if ("__global__" == $block && !$this->flagGlobalParsed)
			$this->parse("__global__");
			
		if (!isset($this->blocklist[$block])) {
			$this->halt("The block '$block' was not found in the template.", __FILE__, __LINE__);
			return true;
		}
		
    if ($this->clearCache) {
    
      $data = (isset($this->blockdata[$block])) ? $this->blockdata[$block] : "";
      unset($this->blockdata[$block]);
      return $data;
       
    } else {
    
      return (isset($this->blockdata[$block])) ? $this->blockdata[$block] : "";
      
    }

	} // end func get()
		
	/**
	* Parses the given block.
	*	
	* @param	string	name of the block to be parsed
	* @access	public
	* @see		parseCurrentBlock()
	*/
	function parse($block = "__global__", $flag_recursion = false) {

		if (!isset($this->blocklist[$block])) {
			$this->halt("The block '$block' was not found in the template.", __FILE__, __LINE__);
			return false;
		}

		if ("__global__" == $block)
			$this->flagGlobalParsed = true;
			
    $regs = array();
    $values = array();

		if ($this->clearCacheOnParse) {
			
			reset($this->variableCache);
			while (list($name, $value)=each($this->variableCache)) {
				$regs[] = "@".$this->openingDelimiter.$name.$this->closingDelimiter."@";
				$values[] = $value;
			}
			$this->variableCache = array();
		
		} else {
		
			reset($this->blockvariables[$block]);
			while (list($k, $allowedvar)=each($this->blockvariables[$block])) {
		
				if (isset($this->variableCache[$allowedvar])) {
 	    	  $regs[]   = "@".$this->openingDelimiter.$allowedvar.$this->closingDelimiter."@";
	   	    $values[] = $this->variableCache[$allowedvar];
					unset($this->variableCache[$allowedvar]);
				}

			}		
			
		}

		$outer = preg_replace($regs, $values, $this->blocklist[$block]);
		$empty = (0==count($values)) ? true : false;

    if (isset($this->blockinner[$block])) {
		
      reset($this->blockinner[$block]);
      while (list($k, $innerblock)=each($this->blockinner[$block])) {

        $this->parse($innerblock, true);
				if (""!=$this->blockdata[$innerblock])
					$empty = false;

				$placeholder = $this->openingDelimiter."__".$innerblock."__".$this->closingDelimiter;				
        $outer = str_replace($placeholder, $this->blockdata[$innerblock], $outer);
				$this->blockdata[$innerblock] = "";				
      }
    }

    if ($this->removeUnknownVariables)
			$outer = preg_replace($this->variablesRegExp, "", $outer);
		
		if ($empty) {
		
			if (!$this->removeEmptyBlocks) 
				$this->blockdata[$block].= $outer;
				
		} else {
		
			$this->blockdata[$block].= $outer;
		
		}

    return $empty;
	} // end func parse

	/**
	* Parses the current block
	* @see	parse(), setCurrentBlock(), $currentBlock
	*	@access	public
	*/
	function parseCurrentBlock() {
		return $this->parse($this->currentBlock);
	} // end func parseCurrentBlock

	/**
	* Sets a variable value.
	* 
	* The function can be used eighter like setVariable( "varname", "value")
	* or with one array $variables["varname"] = "value" given setVariable($variables)
	* quite like phplib templates set_var().
	* 
	* @param 	mixed		string with the variable name or an array %variables["varname"] = "value"
	* @param	string	value of the variable or empty if $variable is an array.
	* @param	string	prefix for variable names
	* @access	public
	*/	
	function setVariable($variable, $value="") {
		
		if (is_array($variable)) {
		
			reset($variable);
			while (list($var, $value)=each($variable)) 
				$this->variableCache[$var] 	= $value;
				
		} else {
			
			$this->variableCache[$variable] 	= $value;
			
		}
	
	} // end func setVariable
	
	/**
	* Sets the name of the current block that is the block where variables are added.
	*
	* @param	string	name of the block 
	* @return	boolean	false on failure otherwise true
	*	@access	public
	*/
	function setCurrentBlock($block = "__global__") {
	
		if (!isset($this->blocklist[$block])) {
			$this->halt("Can't find the block '$block' in the template.", __FILE__, __LINE__);
			return false;
		}
			
		$this->currentBlock = $block;
		
		return true;
	} // end func setCurrentBlock
	
	/**
	* Clears all datafields of the object and rebuild the internal blocklist
	* 
	* LoadTemplatefile() and setTemplate() automatically call this function 
	* when a new template is given. Don't use this function 
	* unless you know what you're doing.
	*
	* @access	public
	* @see	free()
	*/
	function init() {
	
		$this->free();
		$this->findBlocks($this->template);
		$this->buildBlockvariablelist();
		
	} // end func init
	
	/**
	* Clears all datafields of the object.
	* 
	* Don't use this function unless you know what you're doing.
	*
	* @access	public
	* @see	init()
	*/
	function free() {
	
		$this->err[] = "";
		
		$this->currentBlock = "__global__";
		
		$this->variableCache 	= array();		
		$this->blocklist 			= array();
		$this->blockvariables	= array();
		$this->blockinner			= array();
		$this->blockdata			= array();
		$this->blocklookup		= array();
    $this->blocktypes     = array();
		
		$this->flagBlocktrouble = false;
		$this->flagGlobalParsed	= false;
		
	} // end func free
	
	/**
	* Sets the template.
	*  
	* You can eighter load a template file from disk with LoadTemplatefile() or set the
	* template manually using this function.
	* 
	* @param		string	template content
	* @param		boolean	Unbekannte, nicht ersetzte Platzhalter entfernen?
	* @param		boolean	remove unknown/unused variables?
	* @param		boolean	remove empty blocks?
	* @see			LoadTemplatefile(), $template
	* @access		public
	*/
	function setTemplate($template, $removeUnknownVariables = true, $removeEmptyBlocks = false) {
		if (""==$template) {
			$this->halt("The given string is empty.", __FILE__, __LINE__);
			return false;
		}
		
		$this->removeUnknownVariables = $removeUnknownVariables;
		$this->removeEmptyBlocks = $removeEmptyBlocks;
		
		$this->template = '<!-- BEGIN __global__ -->'.$template.'<!-- END __global__ -->';
		$this->init();
		
		if ($this->flagBlocktrouble)
			return false;
		
		return true;
	} // end func setTemplate
	
	/**
	* Reads a template file from the disk.
	*
	* @param		string	name of the template file, full path!
	* @param		boolean	remove unknown/unused variables?
	* @param		boolean	remove empty blocks?
	* @access		public
	* @return		boolean	false on failure, otherwise true
	* @see			$template, setTemplate()
	*/
	function loadTemplatefile($filename, $removeUnknownVariables = true, $removeEmptyBlocks = true) {
	
		$template = $this->getfile($filename);
		
		return $this->setTemplate($this->getFile($filename), $removeUnknownVariables, $removeEmptyBlocks);
	} // end func LoadTemplatefile
	
	/**
	* Sets the file root. The file root gets prefixed to all filenames passed to the object.
	* 
	* Make sure that you override this function when using the class
	* on windows.
	* 
	* @param	string
	* @see		IntegratedTemplate()
	* @access	public
	*/
	function setRoot($root) {
		
		if (""!=$root && "/"!= substr($root, -1))
			$root.="/";
		
		$this->fileRoot = $root;
		
	} // end func setRoot

	/**
	* Build a list of all variables within a block
	*/	
	function buildBlockvariablelist() {

		reset($this->blocklist);
		while (list($name, $content)=each($this->blocklist)) {
			preg_match_all( $this->variablesRegExp, $content, $regs );
			$this->blockvariables[$name] = $regs[1];
		}	
		
	} // end func buildBlockvariablelist
	
	/**
	* Returns a list of all 
	*/
	function getGlobalvariables() {

    $regs   = array();
    $values = array();
    
		reset($this->blockvariables["__global__"]);
		while (list($k, $allowedvar)=each($this->blockvariables["__global__"])) {
			
			if (isset($this->variableCache[$allowedvar])) {
				$regs[]   = "@".$this->openingDelimiter.$allowedvar.$this->closingDelimiter."@";
				$values[] = $this->variableCache[$allowedvar];
				unset($this->variableCache[$allowedvar]);
			}
			
		}
		
    return array($regs, $values);
	} // end func getGlobalvariables

	/**
	* Recusively builds a list of all blocks within the template.
	*
	* @param	string	string that gets scanned
	* @access	private
	* @see	$blocklist
	*/	
	function findBlocks($string) {

		$blocklist = array();

		if (preg_match_all($this->blockRegExp, $string, $regs, PREG_SET_ORDER)) {
			
			reset($regs);
			while (list($k, $match)=each($regs)) {
			
				$blockname 		= $match[1];
				$blockcontent = $match[2];
			
				if (isset($this->blocklist[$blockname])) {
					$this->halt("The name of a block must be unique within a template. Found '$blockname' twice. Unpredictable results may appear.", __FILE__, __LINE__);
					$this->flagBlocktrouble = true;
				}				

				$this->blocklist[$blockname] = $blockcontent;
				$this->blockdata[$blockname] = "";

				$blocklist[] = $blockname;
				
				$inner = $this->findBlocks($blockcontent);
				reset($inner);
				while (list($k, $name)=each($inner)) {

					$pattern = sprintf('@<!--\s+BEGIN\s+%s\s+-->(.*)<!--\s+END\s+%s\s+-->@sm', 
													$name,
													$name
												);

					$this->blocklist[$blockname] = preg_replace(	$pattern, 
																												$this->openingDelimiter."__".$name."__".$this->closingDelimiter, 
																												$this->blocklist[$blockname]
																											);
					$this->blockinner[$blockname][] = $name;
					$this->blockparents[$name] = $blockname;
					
				}
				
			}
			
		}

		return $blocklist;
	} // end func findBlocks

	/**
	* Reads a file from disk and returns its content.
	* @param	string	Filename
	* @return	string	Filecontent
	*/	
	function getFile($filename) {
		
		if ("/" == substr($filename, 0, 1)) 
			$filename = substr($filename, 1);
			
		$filename = $this->fileRoot.$filename;
		
		if ( !($fh = @fopen($filename, "r")) ) {
			$this->halt("Can't read '$filename'.", __FILE__, __LINE__);
			return "";
		}
	
		$content = fread($fh, filesize($filename));
		fclose($fh);
		
		return $content; 
	} // end func getFile
	
	/**
	* Error Handling function.
	* @param	string	error message
	* @param	mixed		File where the error occured
	* @param	int			Line where the error occured
	* @see		$err
	*/
	function halt($message, $file="", $line=0) {
		
		$message = sprintf("IntegratedTemplate Error: %s [File: %s, Line: %d]",
															$message,
															$file,
															$line
													);

		$this->err[] = $message;
																
		if ($this->printError)
			print $message;
			
		if ($this->haltOnError)
			die($message);															

	} // end func halt
	
} // end class IntegratedTemplate
?>