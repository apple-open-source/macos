<?php
/**
* Provides functioninality to render modules and classes.
*/
class PhpdocHTMLDocumentRenderer extends PhpdocHTMLRenderer {

	/**
	* Message displayed if an object lacks documentation.
	* @var	string	$undocumented
	* @access	public
	*/
	var $undocmented = "Warning: documentation is missing.";

	/**
	* Array of functions found in the xml document.
	* @var	array	$functions
	*/
	var $functions = array();	
	
	/**
	* Array of included files.
	* @var	array	$uses
	*/
	var $uses = array();
	
	/**
	* Array of constants.
	* @var	array	$constants
	*/
	var $constants = array();
	
	/**
	* Array of access modifiers.
	* @var	array	$accessModifiers
	*/
	var $accessModifiers = array("public", "private");

	/**
	* Array of doc container fields that get mapped directly to templateblocks.
	* @var	array	$simpleDocfields
	* @see	renderVariableDetail()
	*/	
	var $simpleDocfields = array(
																"VERSION"			=> "version", 
																"SINCE"				=> "since",
																"DEPRECATED"	=> "deprecated", 
																"COPYRIGHT"		=> "copyright", 
																"MAGIC"				=> "magic" 
															);

	/**
	* Known types for @include.
	* @var	array	$usesTypes
	* @see	renderUses()
	*/															
	var $usesTypes = array( "include", "include_once", "require", "require_once" );

	/**
	* Adds a summary and a detailed list of all constants to the template.
	* @see	renderConstantSummary(), renderConstantDetail()
	*/																			
	function renderConstants() {
		
		$this->constants["public"] 	= $this->accessor->getConstantsByAccess("public");
		$this->constants["private"] = $this->accessor->getConstantsByAccess("private");
		
		if (0==count($this->constants["public"]) && 0==count($this->constants["private"]))
			return;
		
		$this->renderConstantSummary();
		$this->renderConstantDetail();
		
		$this->constants = array();
		
	} // end func renderConstants

	/**
	* Adds a summary of all constants to the template.
	*
	* The function assumes that there is a block called "constantssummary" and
	* withing this block a bock called "constantssummary_loop" in the template.
	* 
	* @see	renderConstantDetail()
	*/	
	function renderConstantSummary() {
	
		reset($this->accessModifiers);
		while (list($k, $access)=each($this->accessModifiers)) {
			if (0==count($this->constants[$access])) 
				continue;
			
			$this->tpl->setCurrentBlock("constantssummary_loop");

			reset($this->constants[$access]);
			while (list($name, $const)=each($this->constants[$access])) {
			
				$this->tpl->setVariable("NAME", $name);
				$this->tpl->setVariable("VALUE", htmlentities($const["value"]));			
				if (isset($const["doc"]["shortdescription"]))
					$this->tpl->setVariable("SHORTDESCRIPTION", $this->encode($const["doc"]["shortdescription"]["value"]));
					
				if ("true" == $const["undoc"])
					$this->tpl->setVariable("UNDOC", $this->undocumented);
					
				$this->tpl->parseCurrentBlock();
			
			}
		
			$this->tpl->setCurrentBlock("constantssummary");
			$this->tpl->setVariable("ACCESS", ucfirst($access));
			$this->tpl->parseCurrentBlock();
			
		}
			
	} // end func renderConstantSummary

	/** 
	* Adds a detailed list of all constants to the template.
	* 
	* The function assumes that there is a block named "constantdetails" and
	* withing it another block named "constantdetails_loop". 
	*
	* @see	renderConstantSummary()
	*/	
	function renderConstantDetail() {

		reset($this->accessModifiers);
		while (list($k, $access)=each($this->accessModifiers)) {
			if (0==count($this->constants[$access]))
				continue;
			
			reset($this->constants[$access]);
			while (list($name, $constant)=each($this->constants[$access])) {
			
				$tplvars = array();
			
				$tplvars["NAME"]	=	$name;
				$tplvars["CASE"]	= $constant["case"];
				$tplvars["VALUE"]	=	htmlentities($constant["value"]);
			
				if ("true" == $constant["undoc"])
					$tplvars["UNDOC"] = $this->undocumented;

				if (isset($constant["doc"]["shortdescription"]))
					$tplvars["SHORTDESCRIPTION"] = $this->encode($constant["doc"]["shortdescription"]["value"]);
				if (isset($constant["doc"]["description"]))
					$tplvars["DESCRIPTION"] = $this->encode($constant["doc"]["description"]["value"]);

				$this->renderCommonDocfields("constantdetails_", $constant);
			
				$this->tpl->setCurrentBlock("constantdetails_loop");
				$this->tpl->setVariable($tplvars);
				$this->tpl->parseCurrentBlock();
			}
		
			$this->tpl->setCurrentBlock("constantdetails");
			$this->tpl->setVariable("ACCESS", ucfirst($access));
			$this->tpl->parseCurrentBlock();
		}	
	} // end func renderConstantsDetail

