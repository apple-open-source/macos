<?php
/**
* "Main" class of the Parser collection.
* 
* Note that a lot of communication is done by using shared instance variables.
*/
class PhpdocParser extends PhpdocClassParser {

	/**
	* Name of the file currently parsed. 
	* 
	* Instead of passing the name of the current file by argument
	* PHPDoc uses this slot to communicate. Yeah I know, it's
	* the way methods should communicate, but it saves me a lot 
	* a lot of work.
	* @var	string	Name of the file currently parsed.
	*/
	var $currentFile = "";
	
	/**
	* Array of PHP Sourcecode Files to examine.
	* The array keys hold the filenames, the array values the file content.
	* @var	array		
	* @see	parse()
	*/													
	var	$phpfiles = array();													
	
	/**
	* Mapping from classnames to filenames
	* @var	array
	*/
	var $classnamesToFilenames = array();
	
	/**
	* Hash with the data of the current class tree (one parentclass with all children).
	* @var	array
	* @see	modules
	*/
	var $classes = array();
	
	/**
	* List of all parentclasses found.
	* @var	array
	*/
	var $baseclasses = array();

	/**
	* List of all files containing classes.
	* @var array
	*/		
	var $classfiles = array();
	
	/**
	* Hash of all class trees. 
	* @var array
	*/
	var $classtree = array();

	/**
	* List of all files containing modules.
	* @var	array
	*/	
	var $modulefiles = array();
	
	/**
	* List of all module groups.
	* @var array
	*/
	var $modulegroups = array();
	
	/**
	* Hash with the data of the current module group.
	* @var	array
	* @see	classes
	*/
	var $modules = array();

	/**
	* Hash of all packages found.
	* @var	array
	*/	
	var $packages = array();

	/**
	* Flag indicating that getClassTree() was called.
	* @var	boolean	
	* @see	getClassTree()
	*/
	var $flag_classtree = false;
	
	/**
	* Flag indicating that getModulegroup was called.
	* @var	boolean
	* @see	getModulegroup
	*/
	var $flag_modulegroup = false;	
	
	/**
	* Name of the base class of the current class tree
	* @var	string
	* @see	getClassTree()
	*/
	var $current_baseclass = "";
	
	/**
	* Creates an instance of PhpdocWarning and calls buildComplexRegExps() to initialize the object.
	* @param	boolean  If true the parser prints status messages.
	* @see	$warn, buildComplexRegExps()
	*/
	function PhpdocParser($flag_output = false) {
	
		if ($flag_output)
			$this->setFlagOutput(true);
		else 
			$this->setFlagOutput(false);
			
		$this->buildComplexRegExps();
		
	} // end constructor
	
	/**
	* Central parsing function.
	*
	* With version 0.3alpha PHPdoc changed the way the parser works. It does now
	* 1 1/2 parsing runs. One prescan to build the class trees and a list of module
	* groups and one deep scan to extract the information. This reduces the memory 
	* consumption.
	* 
	* @return	boolean	$ok
	* @access	public	
	* @see	findModulegroups(), findClassTrees(), getModulesAndClasses()
	*/
	function preparse() {

		if (0==count($this->phpfiles)) {
			$this->err[] = new PHPDocError("Can't parse - no files defined.", __FILE__, __LINE__);
			return false;
		}
		
		$para = array();
		reset($this->phpfiles);
		while (list($filename, $phpcode)=each($this->phpfiles))
			$para[$filename] = $this->getModulesAndClasses($phpcode);
			
		$this->findModulegroups($para);
		$this->findClassTrees($para);		
		
		return true;		
	} // end func preparse
	
	
	/**
	* Returns the data of one parentclass and all it's subclasses or false.
	*
	* Use this function to loop through the class trees. The loop should look somewhat like: 
	* <code>while ( $classtree = $parser->getClassTree() ) ...</code>
	*  
	* @return mixed		$classes	Hash with the data of the current class tree or false.
	* @access	public
	* @see		getModulegroup(), %baseclasses
	*/
	function getClassTree() {
	
		// first call, reset the baseclass array pointer
		if (!$this->flag_classtree) {
			reset($this->baseclasses);
			$this->flag_classtree = true;
		}
		
		if (list($classname, $filename)=each($this->baseclasses)) {
		
			$this->classes = array();
			$this->current_baseclass = $classname;
			
			$this->addClass($classname, $filename);

			return $this->classes;
			
		} else {
		
			return false;
			
		}
		
	} // end func getClassTree
	
