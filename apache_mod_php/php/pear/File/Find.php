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
// | Authors: Sterling Hughes <sterling@php.net>                          |
// +----------------------------------------------------------------------+
//
// $Id: Find.php,v 1.1.1.4 2001/12/14 22:14:50 zarzycki Exp $
//

require_once 'PEAR.php';

/**
*  Commonly needed functions searching directory trees
*
* @access public
* @version $Id: Find.php,v 1.1.1.4 2001/12/14 22:14:50 zarzycki Exp $
* @package File
* @author Sterling Hughes <sterling@php.net>
*/
class File_Find
{
    /**
    * internal dir-list
    * @var array
    */
    var $_dirs       = array ();
    /**
    * founded files
    * @var array
    */
    var $files       = array ();
    /**
    * founded dirs
    * @var array
    */
    var $directories = array ();

    /**
     * Search the current directory to find matches for the
     * the specified pattern.
     *
     * @param string $pattern a string containing the pattern to search
     * the directory for.
     *
     * @param string $direct_path a string containing the directory path
     * to search.
     *
     * @param string $pattern_type a string containing the type of
     * pattern matching functions to use (can either be 'php' or
     * 'perl').
     *
     * @return array containing all of the files and directories
     * matching the pattern or null if no matches
     *
     * @author Sterling Hughes <sterling@php.net>
     * @access public
     */
    function &glob ($pattern, $dirpath, $pattern_type='php')
    {
        $dh = @opendir ($dirpath);

        if (!$dh) {
            $pe = new FileFindException("Cannot open directory");
            return ($pe);
        }

        $match_function = File_Find::_determineRegex($pattern, $pattern_type);
        $matches = array();
        while ($entry = @readdir ($dh)) {
            if ($match_function($pattern, $entry) &&
                $entry != '.'                     &&
                $entry != '..') {
                $matches[] = $entry;
            }
        }

        @closedir ($dh);
        return count($matches) > 0 ? $matches : null;
    }

    /**
     * Map the directory tree given by the directory_path parameter.
     *
     * @param string $directory_path contains the directory path that you
     * want to map.
     *
     * @return array a two element array, the first element containing a list
     * of all the directories, the second element containing a list of all the
     * files.
     *
     * @author Sterling Hughes <sterling@php.net>
     * @access public
     */
    function &maptree ($directory)
    {
        $this->_dirs = array($directory);

        while (count($this->_dirs)) {
            $dir = array_pop($this->_dirs);
            File_Find::_build($dir);
            array_push($this->directories, $dir);
        }

        return array($this->directories, $this->files);
    }

    /**
     * Search the specified directory tree with the specified pattern.  Return an
     * array containing all matching files (no directories included).
     *
     * @param string $pattern the pattern to match every file with.
     *
     * @param string $directory the directory tree to search in.
     *
     * @param string $regex_type the type of regular expression support to use, either
     * 'php' or 'perl'.
     *
     * @return array a list of files matching the pattern parameter in the the directory
     * path specified by the directory parameter
     *
     * @author Sterling Hughes <sterling@php.net>
     * @access public
     */
    function &search ($pattern, $directory, $type='php') {
        list (,$files)  = File_Find::maptree($directory);
        $match_function = File_Find::_determineRegex($pattern, $type);

        reset($files);
        while (list(,$entry) = each($files)) {
            if ($match_function($pattern, $entry))
                $matches[] = $entry;
        }

        return ($matches);
    }
    /**
     * Determine whether or not a variable is a PEAR exception
     *
     * @param object PEAR_Error $var the variable to test.
     *
     * @return boolean returns true if the variable is a PEAR error, otherwise
     * it returns false.
     * @access public
     */
    function isError (&$var)
    {
        return PEAR::isError($var);
    }

    /**
     * Fetch the current File_Find version
     *
     * @return string the current File_Find version.
     * @access public
     */
    function File_Find_version()
    {
         return 1.1;
    }
    /**
     * internal function to build singular directory trees, used by
     * File_Find::maptree()
     *
     * @param string $directory name of the directory to read
     * @return void
     */
    function _build ($directory)
    {
        $dh = @opendir ($directory);

        if (!$dh) {
            $pe = new FileFindException("Cannot open directory");
            return $pe;
        }

        while ($entry = @readdir($dh)) {
            if ($entry != '.' &&
                $entry != '..') {

                $entry = "$directory/$entry";

                if (is_dir($entry))
                    array_push($this->_dirs, $entry);
                else
                    array_push($this->files, $entry);

            }

        }

        @closedir($dh);
    }

    /**
     * internal function to determine the type of regular expression to
     * use, implemented by File_Find::glob() and File_Find::search()
     *
     * @param string $type given RegExp type
     * @return string kind of function ( "eregi", "ereg" or "preg_match") ;
     *
     */
    function _determineRegex ($pattern, $type)
    {
        if (! strcasecmp($type, 'perl')) {
            $match_function = 'preg_match';
        } else if (! strcasecmp(substr($pattern, -2), '/i')) {
            $match_function = 'eregi';
        } else {
            $match_function = 'ereg';
        }

        return $match_function;
    }

//End Class
}
/**
* Exception Class for Errorhandling of File_Find
* @access public
*/
class FileFindException extends PEAR_Error
{
    /**
    * classname
    * @var string
    */
    var $classname             = 'FileFindException';
    /**
    * Message in front of the error message
    * @var string
    */
    var $error_message_prepend = 'Error in File_Find';
    /**
    * Creates a PEAR_Error object
    *
    * @param string $message    Error message
    * @param int    $mode       Error mode
    * @param int    $level      Error level
    *
    * @return object PEAR_Error
    * @access public
    */
    function FileFindException ($message, $mode = PEAR_ERROR_RETURN, $level = E_USER_NOTICE)
    {
        $this->PEAR_Error($message, $mode, $level);
    }
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */

?>