	/**
	* Adds a summary and a detailed list of included files to the template.
	* @see	renderUsesSummary(), renderUsesDetail()
	*/		
	function renderUses() {
	
		$found = false;
		
		reset($this->usesTypes);
		while (list($k, $type)=each($this->usesTypes)) {
		
			$this->uses[$type] = $this->accessor->getUsesByType($type);
			if (!$found && 0!=count($this->uses[$type]))
				$found = true;
				
		}

		if (!$found)
			return;
			
		$this->renderUsesSummary();
		$this->renderUsesDetail();
		
		$this->uses = array();
		
	} // end func renderUses															

	/**
	* Adds a detailed list of all @included files to the template.
	* 
	* The function assumes that there is a block names "usesdetail" and within the block 
	* a block names "usesdetail_loop" in the template.
	*
	* @see	renderUsesSummary()
	*/	
	function renderUsesDetail() {
		
		reset($this->usesTypes);
		while (list($k, $type)=each($this->usesTypes)) {
			if (0==count($this->uses[$type]))
				continue;
				
			reset($this->uses[$type]);
			while (list($file, $uses)=each($this->uses[$type])) {
				
				$tplvars = array();
				$tplvars["FILE"]	= $uses["file"];
				$tplvars["TYPE"]	= $type;
				
				if ("true"==$uses["undoc"])
					$tplvars["UNDOC"] = $this->undocumented;
					
				if (isset($uses["doc"]["shortdescription"]))
					$tplvars["SHORTDESCRIPTION"] = $this->encode($uses["doc"]["shortdescription"]["value"]);
				if (isset($uses["doc"]["description"]))
					$tplvars["DESCRIPTION"] = $this->encode($uses["doc"]["description"]["value"]);
				
				$this->renderCommonDocfields("usesdetails_", $uses);
				
				$this->tpl->setCurrentBlock("usesdetails_loop");
				$this->tpl->setVariable($tplvars);
				$this->tpl->parseCurrentBlock();
			}
			
			$this->tpl->setCurrentBlock("usesdetails");
			$this->tpl->setVariable("TYPE", $type);
			$this->tpl->parseCurrentBlock();
			
		}
		
	} // end func renderUsesDetail

	/** 
	* Adds a summary of all @included files to the template.
	* 
	* The function assumes that there is a block names "usessummary" and within
	* the block another block names "usessummary_loop" in the template.
	* 
	* @see	renderUsesDetail()
	*/	
	function renderUsesSummary() {
	
		reset($this->usesTypes);
		while (list($k, $type)=each($this->usesTypes)) {
			if (0==count($this->uses[$type]))
				continue;
				
			$this->tpl->setCurrentBlock("usessummary_loop");
			
			reset($this->uses[$type]);
			while (list($file, $uses)=each($this->uses[$type])) {
				
				$this->tpl->setVariable("FILE", $file);
				
				if (isset($uses["doc"]["shortdescription"]))
					$this->tpl->setVariable("SHORTDESCRIPTION", $this->encode($uses["doc"]["shortdescription"]["value"]));
					
				if ("true"==$uses["undoc"])
					$this->tpl->setVariable("UNDOC", $this->undocumented);
					
				$this->tpl->parseCurrentBlock();
					
			}
			
			$this->tpl->setCurrentBlock("usessummary");
			$this->tpl->setVariable("TYPE", $type);
			$this->tpl->parseCurrentBlock();
			
		}
		
	} // end func renderUsesSummary
	
	/**
	* Adds a summary and a detailed list of all functions to the template.
	* @see	renderFunctionSummary(), renderFunctionDetail(), $functions
	*/
	function renderFunctions() {
	
		$this->functions["private"] = $this->accessor->getFunctionsByAccess("private");
		$this->functions["public"]	= $this->accessor->getFunctionsByAccess("public");
		
		if (0==count($this->functions["private"]) && 0==count($this->functions["public"]))
			return;
		
		$this->renderFunctionSummary();
		$this->renderFunctionDetail();
		
		$this->functions = array();
		
	} // end func renderFunctions
	