	/**
	* Returns the data of one module group.
	* 
	* Use this function to loop through the module groups. The loop should look somewhat like:
	* <code>while ( $modulegroup = $parser->getModulegroup() ) ...</code>.
	*
	* @return	mixed		$modulegroup	Hash with the data of the current class tree or false.
	* @access	public
	* @see		getClassTree(), addModule(), $modulegroups
	*/
	function getModulegroup() {
		
		if (!$this->flag_modulegroup) {
			reset($this->modulegroups);
			$this->flag_modulegroup = true;
		}
		
		if (list($group, $modules)=each($this->modulegroups)) {
			
			$this->modules = array();
			while (list($modulename, $files)=each($modules)) {
				reset($files);
				while (list($k, $filename)=each($files))
					$this->addModule($group, $filename);		
			}
			
			return $this->modules;
			
		} else {
		
			return false;
		
		}
		
	} // end func getModulegroup
	
	/**
	*	Analyses the given file and adds the result to the module list.
	* 
	* The function analyses the given file, unsets the file in the 
	* file list, adds the result of the parser to the module list and 
	* if necessary it adds some data to the package list.
	*
	* @param	string	Name of the module group the parsing result gets added.
	* @param	string	Name of the file to scan.
	* @see	getPhpdocParagraphs(), analyseModule()
	*/	
	function addModule($group, $filename) {

		$data = $this->getPhpdocParagraphs($this->phpfiles[$filename], array("classes", "variables") );	
		// free memory as soon as possible...
		unset($this->phpfiles[$filename]);
		
		// note: not passed by argument
		$this->currentFile = $filename;
		$result = $this->analyseModule($data);
		$result["filename"] = $filename;
		
		$this->modules[$group][$result["name"]] = $result;
					
		if (isset($result["package"]))
			$this->packages[$result["package"]]["modules"][] = array (
																																"name"			=> $result["name"],
																																"group"			=> $result["group"],
																																"filename"	=> $filename
																															);
		
	} // end func addModule
	
	/**
	* Analyses the given file and adds the result to the class list.
	* 
	* The first parameter (classname) comes from the prescan done 
	* by findClassTrees()
	*
	* @param	string	Name of the class that gets added.
	* @param	string	Name of the file to scan.
	* @see	addSubclasses(), analyseClass(), $classes
	*/
	function addClass($classname, $filename) {
		
		$data = $this->getPhpdocParagraphs($this->phpfiles[$filename], array("modules") );
		// free memory as soon as possible...
		unset($this->phpfiles[$filename]);
		
		$this->currentFile = $filename;
		$result = $this->analyseClass($data);

		// Add some informations from the classtree that was build by the prescan to the class.
		$fields = array("subclasses", "noparent", "path", "baseclass");		
		reset($fields);
		while (list($k, $field)=each($fields))
			if (isset($this->classtree[$filename][$classname][$field]))
				$result[$field] = $this->classtree[$filename][$classname][$field];

		$result["filename"] = $filename;		
		
		$this->classes[$classname] = $result;		
		$this->addSubclasses($classname);
		
		if (isset($result["package"]))
			$this->packages[$result["package"]]["classes"][] = $classname;
				
	} // end func addClass
	
	/**
	* Adds recursively subclasses to the specified class.
	* @param	string Name of the class that might contain subclasses
	* @see	addClass()
	*/
	function addSubclasses($classname) {
		
		if (isset($this->classes[$classname]["subclasses"])) {
			
			$subclasses = $this->classes[$classname]["subclasses"];
			while (list($subclass, $v)=each($subclasses)) 
				$this->addClass($subclass, $this->classnamesToFilenames[$subclass]);
				
		}
		
	} // end func addSubclasses
	
