<?php
/**
* Renders classes.
*/
class PhpdocHTMLClassRenderer extends PhpdocHTMLDocumentRenderer {

	/**
	* Internal array of "&nbsp;" strings to format HTML output.
	* @var	array	$indent
	*/
	var $indent = array();
	
	/**
	* Array of variables found in the xml document.
	* @var	array	$variables
	*/
	var $variables = array();

	/**
	* Sets the xml and template root directory.
	* 
	* @param	string	XML file path
	* @param	string	Template file path
	* @param	string	Name of the current application
	* @see	setPath(), setTemplateRoot()
	*/
	function PhpdocHTMLClassRenderer($path, $templateRoot, $application, $extension = ".html") {
	
		$this->setPath($path);
		$this->setTemplateRoot($templateRoot);
		$this->application = $application;
        $this->file_extension = $extension;
		
		$this->accessor = new PhpdocClassAccessor;
		$this->tpl = new IntegratedTemplate($this->templateRoot);
		$this->fileHandler = new PhpdocFileHandler;
		
	} // end constructor

	/**
	* Renders a class
	* @param	string	XML source file
	* @param	string	Name of the HTML target file.
	* @access	public
	*/	
	function renderClass($xmlfile, $htmlfile="") {

		$this->tpl->loadTemplatefile("class.html");	

		if (""==$htmlfile)
			$htmlfile = substr($xmlfile, 6, -4).$this->file_extension;

		$this->accessor->loadXMLFile($this->path.$xmlfile);
		
		$this->renderSubclasses();
		$this->renderInherited();
		$this->renderFunctions();
		$this->renderVariables();
		$this->renderUses();
		$this->renderConstants();
		
		$class = $this->accessor->getClassdata();

		$tplvars = array();
		
		$tplvars["CLASS_FILE"] 		= $class["file"]["value"];
		$tplvars["CLASS_NAME"] 		= $class["name"];
		$tplvars["CLASS_ACCESS"]	= $class["access"];
		$tplvars["CLASS_PACKAGE"]	= $class["package"];
	
		if (""!=$class["extends"])
			$tplvars["CLASS_EXTENDS"] = sprintf('extends <a href="%s">%s</a>', 
																						$class["extends"].$this->file_extension, 
																						$class["extends"]
																					);
			
		$tplvars["CLASS_UNDOC"] 	= ("true" == $class["undoc"]) ? $this->undocumented : "";
		
		$tplvars["CLASS_ABSTRACT"]= ("true" == $class["abstract"]) ? "abstract" : "";
		$tplvars["CLASS_STATIC"]	= ("true"	== $class["static"]) ? "static" : "";
		$tplvars["CLASS_FINAL"] 	= ("true" == $class["final"]) ? "final" : "";
		
		$tplvars["CLASS_TREE"] 		= $this->getClasstree($class["name"]);
		
		if (isset($class["doc"]["link"]))
			$this->renderLinks($class["doc"]["link"], "class_");
			
		if (isset($class["doc"]["author"]))
			$this->renderAuthors($class["doc"]["author"], "class_");
			
		if (isset($class["doc"]["see"]))
			$this->renderSee($class["doc"]["see"], "class_");
		
		$fields = array( 	"version", "deprecated", "copyright"	,
											"since", "magic");
		reset($fields);
		while (list($k, $field)=each($fields)) 
			if (isset($class["doc"][$field])) {
				$this->tpl->setCurrentBlock("class_".strtolower($field));
				$this->tpl->setVariable(strtoupper($field), $class["doc"][$field]["value"]);
				$this->tpl->parseCurrentBlock();
			}

		$fields = array( "description", "shortdescription" );
		reset($fields);
		while (list($k, $field)=each($fields)) 
			if (isset($class["doc"][$field]))
				$tplvars["CLASS_".strtoupper($field)] = $this->encode($class["doc"][$field]["value"]);

		$this->tpl->setCurrentBlock("__global__");				
		$this->tpl->setVariable($tplvars);
		$this->tpl->setVariable("APPNAME", $this->application);
		
		$this->fileHandler->createFile($this->path.$htmlfile, $this->tpl->get() );
		$this->tpl->free();	
		
	} // end func renderClass
	