	/**
	* Adds a function summary to the template.
	* 
	* The function assumes that there is ablock names "functionsummary" and 
	* within it a block names "functionsummary_loop" in the template. 
	*
	* @see	renderFunctionDetail(), renderFunctions(), $functions, $accessModifiers
	*/	
	function renderFunctionSummary() {
		
		reset($this->accessModifiers);
		while (list($k, $access)=each($this->accessModifiers)) {
			if (0==count($this->functions[$access])) 
				continue;			

			$this->tpl->setCurrentBlock("functionsummary_loop");
			
			reset($this->functions[$access]);
			while (list($name, $function)=each($this->functions[$access])) {
				
				$this->tpl->setVariable("NAME", $name);
				
				if (isset($function["doc"]["parameter"]))
					$this->tpl->setVariable("PARAMETER", $this->getParameter($function["doc"]["parameter"]));
					
				if (isset($function["doc"]["shortdescription"]))
					$this->tpl->setVariable("SHORTDESCRIPTION", $this->encode($function["doc"]["shortdescription"]["value"]));
				
				if (isset($function["doc"]["return"]))
					$this->tpl->setVariable("RETURNTYPE", $function["doc"]["return"]["type"]);
				else
					$this->tpl->setVariable("RETURNTYPE", "void");
					
				if ("true"==$function["undoc"])
					$this->tpl->setVariable("UNDOC", $this->undocumented);
					
				$this->tpl->parseCurrentBlock();
				
			}
			
			$this->tpl->setCurrentBlock("functionsummary");				
			$this->tpl->setVariable("ACCESS", ucfirst($access) );
			$this->tpl->parseCurrentBlock();
		}
		
	} // end func renderFunctionSummary
	
	/**
	* Adds a detailed list of functions to the template.
	*
	* The function assumes that there is a block named "functiondetails" and 
	* within it a bloc "functiondetails_loop" in the template.
	*
	* @see	renderFunctions(), renderFunctionSummary(), $functions, $accessModifiers
	*/
	function renderFunctionDetail() {

		reset($this->accessModifiers);
		while (list($k, $access)=each($this->accessModifiers)) {
			if (0==count($this->functions[$access]))
				continue;
			
			reset($this->functions[$access]);
			while (list($name, $function)=each($this->functions[$access])) {
				
				$tplvars = array();
				$tplvars["NAME"] 		= $function["name"];
				$tplvars["ACCESS"]	= $function["access"];
		
				if ("true"==$function["undoc"])
					$tplvars["UNDOC"]  = $this->undocumented;
			
				if ("true"==$function["abstract"])
					$tplvars["ABSTRACT"] = "abstract";
				
				if ("true"==$function["static"])
					$tplvars["STATIC"] = "static";
					
				if (isset($function["doc"]["shortdescription"]))
					$tplvars["SHORTDESCRIPTION"] = $this->encode($function["doc"]["shortdescription"]["value"]);
				if (isset($function["doc"]["description"]))
					$tplvars["DESCRIPTION"] = $this->encode($function["doc"]["description"]["value"]);
				
				$this->renderCommonDocfields("functiondetails_", $function);

				if (isset($function["doc"]["parameter"])) {
					$tplvars["PARAMETER"] = $this->getParameter($function["doc"]["parameter"]);
					$this->renderParameterDetail($function["doc"]["parameter"]);
				}
				
				if (isset($function["doc"]["throws"]))
					$this->renderThrows($function["doc"]["throws"], "functiondetails_");
				
				if (isset($function["doc"]["global"])) 
					$this->renderGlobals($function["doc"]["global"], "functiondetails_");

				if (isset($function["doc"]["return"])) {

					$tplvars["RETURNTYPE"] = $function["doc"]["return"]["type"];					

					$this->tpl->setCurrentBlock("functiondetails_return");
					
					$this->tpl->setVariable("TYPE", $function["doc"]["return"]["type"]);
					$this->tpl->setVariable("DESCRIPTION", $this->encode($function["doc"]["return"]["value"]));
					if (isset($function["doc"]["return"]["name"]))
						$this->tpl->setVariable("NAME", $function["doc"]["return"]["name"]);
						
					$this->tpl->parseCurrentBlock();
					
				} else {
				
					$tplvars["RETURNTYPE"] = "void";
					
				}
				
				$this->tpl->setCurrentBlock("functiondetails_loop");	
				$this->tpl->setVariable($tplvars);
				$this->tpl->parseCurrentBlock();	
			}

			$this->tpl->setCurrentBlock("functiondetails");
			$this->tpl->setVariable("ACCESS", ucfirst($access) );
			$this->tpl->parseCurrentBlock();
			
		}
		
	} // end func renderFunctionDetail
	
