<?php
//
// +----------------------------------------------------------------------+
// | PHP version 4.0                                                      |
// +----------------------------------------------------------------------+
// | Copyright (c) 1997-2001 The PHP Group                                |
// +----------------------------------------------------------------------+
// | This source file is subject to version 2.02 of the PHP license,      |
// | that is bundled with this package in the file LICENSE, and is        |
// | available at through the world-wide-web at                           |
// | http://www.php.net/license/2_02.txt.                                 |
// | If you did not receive a copy of the PHP license and are unable to   |
// | obtain it through the world-wide-web, please send a note to          |
// | license@php.net so we can mail you a copy immediately.               |
// +----------------------------------------------------------------------+
// | Authors: Ulf Wendel <ulf.wendel@phpdoc.de>                           |
// +----------------------------------------------------------------------+
//
// $Id: Menu_Browser.php,v 1.1.1.1 2001/07/19 00:20:48 zarzycki Exp $

/**
* Simple filesystem browser that can be used to generated menu (3) hashes based on the directory structure.
* 
* Together with menu (3) and the (userland) cache you can use this 
* browser to generate simple fusebox like applications / content systems.
* 
* Let the menubrowser scan your document root and generate a menu (3) structure
* hash which maps the directory structure, pass it to menu's setMethod() and optionally
* wrap the cache around all this to save script runs. If you do so, it looks
* like this:
*
* // document root directory
* define("DOC_ROOT", "/home/server/www.example.com/");
*  
* // instantiate the menubrowser
* $browser = new menubrowser(DOC_ROOT);
*
* // instantiate menu (3)
* $menu = new menu($browser->getMenu());
*
* // output the sitemap
* $menu->show("sitemap");
* 
* Now, use e.g. simple XML files to store your content and additional menu informations
* (title!). Subclass exploreFile() depending on your file format. 
* 
* @author   Ulf Wendel <ulf.wendel@phpdoc.de>
* @version  $Id: Menu_Browser.php,v 1.1.1.1 2001/07/19 00:20:48 zarzycki Exp $
*/
class menubrowser {

    /**
    * Filesuffix of your XML files.
    * 
    * @var  string  
    * @see  menubrowser()
    */
    var $file_suffix = "xml";
    
    
    /**
    * Number of characters of the file suffix.
    *
    * @var  int
    * @see  menubrowser()
    */
    var $file_suffix_length = 3;
    
    
    /**
    * Filename (without suffix) of your index / start pages.
    * 
    * @var  string  
    * @see  menubrowser()
    */
    var $index = "index";
    
    
    /**
    * Full filename of your index / start pages.
    *
    * @var  string
    * @see  $file_suffix, $index
    */
    var $index_file = "";
    
    
    /**
    * Directory to scan.
    * 
    * @var  string
    * @see  setDirectory()
    */
    var $dir = "";
    
    
    /**
    * Prefix for every menu hash entry.
    * 
    * Set the ID prefix if you want to merge the browser menu 
    * hash with another (static) menu hash so that there're no 
    * name clashes with the ids.
    * 
    * @var  string
    * @see  setIDPrefix()
    */
    var $id_prefix = "";
    
    
    /**
    * Menu (3)'s setMenu() hash.
    * 
    * @var  array
    */
    var $menu = array();
    
    
    /**
    * Creates the object and optionally sets the directory to scan. 
    * 
    * @param    string
    * @see      $dir
    */
    function menubrowser($dir = "", $index = "", $file_suffix = "") {
        
        if ($dir)
            $this->dir = $dir;                
        if ($index)
            $this->index = $index;            
        if ($file_suffix)
            $this->file_suffix = $file_suffix;
            
        $this->index_file = $this->index . "." . $this->file_suffix;                                
        $this->file_suffix_length = strlen($this->file_suffix);
        
    } // end constructor
    
    
    /**
    * Sets the directory to scan. 
    *
    * @param    string  directory to scan
    * @access   public
    */
    function setDirectory($dir) {
    
        $this->dir = $dir;
        
    } // end func setDirectory
    
    
    /**
    * Sets the prefix for every id in the menu hash.
    * 
    * @param    string
    * @access   public
    */
    function setIDPrefix($prefix) {
    
        $this->id_prefix = $prefix;
        
    } // end func setIDPrefix
    
    
    /**
    * Returns a hash to be used with menu(3)'s setMenu(). 
    *
    * @param    string  directory to scan
    * @param    string  id prefix
    * @access   public
    */
    function getMenu($dir = "", $prefix = "") {
        
        if ($dir)
            $this->setDirectory($dir);
        if ($prefix)
            $this->setIDPrefix($prefix);

        // drop the result of previous runs    
        $this->files = array();          
        
        $this->menu = $this->browse($this->dir);
        $this->menu = $this->addFileInfo($this->menu);
        
        return $this->menu;
    } // end func getMenu

    
    /**
    * Recursive function that does the scan and builds the menu (3) hash. 
    *
    * @param    string  directory to scan
    * @param    integer entry id - used only for recursion
    * @param    boolean ??? - used only for recursion
    * @return   array
    */
    function browse($dir, $id = 0, $noindex = false) {

      $struct = array();
      $dh = opendir($dir);
      while ($file = readdir($dh)) {
        if ("." == $file || ".." == $file)
          continue;
          
        $ffile = $dir . $file;      
        if (is_dir($file)) {
        
          $ffile .= "/";
          
          if (file_exists($ffile . $this->index_file)) {
    
            $id++;
            $struct[$this->id_prefix . $id]["url"] = $ffile . $this->index_file;

            $sub = $this->browse($ffile, $id + 1, true);
            if (0 != count($sub))
              $struct[$this->id_prefix . $id]["sub"] = $sub;
              
          }
          
        } else {
    
          if ($this->file_suffix == substr($file, strlen($file) - $this->file_suffix_length, $this->file_suffix_length)
                && !($noindex && $this->index_file == $file) ) {
            $id++;                
            $struct[$this->id_prefix . $id]["url"] = $dir . $file;
          }
            
        }
          
      }
     
      return $struct;
    } // end func browse
    
    
    /**
    * Adds further informations to the menu hash gathered from the files in it
    *
    * @var      array   Menu hash to examine
    * @return   array   Modified menu hash with the new informations
    */
    function addFileInfo($menu) {

        // no foreach - it works on a copy - the recursive 
        // structure requires already lots of memory        
        reset($menu);
        while (list($id, $data) = each($menu)) {
        
            $menu[$id] = array_merge($data, $this->exploreFile($data["url"]));
            if (isset($data["sub"]))
                $menu[$id]["sub"] = $this->addFileInfo($data["sub"]);
                
        }

        return $menu;        
    } // end func addFileInfo
    
    
    /**
    * Returns additional menu informations decoded in the file that appears in the menu.
    * 
    * You should subclass this method to make it work with your own 
    * file formats. I used a simple XML format to store the content.
    * 
    * @param    string  filename
    */
    function exploreFile($file) {
    
        $xml = join("", @file($file));
        if (!$xml)
            return array();

        $doc = xmldoc($xml);
        $xpc = xpath_new_context($doc);
        
        $menu = xpath_eval($xpc, "//menu");
        $node = &$menu->nodeset[0];
        
        return array("title" => $node->content);
    } // end func exploreFile
    

} // end class menubrowser
?>