	/**
	* Builds the hash of module groups and the module file list.
	*
	* @param	array	Hash with the result of getClassesAndModules() of all files
	* @see	parse(), findClassTree(), $modulegroups, $modulefiles
	*/
	function findModulegroups($para) {
		
		reset($para);
		while (list($filename, $data)=each($para)) {

			if (isset($data["modules"]["name"])) {
			
				$name = (""!=$data["modules"]["name"]) ? $data["modules"]["name"] : $filename;
				$group = (""!=$data["modules"]["group"]) ? $data["modules"]["group"] : $name;			
				
				if (0!=count($data["classes"])) {
					// As we do not have a real parser that returns a parsing tree we can't 
					// handle modules and classes in one file. Drop a note to the user.
					$this->warn->addDocWarning(	$filename, "module", $name, "PHPDoc is confused: module files must not contain classes. Doc will probably be broken, module gets ignored.", "collision" );
					continue;
				}

				if (isset($this->modulegroups[$group][$name])) 
					$this->warn->addDocWarning($filename, "module", $name, "Warning: there's more than one module '$name' (file: '$filename) in the module group '$group'.", "warning");

				$this->modulegroups[$group][$name][] = $filename;					
				$this->modulefiles[] = $filename;				
								
			}
			
		}

	} // end func findModulegroups
	
	/**
	* Builds a hash of all class trees.
	*
	* @param array	Hash with the result of getClassesAndModules() of all files
	* @see	parse(), findModulegroups(), $classnamesToFilenames, $classtree, $classfiles, $baseclasses
	*/
	function findClassTrees($para) {
		
		reset($para);
		while(list($filename, $data)=each($para)) {
		
			if (0!=count($data["classes"])) {

				$classname = $data["classes"][0]["name"];
															
				if (1<count($data["classes"]))
					$this->warn->addDocWarning($filename, "class", $classname , "PHPDoc is confused: there is more than one class in this file. Doc will probably be broken, first class '$classname' gets used, file '$filename' get ignored.", "collision");
					
				if (isset($data["modules"]["name"]))
					$this->warn->addDocWarning($filename, "class", "", "Warning: found a module comment in a class file. Module comment gets ignored, doc might be broken.", "collision");
				
				$this->classnamesToFilenames[$classname] = $filename;
				$this->classtree[$filename][$classname] = $data["classes"][0];
				$this->classfiles[] = $filename;
																
			}
			
		}
		
		reset($this->classnamesToFilenames);
		while (list($classname, $filename)=each($this->classnamesToFilenames)) {

			$path 			= array();
			$baseclass	= $classname;
			$basefile 	= $filename;
			$flag_noparent	= false;
			
			while ($extends = $this->classtree[$basefile][$baseclass]["extends"]) {
				if (!isset($this->classnamesToFilenames[$extends])) {
					$flag_noparent = true;
					break;
				}

				$this->classtree[$this->classnamesToFilenames[$extends]][$extends]["subclasses"][$baseclass] = true;
				$path[] 		= $extends;
				$baseclass 	= $extends;
				$basefile 	= $this->classnamesToFilenames[$baseclass];
			}
			
			if ($flag_noparent)
				$this->classtree[$filename][$classname]["noparent"] = $flag_noparent;
			
			$base = (0==count($path)) ? true : false;
			if ($base) 
				$this->baseclasses[$classname] = $filename;
			else 
				$this->classtree[$filename][$classname]["path"] = $path;
				
			if ($baseclass != $classname)
				$this->classtree[$filename][$classname]["baseclass"] = $baseclass;
		}
		
	} // end func findClassTrees

	/**
	* Returns the mapping array from classnames to filenames
	* @return array	$this->classnamesToFilenames
	* @see	classnamesToFilenames
	*/
	function getClassnamesToFilenames() {
		return $this->classnamesToFilenames;
	} // end func getClassnamesToFilenames

	
	/**
	* Sets the list of PHP Soucecode Files to examine.
	* @param	array		$phpfiles
	* @return	bool		$ok
	* @access	public
	*/
	function setPhpSourcecodeFiles($phpfiles) {
		if (!is_array($phpfiles) || 0==count($phpfiles)) 
			return false;
		
		$this->phpfiles = $phpfiles;
		return true;	
	} // end func setPhpSourcecodeFiles
	
} // end class PhpdocParser
?>