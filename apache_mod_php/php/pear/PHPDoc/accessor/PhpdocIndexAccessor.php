<?php
/**
* Provides a API to access Index xml documents.
*/
class PhpdocIndexAccessor extends PhpdocAccessor {

	/**
	* Ordered list of all chapternames.
	* @var	array
	*/
	var $chapternames = array();
	
	/**
	* Ordered list of all chapters.
	* @var	array
	*/
	var $chapters = array();
	
	/**
	* List of all packages.
	* @var	array
	*/
	var $packages = array();
	
	/**
	* Data of a classtree
	* @var	array
	*/
	var $classtree = array();
	
	/**
	* Data of a modulegroup
	* @var	array
	*/
	var $modulegroup = array();
	
	/**
	* Some container withing the packagelist.
	* @var array
	* @see	buildPackagelist()
	*/
	var $packageFields = array("class", "module");
	
	/**
	* Flag indicating that certain internal datafield have been filled.
	* @var	array
	*/
	var $flagBuild = array(
												"chapter"		=> false,
												"package"		=> false
											);


	/**
	* Returns a modulegroup
	* 
	* @access	public
	*/
	function getModulegroup() {
		
		$this->buildModulegroup();
		
		if ($this->freeOnGet) {
			
			$data = $this->modulegroup;
			$this->modulegroup = array();
			return $data;
			
		} else {
		
			return $this->modulegroup;
		}
		
	} // end func getModulegroup
												
	/**
	* Returns a classtree.
	*
	* @return	array
	* @access	public
	*/		
	function getClasstree() {

		$this->buildClasstree();
		
		if ($this->freeOnGet) {
		
			$data = $this->classtree;
			$this->classtree = array();
			return $data;
			
		} else {
			
			return $this->classtree;
			
		}
		
	} // end func getClasstree
	
	/**
	* Returns an ordered list of all chapternames.
	* 
	* @return	array
	* @access	public
	* @see	getChapters()
	*/
	function getChapternames() {
	
		$this->buildChapterlist();
		
		if ($this->freeOnGet) {
		
			$data = $this->chapternames;
			$this->chapternames = array();
			return $data;
			
		} else {
		
			return $this->chapternames;
			
		}
		
	} // end func getChapternames
	
	/**
	* Returns an ordered list of all chapters.
	* 
	* @return array
	* @access	public
	* @see	getChapternames()
	*/
	function getChapters() {
	
		$this->buildChapterlist();
		
		if ($this->freeOnGet) {
			
			$data = $this->chapters;
			$this->chapters = array();
			return $data;
				
		} else {
		
			return $this->chapters;
			
		}
		
	} // end func getChapters
	
	/**
	* Returns a list of all packages
	*
	* @return	array
	* @access	public
	*/
	function getPackagelist() {
	
		$this->buildPackagelist();

		if ($this->freeOnGet) {
			
			$data = $this->packages;
			$this->packages = array();
			return $data;
			
		} else {
			
			return $this->packages;
			
		}
		
	} // end func getPackagelist
	
	
	/**
	* Builds the internal packagelist.
	*/
	function buildPackagelist() {
	
		if ($this->flagBuild["package"])
			return;
		
		$data = $this->xml["packagelist"];
		$this->xml = array();
		$this->flagBuild["package"] = true;
		
		$this->packages = array();
		
		if (!isset($data["package"][0]))
			$data["package"] = array($data["package"]);
			
		reset($data["package"]);
		while (list($k, $package)=each($data["package"])) {
			
			$packagename = $package["name"];
			
			reset($this->packageFields);
			while (list($k, $field)=each($this->packageFields)) {
				
				if (isset($package[$field][0])) {
					
					reset($package[$field]);
					while (list($k, $element)=each($package[$field]))
						$this->packages[$packagename][$field][] = $element["name"];
					 
				} else if (isset($package[$field])) {
					
					$this->packages[$packagename][$field][] = $package[$field]["name"];
					
				}
			}
			
		}
		
	} // end func buildPackagelist
	
	/**
	* Builds the internal chapterlists. 
	*/
	function buildChapterlist() {
	
		if ($this->flagBuild["chapter"])
			return;
			
		$data = $this->xml["index"];
		$this->xml = array();
		$this->flagBuild["chapter"] = true;

		$this->chapternames = array();
		$this->chapters = array();
		
		if (isset($data["chapter"][0])) {
			
			$chapterlist = array();
			reset($data["chapter"]);
			while (list($k, $chapter)=each($data["chapter"])) 
				$chapterlist[strtoupper($chapter["char"])][$chapter["char"]] = $k;
				
			ksort($chapterlist, SORT_STRING);
			
			reset($chapterlist);
			while (list($k, $chapters)=each($chapterlist)) {
			
				reset($chapters);
				while (list($chapter, $index)=each($chapters)) {
					$this->chapternames[] = $chapter;
					$this->chapters[$chapter] = $data["chapter"][$index];
				}
									
			}
			
		} else {
			
			$this->chapternames[] = $data["chapter"]["char"];
			$this->chapters[$data["chapter"]["char"]] = $data["chapter"]["char"];
			
		}
		
	} // end func buildChapterlist

	/**
	* Extracts the modulegroup data of the xml file.
	* 
	* @see	getModulegroup
	*/
	function buildModulegroup() {
		
		if ($this->flagBuild["modulegroup"])
			return;
			
		$this->flagBuild["modulegroup"] = true;
		$data = $this->xml["modulegroup"];
		
		$this->xml = "";
		$this->modulegroup = array(
																"group"		=> $data["name"],
																"modules"	=> array()
															);
		
		if (!isset($data["module"][0]))
			$data["module"] = array( $data["module"] );
		
		reset($data["module"]);
		while (list($k, $module)=each($data["module"]))
			$this->modulegroup["modules"][] = $module["name"];
			
	} // end func buildModulegroup
	
	/**
	* Extracts the classtree data of the xml file. 
	*
	* @see	getClasstree()
	*/	
	function buildClasstree() {
	
		if ($this->flagBuild["classtree"])
			return;
			
		$this->flagBuild["classtree"] = true;
		$data = $this->xml["classtree"];
		$this->xml = "";
		
		$this->classtree = array( 
															"baseclass"	=> $data["baseclass"], 
															"classes" 	=> array()
														);
														
		if (!isset($data["class"][0]))
			$data["class"] = array( $data["class"] );

		reset($data["class"]);
		while (list($k, $class)=each($data["class"])) {
			
			if (!isset($class["subclass"])) {
			
				$this->classtree["classes"][$class["name"]] = array();				
				
			} else {
				
				if (!isset($class["subclass"][0])) {
				
					$this->classtree["classes"][$class["name"]][] = $class["subclass"]["value"];
					
				} else {
				
					reset($class["subclass"]);
					while (list($k, $subclass)=each($class["subclass"]))
						$this->classtree["classes"][$class["name"]][] = $subclass["value"];
						
				}
				
			}
			
		}
		
	} // end func buildClasstree
	
	/**
	* Resets the build flags.
	* @see	$flagBuild
	*/											
	function init() {
		
		reset($this->flagBuild);
		while (list($k, $v)=each($this->flagBuild))
			$this->flagBuild[$k] = false;
			
	} // end func init

	
} // end class PhpdocIndexAccessor
?>