		/**
	* Renders a list of inherited elements.
	* @see	renderInheritedElements()
	*/
	function renderInherited() {
		
		$this->renderInheritedElements(	$this->accessor->getInheritedFunctions(),
																		"inheritedfunctions",
																		"function"
																	);
																	
		$this->renderInheritedElements( $this->accessor->getInheritedVariables(),
																		"inheritedvariables",
																		"variable"
																	);

		$this->renderInheritedElements( $this->accessor->getInheritedConstants(),
																		"inheritedconstants",
																		"constant"
																	);																	

		$this->renderInheritedElements(	$this->accessor->getInheritedUses(),
																		"inheriteduses",
																		"uses"
																	);

	} // end func renderInherited
	
	/**
	* Renders a list of a certain inherited element.
	* @see	renderInheritedElements()
	*/
	function renderInheritedElements($inherited, $block, $type) {
		
		if (0==count($inherited))
			return;
			
		$this->tpl->setCurrentBlock($block);
		reset($inherited);
		while (list($source, $elements)=each($inherited)) {
			
			$value = "";
			reset($elements);
			while (list($k, $element)=each($elements))
				$value.= sprintf('<a href="%s#%s_%s">%s</a>, ', 
															$source.$this->file_extension,
															$type, 
															$element, 
															$element
													);
			$value = substr($value, 0, -2);
			
			$this->tpl->setVariable("SOURCE", $source);
			$this->tpl->setVariable("ELEMENTS", $value);
			$this->tpl->parseCurrentBlock();					
		}

	} // end func renderInheritedElements
	
	/**
	* Renders a list of direct known subclasses.
	*/
	function renderSubclasses() {
		
		$subclasses = $this->accessor->getSubclasses();
		if (0==count($subclasses)) 
			return;
		
		$elements = "";
		reset($subclasses);
		while (list($k, $subclass)=each($subclasses))
			$elements.= sprintf('<a href="%s">%s</a>, ', $subclass.$this->file_extension, $subclass);
		
		$elements	= substr($elements, 0, -2);
		
		if (""!=$elements) {
		
			$this->tpl->setCurrentBlock("subclasses");
			$this->tpl->setVariable("ELEMENTS", $elements);
			$this->tpl->parseCurrentBlock();

		}
	} // end func renderSubclasses
	
	/**
	* Adds a summary and a detailed list of all variables to the template.
	* @see	renderVariableSummary(), renderVariableDetail()
	*/
	function renderVariables() {
		
		$this->variables["private"] = $this->accessor->getVariablesByAccess("private");
		$this->variables["public"] 	= $this->accessor->getVariablesByAccess("public");
		
		if (0==count($this->variables["private"]) && 0==count($this->variables["public"]))
			return;
		
		$this->renderVariableSummary();
		$this->renderVariableDetail();

		$this->variables = array();
		
	} // end func renderVariables

	/**
	* Adds a summary of all variables to the template.
	* 
	* The function assumes that there is a block named "variablesummary" and
	* within it a block names "variablesummay_loop" in the template.
	*
	* @see	renderVariableDetail()
	*/	
	function renderVariableSummary() {	

		reset($this->accessModifiers);
		while (list($k, $access)=each($this->accessModifiers)) {
			if (0==count($this->variables[$access]))
				continue;
				
			$this->tpl->setCurrentBlock("variablesummary_loop");
			
			reset($this->variables[$access]);
			while (list($name, $variable)=each($this->variables[$access])) {
				
				$this->tpl->setVariable("NAME", $name);
				$this->tpl->setVariable("TYPE", $variable["type"]);
				
				if (isset($variable["doc"]["shortdescription"]))
					$this->tpl->setVariable("SHORTDESCRIPTION", $this->encode($variable["doc"]["shortdescription"]["value"]));
			
				$this->tpl->parseCurrentBlock();				
				
			}
			
			$this->tpl->setCurrentBlock("variablesummary");
			$this->tpl->setVariable("ACCESS", ucfirst($access));
			$this->tpl->parseCurrentBlock();
			
		}

	} // end func renderVariableSummary

