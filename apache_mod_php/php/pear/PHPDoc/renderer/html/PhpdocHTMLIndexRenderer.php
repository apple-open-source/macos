<?php
/**
* Renders Index lists.
*/
class PhpdocHTMLIndexRenderer extends PhpdocHTMLRenderer {

	/**
	* Some container in the package list.
	* @var	array
	* @see	renderPackagelist()
	*/
	var $packageFields = array("class", "module");
	
	/**
	* Packagelist from the PhpdocIndexAccessor
	* @var	array
	*/
	var $packages = array();
	
	/**
	* Array with classtree informations
	* @var	array
	*/
	var $classtree = array();
	
	/**
	* IntegratedTemplate Object used be renderClasstree()
	*
	* @var	object	IntegratedTemplate
	* @see	renderClasstree()
	*/
	var $treeTpl;
	
	/**
	* IntegratedTemplateObject used by renderModulegroup()
	* 
	* @var object	IntegratedTemplate
	* @see	renderModulegroup()
	*/
	var $moduleTpl;
	
	/**
	* Sets the xml and template root directory.
	* 
	* @param	string	XML file path
	* @param	string	Template file path
	* @param	string	Name of the application
	* @see	setPath(), setTemplateRoot()
	*/
	function PhpdocHTMLIndexRenderer($path, $templateRoot, $application, $extension = ".html") {
		
		$this->setPath($path);
		$this->setTemplateRoot($templateRoot);
		$this->application = $application;
        $this->file_extension = $extension;
		
		$this->accessor = new PhpdocIndexAccessor;
		$this->tpl = new IntegratedTemplate($this->templateRoot);
		$this->fileHandler = new PhpdocFileHandler;
		
	} // end constructor
	
	/**
	* Builds all index files phpdoc needs assuming that the xml files have default names
	* 
	* @access	public
	* @see	renderElementlist(), renderPackagelist(), renderFramelementlist(), renderFramePackageSummary()
	*/
	function generate() {

		$this->renderElementlist("elementlist.xml");
		$this->renderFrameElementlist("elementlist.xml");
		
		$this->renderPackagelist("packagelist.xml");
		$this->renderFramePackageSummary("packagelist.xml");
		$this->renderFrameElementlist("packagelist.xml");

	} // end function generate

	/**
	* Saves the generated classtree summary to disk.
	* 
	* @see		renderClasstree()
	* @access	public
	*/
	function finishClasstree() {
		
		if (!is_object($this->treeTpl)) 
			return;

		$this->treeTpl->setVariable("APPNAME", $this->application);
		$this->fileHandler->createFile($this->path."phpdoc_classtree".$this->file_extension, $this->treeTpl->get() );
		$this->treeTpl = "";
		
	}	// end func finishClasstree
	
	/**
	* Adds a classtree to the classtree summary template.
	* 
	* @param	string	XML Classtree file
	* @see		finishClasstree()
	* @access	public
	*/
	function addClasstree($xmlfile) {

		$this->accessor->loadXMLFile($this->path.$xmlfile);

		if (!is_object($this->treeTpl)) {
		
			$this->treeTpl = new IntegratedTemplate($this->templateRoot);
			$this->treeTpl->loadTemplatefile("classtree.html");
			
		}
		
		$this->classtree = $this->accessor->getClasstree();
		
		$this->treeTpl->setCurrentBlock("classtree");
		$this->treeTpl->setVariable("BASECLASS", $this->classtree["baseclass"]);
		$this->treeTpl->setVariable("TREE", "<ul>".$this->buildClasstreeHTML($this->classtree["baseclass"])."</ul>");
		$this->treeTpl->parseCurrentBlock();
		
		return true;
	} // end func addClasstree

	function finishModulegroup() {
		
		if (!is_object($this->moduleTpl)) 
			return;
			
		$this->moduleTpl->setVariable("APPNAME", $this->application);
		$this->fileHandler->createFile($this->path."phpdoc_modulegroup".$this->file_extension, $this->moduleTpl->get() );
		$this->moduleTpl = "";
		
	} // end func finishModulegroups
	/**
	* Renders a modulegroup xml file.
	* @param	string	XML File
	*/	
	function addModulegroup($xmlfile) {
	
		$this->accessor->loadXMLFile($this->path.$xmlfile);
		
		if (!is_object($this->moduleTpl)) {
		
			$this->moduleTpl 	= new IntegratedTemplate($this->templateRoot);
			$this->moduleTpl->loadTemplateFile("modulegroup.html");
			
		}
		
		$modulegroup = $this->accessor->getModulegroup();

		$modules = "<ul>";
		reset($modulegroup["modules"]);
		while (list($k, $module)=each($modulegroup["modules"])) 
			$modules.= sprintf('<li><a href="%s">%s</a>', $this->nameToUrl($module).$this->file_extension, $module);
		$modules.= "</ul>";
		
		$this->moduleTpl->setCurrentBlock("modulegroup");
		$this->moduleTpl->setVariable("MODULEGROUP", $modulegroup["group"]);
		$this->moduleTpl->setVariable("MODULES", $modules);
		$this->moduleTpl->parseCurrentBlock();		
		
	} // end func addModulegroup
	
