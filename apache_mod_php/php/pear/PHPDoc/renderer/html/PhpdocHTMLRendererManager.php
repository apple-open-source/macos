<?php
/**
* Controls the HTML Renderer objects.
* 
*/ 
class PhpdocHTMLRendererManager extends PhpdocObject {
	/**
	* @var	object PhpdocHTMLIndexRenderer
	*/
	var $indexrenderer;
	
	/**
	* @var	object PhpdocHTMLClassRenderer
	*/
	var $classrenderer;
	
	/**
	* @var	object PhpdocHTMLModuleRenderer
	*/
	var $modulerenderer;
	
	/**
	* @var	object PhpdocHTMLWarningRenderer
	*/
	var $warningrenderer;
    
	/**
	* Creates all necessary renderer objects
	* 
	* @param	string	Name of the target directory
	* @param 	string	Name of the directory with the templates.
	* @param	string	Name of the current application
	* @param	string	Extension of generated files
	*/	
	function PhpdocHTMLRendererManager($target, $template, $application, $extension = ".html") {
	
		$this->indexrenderer = new PhpdocHTMLIndexRenderer($target, $template, $application, $extension);
		$this->indexrenderer->generate();

		$this->classrenderer 	= new PhpdocHTMLClassRenderer($target, $template, $application, $extension);
		$this->modulerenderer = new PhpdocHTMLModuleRenderer($target, $template, $application, $extension);
		$this->warningrenderer = new PhpdocHTMLWarningRenderer($target, $template, $application, $extension);
		
	} // end constructor
	
	/**
	* Renders the given xml file.
	* 
	* @param	string	XML file.
	* @param	string	Content of the XML file: class, classtree, 
	*									module, modulegroup, warnings, indexdata
	* @access	public
	*/
	function render($xmlfile, $type) {
			
		switch(strtolower($type)) {
		
			case "class":
				$this->classrenderer->renderClass($xmlfile);
				break;
			
			case "classtree":
				$this->indexrenderer->addClasstree($xmlfile);
				break;
				
			case "module":
				$this->modulerenderer->renderModule($xmlfile);
				break;	
			
			case "modulegroup":
				$this->indexrenderer->addModulegroup($xmlfile);
				break;
				
			case "warning":
				$this->warningrenderer->addWarnings($xmlfile);
				break;
				
		}
		
	} // end func render

	/**
	* Finishes the rendering process.
	* 
	* Finish means here: write the classtree and modulegroup overview to disk.
	*
	* @access	public
	*/	
	function finish() {

		$this->indexrenderer->finishClasstree();
		$this->indexrenderer->finishModulegroup();
		$this->warningrenderer->finishWarnings();

	} // end func finish
	
} // end class PhpdocHTMLRendererManager
?>