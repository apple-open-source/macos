<?php
/**
* Renders modules.
*/
class PhpdocHTMLModuleRenderer extends PhpdocHTMLDocumentRenderer {

	/**
	* Sets the xml and template root directory.
	* 
	* @param	string	XML file path
	* @param	string	Template file path
	* @param	string	Name of the current application
	* @see	setPath(), setTemplateRoot()
	*/
	function PhpdocHTMLModuleRenderer($path, $templateRoot, $application, $extension = ".html") {
	
		$this->setPath($path);
		$this->setTemplateRoot($templateRoot);
		$this->application = $application;
        $this->file_extension = $extension;
		
		$this->accessor = new PhpdocModuleAccessor;
		$this->tpl = new IntegratedTemplate($this->templateRoot);
		$this->fileHandler = new PhpdocFileHandler;
		
	} // end constructor

	/**
	* Renders a module
	* @param	string	XML source file
	* @param	string	Name of the HTML target file.
	* @access	public
	*/	
	function renderModule($xmlfile, $htmlfile="") {

		$this->tpl->loadTemplatefile("module.html");	
		
		if (""==$htmlfile)
			$htmlfile = substr($xmlfile, 7, -4).$this->file_extension;
			
		$this->accessor->loadXMLFile($this->path.$xmlfile);
		$module = $this->accessor->getModuledata();		
			
		$this->renderFunctions();
		$this->renderUses();
		$this->renderConstants();

		$tplvars = array();
		
		$tplvars["MODULE_FILE"] 		= $module["file"]["value"];
		$tplvars["MODULE_NAME"] 		= $module["name"];
		$tplvars["MODULE_GROUP"] = $module["group"];
		$tplvars["MODULE_ACCESS"]	= $module["access"];
		$tplvars["MODULE_PACKAGE"]	= $module["package"];
		$tplvars["MODULE_UNDOC"] 	= ("true" == $module["undoc"]) ? $this->undocumented : "";
		
		if (isset($module["doc"]["link"]))
			$this->renderLinks($module["doc"]["link"], "class_");
			
		if (isset($module["doc"]["author"]))
			$this->renderAuthors($module["doc"]["author"], "class_");
			
		if (isset($module["doc"]["see"]))
			$this->renderSee($module["doc"]["see"], "class_");
		
		$fields = array( 	"version", "deprecated", "copyright"	,
											"since", "magic");
		reset($fields);
		while (list($k, $field)=each($fields)) 
			if (isset($module["doc"][$field])) {
				$this->tpl->setCurrentBlock("module_".strtolower($field));
				$this->tpl->setVariable(strtoupper($field), $module["doc"][$field]["value"]);
				$this->tpl->parseCurrentBlock();
			}

		$fields = array( "description", "shortdescription" );
		reset($fields);
		while (list($k, $field)=each($fields)) 
			if (isset($module["doc"][$field]))
				$tplvars["MODULE_".strtoupper($field)] = $this->encode($module["doc"][$field]["value"]);

		$this->tpl->setCurrentBlock("__global__");
		$this->tpl->setVariable($tplvars);
		$this->tpl->setVariable("APPNAME", $this->application);
		
		$this->fileHandler->createFile($this->path.$htmlfile, $this->tpl->get() );
		$this->tpl->free();	
		
	} // end func renderModule

} // end class PhpdocHTMLModuleRenderer
?>
