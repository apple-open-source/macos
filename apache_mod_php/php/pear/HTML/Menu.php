<?php
//
// +----------------------------------------------------------------------+
// | PHP version 4.0                                                      |
// +----------------------------------------------------------------------+
// | Copyright (c) 1997-2001 The PHP Group                                |
// +----------------------------------------------------------------------+
// | This source file is subject to version 2.0 of the PHP license,       |
// | that is bundled with this package in the file LICENSE, and is        |
// | available at through the world-wide-web at                           |
// | http://www.php.net/license/2_02.txt.                                 |
// | If you did not receive a copy of the PHP license and are unable to   |
// | obtain it through the world-wide-web, please send a note to          |
// | license@php.net so we can mail you a copy immediately.               |
// +----------------------------------------------------------------------+
// | Authors: Ulf Wendel <ulf.wendel@phpdoc.de                            |
// +----------------------------------------------------------------------+
//
// $Id: Menu.php,v 1.1.1.1 2001/07/19 00:20:49 zarzycki Exp $
// 

/**
* Generates a HTML menu from a multidimensional hash.
* 
* Special thanks to the original author: Alex Vorobiev  <sasha@mathforum.com>. 
*
* @version  $Id: Menu.php,v 1.1.1.1 2001/07/19 00:20:49 zarzycki Exp $
* @author   Ulf Wendel <ulf.wendel@phpdoc.de>
* @access   public
* @package  HTML
*/
class menu {


    /**
    * Menu structure as a multidimensional hash.
    * 
    * @var  array
    * @see  setMenu(), Menu()
    */    
    var $menu = array();
    
    
    /**
    * Mapping from URL to menu path.
    *
    * @var  array
    * @see  getPath()
    */
    var $urlmap = array();
    
    
    /**
    * Menu type: tree, rows, you-are-here.
    * 
    * @var  array
    * @see  setMenuType()
    */
    var $menu_type = "tree";
    
    
    /**
    * Path to a certain menu item.
    * 
    * Internal class variable introduced to save some recursion overhead. 
    *
    * @var  array
    * @see  get(), getPath()
    */
    var $path = array();
    
    
    /**
    * Generated HTML menu.
    * 
    * @var  string
    * @see  get()
    */
    var $html = "";
    
    
    /**
    * URL of the current page.
    *
    * This can be the URL of the current page but it must not be exactly the 
    * return value of getCurrentURL(). If there's no entry for the return value
    * in the menu hash getPath() tries to find the menu item that fits best
    * by shortening the URL sign by sign until it finds an entry that fits.
    * 
    * @see  getCurrentURL(), getPath()
    */
    var $current_url = "";

    
    /**
    * Initializes the menu, sets the type and menu structure.
    * 
    * @param    array
    * @param    string
    * @see      setMenuType(), setMenu()
    */
    function menu($menu = "", $type = "tree") { 
        
        if (is_array($menu))
            $this->setMenu($menu);

        $this->setMenuType($type);
        
    } // end constructor   
    
    
    /**
    * Sets the menu structure.
    *
    * The menu structure is defined by a multidimensional hash. This is 
    * quite "dirty" but simple and easy to traverse. An example 
    * show the structure. To get the following menu:
    * 
    * 1  - Projects
    * 11 - Projects => PHPDoc
    * 12 - Projects => Forms
    * 2  - Stuff
    *
    * you need the array:
    * 
    * $menu = array( 
    *           1 => array( 
    *                  "title" => "Projects", 
    *                  "url" => "/projects/index.php",
    *                  "sub" => array(
    *                           11 => array(
    *                                       "title" => "PHPDoc",
    *                                       ...
    *                                     ),
    *                           12 => array( ... ),
    *                 )
    *             ),
    *           2 => array( "title" => "Stuff", "url" => "/stuff/index.php" )
    *        )
    *
    * Note the index "sub" and the nesting. Note also that 1, 11, 12, 2 
    * must be unique. The class uses them as ID's. 
    * 
    * @param    array
    * @access   public
    * @see      append(), update(), delete()
    */
    function setMenu($menu) {
        
        $this->menu = $menu;
        $this->urlmap = array();
        
    } // end func setMenu
    
    
    /**
    * Sets the type / format of the menu: tree, rows or urhere.
    *
    * @param    string  "tree", "rows", "urhere", "prevnext"
    * @access   public
    */
    function setMenuType($menu_type) {
    
        $this->menu_type = strtolower($menu_type);
        
    } // end func setMenuType