	/**
	* Renders a detailed list of function parameters.
	*
	* The function assumes that there is a block named "functiondetails_parameter" in 
	* the template and within it a block named "functiondetails_parameter_loop".
	*
	* @param	array	Parameter
	*/
	function renderParameterDetail($parameter) {
		
		if (!isset($parameter[0]))
			$parameter = array($parameter);
			
		$this->tpl->setCurrentBlock("functiondetails_parameter_loop");
			
		reset($parameter);
		while (list($k, $param)=each($parameter)) {

			$this->tpl->setVariable("NAME",	$param["name"]);
			$this->tpl->setVariable("DESCRIPTION", $this->encode($param["value"]));

			if (isset($param["type"]))
				$this->tpl->setVariable("TYPE", $param["type"]);
				
			if (isset($param["default"]))
				$this->tpl->setVariable("DEFAULT", "= >>".htmlentities($param["default"])."<<");
				
			if ("true" == $param["undoc"])
				$this->tpl->setVariable("UNDOC", $this->undocumented);
				
			$this->tpl->parseCurrentBlock();			
		}
		
	} // end func renderParameterDetail

	/**
	* Converts the XML parameter array into formatted string.
	* @param	array	XML parameter array
	* @return	string	Formatted parameter string
	*/
	function getParameter($parameter) {
		
		if (!is_array($parameter))
			return "void";
		
		$value = "";
		
		if (!isset($parameter[0])) {
			
			if (!isset($parameter["default"]))
				$value.= $parameter["type"]." ".$parameter["name"];
			else
				$value.= "[ ".$parameter["type"]." ".$parameter["name"]." ]";
				
		} else {
			
			$flag_optional = false;
			
			reset($parameter);
			while (list($k, $param)=each($parameter)) {
			
				if (!isset($param["default"])) {
					
					if ($flag_optional) {
						$value = substr($value, 0, -2)." ], ";
						$flag_optional = false;
					}
					
				} else {
				
					if (!$flag_optional) {
						$value.= "[ ";
						$flag_optional = true;
					}
				}
				
				$value.= $param["type"]." ".$param["name"].", ";
			}
			
			$value = substr($value, 0, -2);
			if ($flag_optional)
				$value.= " ]";
				
		}
		
		return $value;		
	} // end func getParameter

	/**
	* Renders a block with references to other source elements, @see.
	* @param	array		XML references array
	* @param	string	optional template blockname prefix
	*/	
	function renderSee($references, $prefix="") {

		$value = "";		
		
		if (!isset($references[0])) {
			
			if (isset($references["group"]))
				$value.= sprintf('<a href="%s#%s_%s">%s::%s</a>',
														$this->nameToUrl($references["group"]).$this->file_extension,
														$references["type"],
														$references["value"],
														$references["group"],
														$references["value"] 
												);
			else 
				$value.= sprintf('<a href="#%s_%s">%s</a>',
														$references["type"],
														$references["value"],
														$references["value"]
													);
													
		} else {
				
			reset($references);
			while (list($k, $reference)=each($references)) {
			
				if (isset($reference["group"]))
					$value.= sprintf('<a href="%s#%s_%s">%s::%s</a>, ',
															$this->nameToUrl($reference["group"]).$this->file_extension,
															$reference["type"],
															$reference["value"],
															$reference["group"],
															$reference["value"] 
													);
				else 
					$value.= sprintf('<a href="#%s_%s">%s</a>, ',
															$reference["type"],
															$reference["value"],
															$reference["value"]
														);
				
			}	
			
			$value = substr($value, 0, -2);
		
		}
		
		$this->tpl->setCurrentBlock(strtolower($prefix)."see");
		$this->tpl->setVariable("SEE", $value);
		$this->tpl->parseCurrentBlock();
		
	} // end func renderSee

	/**
	* Renders an author list, @author.
	* @param	array		XML author array
	* @param	string	optional template blockname prefix
	*/	
	function renderAuthors($authors, $prefix="") {
	
		$value = "";
			
		if (!isset($authors[0])) {
			
			if (isset($authors["email"]))
				$value.= sprintf('%s &lt;<a href="mailto:%s">%s</a>&gt;, ', $authors["value"], $authors["email"], $authors["email"]);
			else 
				$value.= $authors["email"].", ";
			
		} else {
		
			reset($authors);
			while (list($k, $author)=each($authors)) {
				
				if (isset($author["email"]))
					$value.= sprintf('%s &lt;<a href="mailto:%s">%s</a>&gt;, ', $author["value"], $author["email"], $author["email"]);
				else 
					$value.= $author["email"].", ";
				
			}
			
		}

		$value = substr($value, 0, -2);
		$this->tpl->setCurrentBlock(strtolower($prefix)."authors");
		$this->tpl->setVariable("AUTHOR", $value);
		$this->tpl->parseCurrentBlock();
		
	} // end func renderAuthors
	