	/**
	* Renders the element index list.
	*
	* @param	string	XML file
	* @access	public
	* @see	generate()
	*/ 
	function renderElementlist($xmlfile) {
	
		$this->accessor->loadXMLFile($this->path.$xmlfile);
		$this->tpl->loadTemplatefile("elementlist.html");
		
		$chapters = $this->accessor->getChapternames();
		
		if (0!=count($chapters)) {
		
			$this->tpl->setCurrentBlock("chaptersummary_loop");
			reset($chapters);
			while (list($k, $chapter)=each($chapters)) {
				$this->tpl->setVariable("CHAPTER", $chapter);
				$this->tpl->parseCurrentBlock();
			}
			
			$chapters = $this->accessor->getChapters();
			
			reset($chapters);
			while (list($name, $elements)=each($chapters)) {
			
				if (!isset($elements["element"][0])) 
					$elements["element"] = array($elements["element"]);
				
				$this->tpl->setCurrentBlock("chapter_loop");
				
				reset($elements["element"]);
				while (list($k, $element)=each($elements["element"])) {
					
					switch($element["type"]) {
						case "package":
							$desc = "Package";
							break;
							
						case "class":
							$desc = sprintf('Class <a href="%s">%s</a>.', 
																$this->nameToUrl($element["name"]).$this->file_extension,
																$element["name"]
															);
							break;
							
						case "module":
							$desc = sprintf('Module <a href="%s">%s</a>.',
																$this->nameToUrl($element["name"]).$this->file_extension,
																$element["name"]
															);
							break;
							
						case "functions":
							$desc = sprintf('Function in %s <a href="%s">%s</a>',
																$element["sourcetype"],
																$this->nameToUrl($element["source"]).$this->file_extension,
																$element["source"]
															);
							break;
							
						case "variables":
							$desc = sprintf('Variable in Class <a href="%s">%s</a>',
																$this->nameToUrl($element["source"]).$this->file_extension,
																$element["source"]
															);
							break;
							
						case "uses":
							$desc = sprintf('Included file in %s <a href="%s">%s</a>',
																$element["sourcetype"],
																$this->nameToUrl($element["source"]).$this->file_extension,
																$element["source"]
															);
							break;
							break;
						
						case "consts":
							$desc = sprintf('Constant defined in %s <a href="%s">%s</a>',
																$element["sourcetype"],
																$this->nameToUrl($element["source"]).$this->file_extension,
																$element["source"]
															);
							break;
					}

					$this->tpl->setVariable("ELEMENTNAME", $element["name"]);
					$this->tpl->setVariable("ELEMENT", $desc);
					$this->tpl->setVariable("SHORTDESCRIPTION", $element["value"]);
					$this->tpl->parseCurrentBlock();
					
				}
					
				$this->tpl->setCurrentBlock("chapter");
				$this->tpl->setVariable("CHAPTER", $name);
				$this->tpl->parseCurrentBlock();
			}
				
				
		}

		$this->tpl->setVariable("APPNAME", $this->application);
		$this->fileHandler->createFile($this->path."phpdoc_elementlist".$this->file_extension, $this->tpl->get() );
		$this->tpl->free();
		
	} // end func renderElementlist
	
	/**
	* Renders a complete packagelist.
	*
	* @param	string	XML file
	* @access	public
	* @see	renderFrameElementlist(), renderFramePackagesummary()
	*/
	function renderPackagelist($xmlfile) {
		
		$this->loadPackagelist($xmlfile);
		$this->tpl->loadTemplatefile("packagelist.html");
		
		reset($this->packages);
		while (list($packagename, $package)=each($this->packages)) {
			
			reset($this->packageFields);
			while (list($k, $field)=each($this->packageFields)) {
				if (!isset($package[$field]))
					continue;
				
				$this->tpl->setCurrentBlock("package_".$field."_loop");	
				reset($package[$field]);
				while (list($k, $element)=each($package[$field])) {
					$this->tpl->setVariable("ELEMENT", sprintf('<a href="%s">%s</a>', 
																												$this->nameToUrl($element).$this->file_extension, 
																												$element
																											)
																							);
					$this->tpl->parseCurrentBlock();
				}
				
				$this->tpl->setCurrentBlock("package_".$field);
				$this->tpl->setVariable("EMPTY", "");
				$this->tpl->parseCurrentBlock();
				
			}
			
			$this->tpl->setCurrentBlock("package");
			$this->tpl->setVariable("PACKAGE_NAME", $packagename);
			$this->tpl->parseCurrentBlock();
					
		}
		
		$this->tpl->setVariable("APPNAME", $this->application);
		$this->fileHandler->createFile($this->path."phpdoc_packagelist".$this->file_extension, $this->tpl->get() );
		$this->tpl->free();
		
	} // end func renderPackagelist
	