    /**
    * Returns the HTML menu.
    * 
    * @param    string  Menu type: tree, urhere, rows, prevnext, sitemap
    * @return   string  HTML of the menu
    * @access   public
    */
    function get($menu_type = "") {
        if ("" != $menu_type)
            $this->setMenuType($menu_type);

        $this->html = ""; 
                   
        // buildMenu for rows cares on this itself
        if ("rows" != $this->menu_type)
            $this->html  .= $this->getStart();
        
        // storing to a class variable saves some recursion overhead
        $this->path = $this->getPath();
        
        if ("sitemap" == $this->menu_type) {
        
            $this->setMenuType("tree");
            $this->buildSitemap($this->menu);
            $this->setMenuType("sitemap");
            
        } else {
            $this->buildMenu($this->menu);
        }    
        
        if ("rows" != $this->menu_type)
            $this->html .= $this->getEnd();
        
        return $this->html;
    } // end func get
    
    
    /**
    * Prints the HTML menu.
    * 
    * @brother  get()
    * @return   void
    */
    function show($menu_type = "") {
        print $this->get($menu_type);
    } // end func show
    

    /**
    * Returns the prefix of the HTML menu items.
    * 
    * @return   string  HTML menu prefix
    * @see      getEnd(), get()
    */
    function getStart() {
    
        $html = "";
        switch ($this->menu_type) {
            case "rows":
            case "prevnext":
                $html .= '<table border><tr>';
                break;
                
            case "tree":
            case "urhere":
            case "sitemap":
                $html .= '<table border>';
                break;
        }
        
        return $html;
    } // end func getStart

    
    /**
    * Returns the postfix of the HTML menu items.
    *
    * @return   string  HTML menu postfix
    * @see      getStart(), get()
    */
    function getEnd() {

        $html = "";
        switch ($this->menu_type) {
            case "rows":
            case "prevnext":
                $html .= '</tr></table>';
                break;
            
            case "tree":
            case "urhere":
            case "sitemap":
                $html .= '</table>';
                break;
        }
        
        return $html;
    } // end func getEnd
    
    
    /**
    * Build the menu recursively.
    *
    * @param    array   first call: $this->menu, recursion: submenu
    * @param    integer level of recursion, current depth in the tree structure
    * @param    integer prevnext flag
    */
    function buildMenu($menu, $level = 0, $flag_stop_level = -1) {
        static $last_node = array(), $up_node = array();
        
        // the recursion goes slightly different for every menu type
        switch ($this->menu_type) {
            
            case "tree":
            
                // loop through the (sub)menu
                foreach ($menu as $node_id => $node) {

                    if ($this->current_url == $node["url"]) {
                        // menu item that fits to this url - "active" menu item
                        $type = 1;
                    } else if (isset($this->path[$level]) && $this->path[$level] == $node_id) {
                        // processed menu item is part of the path to the active menu item
                        $type = 2;
                    } else {
                        // not selected, not a part of the path to the active menu item
                        $type = 0;
                    }
        
                    $this->html .= $this->getEntry($node, $level, $type);
                    
                    // follow the subtree if the active menu item is in it
                    if ($type && isset($node["sub"]))
                        $this->buildMenu($node["sub"], $level + 1);
                }
                break;
                
            case "rows":
                
                // every (sub)menu has it's own table
                $this->html .= $this->getStart();
                
                $submenu = false;
                
                // loop through the (sub)menu
                foreach ($menu as $node_id => $node) {
                
                    if ($this->current_url == $node["url"]) {
                        // menu item that fits to this url - "active" menu item
                        $type = 1;
                    } else if (isset($this->path[$level]) && $this->path[$level] == $node_id) {
                        // processed menu item is part of the path to the active menu item
                        $type = 2;
                    } else {
                        // not selected, not a part of the path to the active menu item
                        $type = 0;
                    }

                    $this->html .= $this->getEntry($node, $level, $type);
                    
                    // remember the subtree
                    if ($type && isset($node["sub"]))
                        $submenu = $node["sub"];
                        
                }
                
                // close the table for this level
                $this->html .= $this->getEnd();
                
                // go deeper if neccessary
                if ($submenu)
                    $this->buildMenu($submenu, $level + 1);

                break;

            case "urhere":

                // loop through the (sub)menu
                foreach ($menu as $node_id => $node) {
                        
                    if ($this->current_url == $node["url"]) {
                        // menu item that fits to this url - "active" menu item
                        $type = 1;
                    } else if (isset($this->path[$level]) && $this->path[$level] == $node_id) {
                        // processed menu item is part of the path to the active menu item
                        $type = 2;
                    } else {
                        // not selected, not a part of the path to the active menu item
                        $type = 0;
                    }
        
                    // follow the subtree if the active menu item is in it
                    if ($type) {
                        $this->html .= $this->getEntry($node, $level, $type);
                        if (isset($node["sub"])) {
                            $this->buildMenu($node["sub"], $level + 1);
                            continue;
                        }
                    }
                    
                }
                break;
                
          case "prevnext":
                
                // loop through the (sub)menu
                foreach ($menu as $node_id => $node) {
                    
                    if (-1 != $flag_stop_level) {
                    
                        // add this item to the menu and stop recursion - (next >>) node
                        if ($flag_stop_level == $level) {
                            $this->html .= $this->getEntry($node, $level, 4);
                            $flag_stop_level = -1;
                        }

                        break;
                        
                    
                    } else if ($this->current_url == $node["url"]) {
                        // menu item that fits to this url - "active" menu item
                        $type = 1;
                        $flag_stop_level = $level;
                        
                        if (0 != count($last_node)) {
                        
                            $this->html .= $this->getEntry($last_node, $level, 3);
                          
                        } else {
                        
                            // WARNING: if there's no previous take the first menu entry - you might not like this rule!
                            reset($this->menu);
                            list($node_id, $first_node) = each($this->menu);
                            $this->html .= $this->getEntry($first_node, $level, 3);
                            
                        
                        }
                        
                        if (0 != count($up_node)) {
                          
                          $this->html .= $this->getEntry($up_node, $level, 5);
                          
                        } else {
                        
                            // WARNING: if there's no up take the first menu entry - you might not like this rule!
                            reset($this->menu);
                            list($node_id, $first_node) = each($this->menu);
                            $this->html .= $this->getEntry($first_node, $level, 5);
                          
                        }
                        
                    } else if (isset($this->path[$level]) && $this->path[$level] == $node_id) {
                        // processed menu item is part of the path to the active menu item
                        $type = 2;
                     
                    } else {
                    
                        $type = 0;
                    
                    }
                    
                    // remember the last (<< prev) node
                    $last_node = $node;
                    
                    // follow the subtree if the active menu item is in it
                    if ($type && isset($node["sub"])) {
                        $up_node = $node;
                        $flag_stop_level = $this->buildMenu($node["sub"], $level + 1, (-1 != $flag_stop_level) ? $flag_stop_level + 1 : -1); 
                    }
                }
                break;
                
            
        } // end switch menu_type
        
        return ($flag_stop_level) ? $flag_stop_level - 1 : -1;
    } // end func buildMenu
    
    
    /**
    * Build the menu recursively.
    *
    * @param    array   first call: $this->menu, recursion: submenu
    * @param    int     level of recursion, current depth in the tree structure
    */
    function buildSitemap($menu, $level = 0) {

        // loop through the (sub)menu
        foreach ($menu as $node_id => $node) {
        
            if ($this->current_url == $node["url"]) {
                // menu item that fits to this url - "active" menu item
                $type = 1;
            } else if (isset($this->path[$level]) && $this->path[$level] == $node_id) {
                // processed menu item is part of the path to the active menu item
                $type = 2;
            } else {
                // not selected, not a part of the path to the active menu item
                $type = 0;
            }

            $this->html .= $this->getEntry($node, $level, $type);
            
            // follow the subtree if the active menu item is in it
            if (isset($node["sub"]))
                $this->buildSitemap($node["sub"], $level + 1);
        }

    } // end func buildSitemap
    
    
    /**
    * Returns the HTML of one menu item.
    * 
    * @param    array   menu item data (node data)
    * @param    integer level in the menu tree
    * @param    integer menu item type: 0, 1, 2. 0 => plain menu item,
    *                   1 => selected (active) menu item, 2 => item 
    *                   is a member of the path to the selected (active) menu item
    *                   3 => previous entry, 4 => next entry
    * @return   string  HTML
    * @see      buildMenu()
    */
    function getEntry(&$node, $level, $item_type) {

        $html = "";
        
        if ("tree" == $this->menu_type) {
            // tree menu
            $html .= '<tr>';
            $indent = "";
            if ($level) 
                for ($i = 0; $i < $level; $i++)
                    $indent .= '&nbsp;&nbsp;&nbsp;';
        }
        
        // draw the <td></td> cell depending on the type of the menu item
        switch ($item_type) {
            case 0:
                // plain menu item
                $html .= sprintf('<td>%s<a href="%s">%s</a></td>',
                                    $indent,
                                    $node["url"],
                                    $node["title"]
                                 );
                break;
                
            case 1:
                // selected (active) menu item
                $html .= sprintf('<td>%s<b>%s</b></td>', 
                                   $indent,
                                   $node["title"]
                                );
                break;
                
            case 2:
                // part of the path to the selected (active) menu item
                $html .= sprintf('<td>%s<b><a href="%s">%s</a></b>%s</td>',
                                    $indent,
                                    $node["url"],
                                    $node["title"],
                                    ("urhere" == $this->menu_type) ? " &gt;&gt; " : ""
                                );
                break;
                
            case 3: 
                // << previous url
                $html .= sprintf('<td>%s<a href="%s">&lt;&lt; %s</a></td>',
                                    $indent,
                                    $node["url"],
                                    $node["title"]
                                );
                break;

            case 4:
                // next url >>
                $html .= sprintf('<td>%s<a href="%s">%s &gt;&gt;</a></td>',
                                    $indent,
                                    $node["url"],
                                    $node["title"]
                                );
                break;

            case 5:
                // up url ^^
                $html .= sprintf('<td>%s<a href="%s">^ %s ^</a></td>',
                                    $indent,
                                    $node["url"],
                                    $node["title"]
                          );
                break;
                
        }
            
        if ("tree" == $this->menu_type)
            $html .= '</tr>';

        return $html;
    } // end func getEnty


