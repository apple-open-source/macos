<?php
/**
* File handling functions in phpdoc. 
*
* @version	0.1alpha
* @author		Ulf Wendel <ulf@redsys.de>
*/
class PhpdocFileHandler extends PhpdocObject {
	
	/**
	* Filepath. The path is automatically added in front of all filenames
	* @var		string	$path
	* @see		setFilePath()
	* @access	private
	*/
	var $path = "";
		
	/**
	* Reads a list of files or one file.
	* @param	string	$files	filename
	* @param	array		$files	list of filenames, $k => $filename
	* @return	array		$contents file contents indexed by filename
	* @throws PhpdocError
	* @access	public
	*/		
	function Get($files) {
		if (""==$files) {
			$this->err[] = new PhpdocError("No files specified.", __FILE__, __LINE__);
			return array("", "");
		}
		
		if (!is_array($files))
			$files = array($files);
		
		$contents = array();	
		$together = "";
		
		reset($files);
		while (list($k, $filename)=each($files)) 
			$contents[$filename] = $this->getFile($filename);
		
		return $contents;
	} // end func Get
	
		/**
	* Sets the filepath. The path is automatically added in front of all filenames
	* @param	string	$path
	* @return	bool	$ok
	* @access	public
	*/
	function setFilePath($path) {
		$this->path = $path;
	} // end func setFilePath
	
	/** 
	* Reads a file. 
	* @param	string	$filename
	* @return	string	$content
	* @throws	PhpdocError
	* @access	private
	*/
	function getFile($filename) {
		if (""==$filename) {
			$this->err[] = new PhpdocError("getFile(), no filename specified.", __FILE__, __LINE__);				
			return "";
		}
		if (!file_exists($filename)) {
			$this->err[] = new PhpdocError("getFile(), unknown file '$filename'.", __FILE__, __LINE__);
			return "";
		}
		if (!$fh = @fopen($filename, "r")) {
			$this->err[] = new PhpdocError("getFile(), can't open file '$filename' for reading.", __FILE__, __LINE__);
			return "";
		}

		$content = fread($fh, filesize($filename));
		fclose($fh);
		return $content;			
	} // end func getFile
	
	function appendToFile($filename, $content, $directory="") {
		if (""==$filename || ""==$content) {
			$this->err[] = new PhpdocError("No filename and/or no content given.", __FILE__, __LINE__);
			return false;
		}
		
		$fh = @fopen($filename, "a");
		if (!$fh) {
			print $filename;
			### PANIK
			return false;
		}
		
		fwrite($fh, $content);
		fclose($fh);
		
		return true;
	} // end func appendToFile
	
	/**
	* Creates a new file.
	* 
	* Create or overrides a file in a specified directory. If the
	* directory does not exists, it attempts to create it.
	*/ 
	function createFile($filename, $content, $directory="") {
		if (""==$filename || ""==$content) {
			$this->err[] = new PhpdocError("No filename or no content given.", __FILE__, __LINE__);
			return false;
		}
		
		$fh = @fopen($filename, "w");
		if (!$fh) {
			$this->err[] = new PhpdocError("Can't create file '$filename'.", __FILE__, __LINE__);
			return false;
		}
		
		fwrite($fh, $content);
		fclose($fh);
		
		return true;
	} // end func createFile
	
	/**
	* Returns a list of files in a specified directory
	* @param	string	$directory
	* @param	mixed		$suffix				Suffix of the files returned 
	* @param	boolean	$flag_subdir	include subdirectories? 
	* @param	array		$files				New entries are added to this variable if provided.
	*																Used only for the subdir feature.
	* @return	array		$files
	* @throws	PhpdocError
	*/
	function getFilesInDirectory($directory, $suffix="", $flag_subdir = true, $files = "") {
		if (""==$directory) {
			$this->err[] = new PhpdocError("No directory specified", __FILE__, __LINE__);
			return array();
		}
		
		if ("/"!=substr($directory, -1))
			$directory.= "/";

		if (""==$suffix) 
			$flag_all = true;
		else {
		
			$flag_all = false;
			$allowed 	= array();
			
			if (!is_array($suffix))
				$suffix = array($suffix);
			
			reset($suffix);
			while (list($k, $v)=each($suffix))
				$allowed[".".$v] = true;
				
		}

		if (!is_array($files)) 
			$files = array();
		
		$dh = @opendir($directory);
		if (!$dh) {
			$this->err[] = new PhpdocError("Can't open '$directory' for reading.", __FILE__, __LINE__);
			return array();
		}
		while ($file = readdir($dh)) {
			if ("." == $file || ".." == $file)
				continue;
			
			if ($flag_subdir && is_dir($directory.$file))
				$files = $this->getFilesInDirectory($directory.$file, $suffix, true, $files);
				
			if (!is_file($directory.$file))
				continue;
			
			if ($flag_all) {
				$files[] = $file;
			} else {			
				if (isset($allowed[substr($file, strrpos($file, "."))]))
					$files[] = $directory.$file;
			}
			
		}
		closedir($dh);
		
		return $files;
	} // end fun getFilesInDirectory
		
} // end class PhpdocFileHandler
?>