	/**
	* Renders files for the lower left frame with the elements of a certain file.
	*
	* @param	string	This function needs the packagelist.xml to work!
	* @access	public
	* @see	renderFramePackagesummary(), renderPackagelist()
	*/
	function renderFrameElementlist($xmlfile) {
	
		$this->loadPackagelist($xmlfile);

		reset($this->packages);
		while (list($packagename, $package)=each($this->packages)) {

			$this->tpl->loadTemplatefile("frame_packageelementlist.html");
						
			reset($this->packageFields);
			while (list($k, $field)=each($this->packageFields)) {
				if (!isset($package[$field]))
					continue;
				
				$this->tpl->setCurrentBlock("package_".$field."_loop");	
				reset($package[$field]);
				while (list($k, $element)=each($package[$field])) {
				
					$this->tpl->setVariable("ELEMENT", sprintf('<a href="%s" target="main">%s</a>', 
																												$this->nameToUrl($element).$this->file_extension, 
																												$element
																											) 
																								);
					$this->tpl->parseCurrentBlock();
				}
				
				$this->tpl->setCurrentBlock("package_".$field);
				$this->tpl->setVariable("EMPTY", "");
				$this->tpl->parseCurrentBlock();
				
			}
			
			$this->tpl->setCurrentBlock("package");
			$this->tpl->setVariable("PACKAGE_NAME", $packagename);
			$this->tpl->parseCurrentBlock();
			
			$this->tpl->setVariable("APPNAME", $this->application);
			$packagename = $this->nameToUrl($packagename);
			$this->fileHandler->createFile($this->path."packageelementlist_".$packagename.$this->file_extension, $this->tpl->get() );					
		}
		
		$this->tpl->free();
				
	} // end func renderFrameElementlist
	
	/**
	* Renders a Packagesummary for the frameset.
	* 
	* @param	string	XML file.
	* @access	public
	* @see	renderPackagelist(), renderFrameElementlist()
	*/
	function renderFramePackagesummary($xmlfile) {

		$this->loadPackagelist($xmlfile);
		$this->tpl->loadTemplatefile("frame_packagelist.html");
		$this->tpl->setCurrentBlock("package");
		
		reset($this->packages);
		while (list($packagename, $v)=each($this->packages)) {
			$this->tpl->setVariable("PACKAGE", sprintf('<a href="packageelementlist_%s" target="packageelements">%s</a>',
																										$this->nameToUrl($packagename).$this->file_extension,
																										$packagename )
															);
			$this->tpl->parseCurrentBlock();															
		}
		
		$this->tpl->setVariable("APPNAME", $this->application);
		$this->fileHandler->createFile($this->path."frame_packagelist".$this->file_extension, $this->tpl->get() );
		$this->tpl->free();
		
	} // end func renderFramePackagesummary

	/**
	* Imports the packagelist from the PhpdocIndexAccessor if not done previously.
	* 
	* @param	string 	XMl file.
	* @see	$packages
	*/	
	function loadPackagelist($xmlfile) {
		
		if (0==count($this->packages)) {
			$this->accessor->loadXMLFile($this->path.$xmlfile);
			$this->packages = $this->accessor->getPackagelist();		
		}

	} // end func loadPackagelist
	
	/**
	* Recursivly builds an HTML class tree using <ul><li></ul>.
	*
	* @param	string	Name of the class the recursive loop starts with
	* @see	renderClasstree()
	*/
	function buildClasstreeHTML($class) {
		
		$html = "";
		
		if (0==count($this->classtree["classes"][$class])) {
		
			$html.= sprintf('<li><a href="%s">%s</a>', $this->nameToUrl($class).$this->file_extension, $class);
			
		} else {
		
			$html.= sprintf('<li><a href="%s">%s</a>', $this->nameToUrl($class).$this->file_extension, $class);

			$html.= "<ul>";
			reset($this->classtree["classes"][$class]);
			while (list($k, $subclass)=each($this->classtree["classes"][$class])) 
				$html.= $this->buildClasstreeHTML($subclass);					
			$html.= "</ul>";
		}
		
		return $html;
	} // end func buildClasstreeHTML

} // end class PhpdocHTMLIndexRenderer
?>