    /*
    * Returns the path of the current page in the menu "tree".
    *
    * @return   array    path to the selected menu item
    * @see      buildPath(), $urlmap
    */
    function getPath() {

        $this->current_url = $this->getCurrentURL();
        $this->buildPath($this->menu, array());  

        while ($this->current_url && !isset($this->urlmap[$this->current_url]))
          $this->current_url = substr($this->current_url, 0, -1);
        
        return $this->urlmap[$this->current_url];
    } // end func getPath
    
    
    /**
    * Computes the path of the current page in the menu "tree".
    * 
    * @param    array       first call: menu hash / recursion: submenu hash
    * @param    array       first call: array() / recursion: path
    * @return   boolean     true if the path to the current page was found, 
    *                       otherwise false. Only meaningful for the recursion.
    * @global   $PHP_SELF   
    * @see      getPath(), $urlmap
    */
    function buildPath($menu, $path) {

        // loop through the (sub)menu
        foreach ($menu as $node_id => $node) {
        
            // save the path of the current node in the urlmap
            $this->urlmap[$node["url"]] = $path;
            
            // we got'em - stop the search by returning true
            // KLUDGE: getPath() works with the best alternative for a URL if there's
            // no entry for a URL in the menu hash, buildPath() does not perform this test
            // and might do some unneccessary recursive runs.
            if ($node["url"] == $this->current_url)
               return true;
               
            // current node has a submenu               
            if ($node["sub"]) {
                
                // submenu path = current path + current node
                $subpath = $path;
                $subpath[] = $node_id;
                
                // continue search recursivly - return is the inner loop finds the 
                // node that belongs to the current url
                if ($this->buildPath($node["sub"], $subpath))
                    return true;
            }

        } 

        // not found
        return false;        
    } // end func buildPath
    
    
    /**
    * Returns the URL of the currently selected page.
    *
    * The returned string is used for all test against the URL's
    * in the menu structure hash.
    *
    * @return string
    */
    function getCurrentURL() {
      return $GLOBALS["PHP_SELF"];
    } // end func getCurrentURL
    
} // end class menu
?>