	/**
	* Renders a list of external links, @link.
	* @param	array		XML link array
	* @param	string	optional template blockname prefix
	*/
	function renderLinks($links, $prefix="") {
	
		$value= "";
		
		if (!isset($links[0])) {
			
			$value.= sprintf('<a href="%s">%s</a>%s, ', 
													$links["url"], 
													$links["url"], 
													(""!=$links["description"]) ? " - ".$links["description"] : ""
												);
			
		} else {
			
			reset($links);
			while (list($k, $link)=each($links)) {
	
				$value.= sprintf('<a href="%s">%s</a>%s, ', 
														$link["url"], 
														$link["url"], 
														(""!=$link["description"]) ? " - ".$links["description"] : ""
													); 
																									
			}
			
		}
		
		$value = substr($value, 0, 2);
		
		$this->tpl->setCurrentBlock(strtolower($prefix)."links");
		$this->tpl->setVariable("LINK", $value);
		$this->tpl->parseCurrentBlock();

	} // end func renderLinks
	
	/**
	* Renders a list of exceptions, @throws.
	* @param	array		XML array 
	* @param	string	optional template blockname prefix
	*/
	function renderThrows($throws, $prefix="") {
		
		$value = "";
		
		if (!isset($throws[0])) {
			
			$value = $throws["value"];
			
		}	else {
			
			reset($throws);
			while (list($k, $exception)=each($throws)) 
				$value.= sprintf("%s, ", $exception["value"]);
				
			$value = substr($value, 0, -2);
			
		}	

		$this->tpl->setCurrentBlock(strtolower($prefix)."throws");
		$this->tpl->setVariable("EXCEPTIONS", $value);
		$this->tpl->parseCurrentBlock();
		
	} // end func renderThrows
	
	/**
	* Renders a list of global elements, @global
	* @param	array		XML globals array
	* @param	string	optional template blockname prefix
	*/
	function renderGlobals($globals, $prefix="") {
	
		$prefix = strtolower($prefix);
		$this->tpl->setCurrentBlock($prefix."globals_loop");
		
		if (!isset($globals[0])) {
			
			$this->tpl->setVariable("NAME", $globals["name"]);
			$this->tpl->setVariable("DESCRIPTION", $this->encode($globals["value"]));
			if (isset($globals["type"]))
				$this->tpl->setVariable("TYPE", $globals["type"]);
				
			$this->tpl->parseCurrentBlock();
			
		} else {
		
			reset($globals);
			while (list($k, $global)=each($globals)) {
			
				$this->tpl->setVariable("NAME", $global["name"]);
				$this->tpl->setVariable("DESCRIPTION", $this->encode($global["value"]));
				if (isset($globals["type"]))
					$this->tpl->setVariable("TYPE", $globals["type"]);
					
				$this->tpl->parseCurrentBlock();
				
			}
			
		}
		
	} // end func renderGlobals

	/**
	* Adds some tags to the template that are allowed nearly everywhere.
	* @param	string	template blockname prefixs
	* @param	array		
	* @see	$simpleDocfields, renderLinks(), renderAuthors(), renderSee()
	*/	
	function renderCommonDocfields($block, &$data) {

		reset($this->simpleDocfields);
		while (list($varname, $field)=each($this->simpleDocfields)) {
		
			if (isset($data["doc"][$field])) {
				$this->tpl->setCurrentBlock($block.$field);
				$this->tpl->setVariable($varname, htmlentities($data["doc"][$field]["value"]));
				$this->tpl->parseCurrentBlock();
			}
			
		}
		
		if (isset($data["doc"]["link"]))
			$this->renderLinks($data["doc"]["link"], $block);
					
		if (isset($data["doc"]["author"])) 
			$this->renderAuthors($data["doc"]["author"], $block);
					
		if (isset($data["doc"]["see"]))
			$this->renderSee($data["doc"]["see"], $block);
		
	} // end func renderCommonDocfields

} // end func PhpdocHTMLDocumentRenderer
?>