	/**
	* Adds a detailed list of all variables to the template.
	* 
	* The function assumes that there is a block named "variabledetails"
	* and within it a block names "variablesdetails_loop" in the template.
	*
	* @see	renderVariableSummary()
	*/	
	function renderVariableDetail() {
		
		reset($this->accessModifiers);
		while (list($k, $access)=each($this->accessModifiers)) {
			if (0==count($this->variables[$access]))
				continue;
			
			reset($this->variables[$access]);
			while (list($name, $variable)=each($this->variables[$access])) {
			
				$tplvars = array();
				$tplvars["NAME"] 		=	$variable["name"];
				$tplvars["ACCESS"] 	= $variable["access"];
				$tplvars["TYPE"] 		= $variable["type"];
				$tplvars["VALUE"]		= htmlentities($variable["value"]);
				
				if ("true"==$variable["undoc"]) 
					$tplvars["UNDOC"] = $this->undocumented;
				
				if ("true"==$variable["static"])
					$tplvars["STATIC"] = "static";
					
				if ("true"==$variable["final"])
					$tplvars["FINAL"] = "final";

				if (isset($variable["doc"]["shortdescription"]))
					$tplvars["SHORTDESCRIPTION"] = $this->encode($variable["doc"]["shortdescription"]["value"]);
				if (isset($variable["doc"]["description"]))
					$tplvars["DESCRIPTION"] = $this->encode($variable["doc"]["description"]["value"]);
					
				$this->renderCommonDocfields("variabledetails_", $variable);
				
				$this->tpl->setCurrentBlock("variabledetails_loop");	
				$this->tpl->setVariable($tplvars);
				$this->tpl->parseCurrentBlock();	
			}

			$this->tpl->setCurrentBlock("variabledetails");
			$this->tpl->setVariable("ACCESS", ucfirst($access) );
			$this->tpl->parseCurrentBlock();
		}		
		
	} // end func renderVariableDetail

	/**
	* Returns a html string that shows the class tree.
	* @param	string	name of the current class
	* @return	string	HTML that shows the tree
	*/	
	function getClasstree($class) {
		
		$path = $this->accessor->getClasstree();
		
		$level = 0;
		$num = count($path)-1;
		
		for ($i = $num; $i>=0; $i--) {

			$indent = $this->getIndent($level);
			if ($level>0)
				$value.= sprintf("%s |<br>%s+-- ", $indent, $indent);
			$value.= sprintf('<a href="%s">%s</a><br>', 
												$path[$i].$this->file_extension,
												$path[$i]
											);
			$level++;
			
		}

		$indent = $this->getIndent($level);
		if ($level>0)
			$value.= sprintf("%s |<br>%s+-- ", $indent, $indent);
		$value.= sprintf('%s<br>', $class);
			
		return $value;
	} // end func getClasstree

	/**
	* Returns a certain number of "&nbsp;"s.
	* @param	int	number of "&nbsp;" required.
	* @see		$indent
	* @return	string	A string with the requested number of nunbreakable html spaces
	*/
	function getIndent($level) {
		
		if (!isset($this->indent[$level])) {
		
			$html = "";
			for ($i=0; $i<$level; $i++)
				$html.= "&nbsp;&nbsp;";
			
			$this->indent[$level] = $html;
			
		}
		
		return $this->indent[$level];
			
	} // end func getIndent
	
} // end class PhpdocHTMLClassRenderer
?>
