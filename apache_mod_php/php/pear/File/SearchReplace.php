<?php
// +----------------------------------------------------------------------+
// | PHP version 4.0                                                      |
// +----------------------------------------------------------------------+
// | Copyright (c) 1997, 1998, 1999, 2000, 2001 The PHP Group             |
// +----------------------------------------------------------------------+
// | This source file is subject to version 2.0 of the PHP license,       |
// | that is bundled with this package in the file LICENSE, and is        |
// | available at through the world-wide-web at                           |
// | http://www.php.net/license/2_02.txt.                                 |
// | If you did not receive a copy of the PHP license and are unable to   |
// | obtain it through the world-wide-web, please send a note to          |
// | license@php.net so we can mail you a copy immediately.               |
// +----------------------------------------------------------------------+
// | Authors: Richard Heyes <richard.heyes@heyes-computing.net>           |
// +----------------------------------------------------------------------+
//
// $Id: SearchReplace.php,v 1.1.1.2 2001/12/14 22:14:51 zarzycki Exp $
//
// Search and Replace Utility
//

/**
 * Search and Replace Utility
 *
 * See http://www.heyes-computing.net/scripts/ for full tar/zip
 * including example file.
 *
 * @author  Richard Heyes <richard.heyes@heyes-computing.net>
 * @version 1.0
 * @package File
 */
class File_SearchReplace
{
    
    // {{{ Properties (All private)

    var $find;
    var $replace;
    var $files;
    var $directories;
    var $include_subdir;
    var $ignore_lines;
    var $ignore_sep;
    var $occurences;
    var $search_function;
    var $last_error;

    // }}}
    // {{{ Constructor

    /**
     * Sets up the object
     *
     * @access public
     * @param string $find                      The string/regex to find.
     * @param string $replace                   The string/regex to replace $find with.
     * @param array  $files                     The file(s) to perform this operation on.
     * @param array  $directories    (optional) The directories to perform this operation on.
     * @param int    $include_subdir            If performing on directories, whether to traverse subdirectories.
     * @param array  $ignore_lines              Ignore lines beginning with any of the strings in this array. This
     *                                          feature only works with the "normal" search.
     *
     * @author Richard Heyes <richard.heyes@heyes-computing.net>
     */
    function File_SearchReplace($find, $replace, $files, $directories = '', $include_subdir = 1, $ignore_lines = array())
    {

        $this->find            = $find;
        $this->replace         = $replace;
        $this->files           = $files;
        $this->directories     = $directories;
        $this->include_subdir  = $include_subdir;
        $this->ignore_lines    = $ignore_lines;

        $this->occurences      = 0;
        $this->search_function = 'search';
        $this->last_error      = '';

    }

    // }}}
    // {{{ getNumOccurences()

    /**
     * Accessor to return the number of occurences found.
     *
     * @access public
     * @return int Number of occurences found.
     *
     * @author Richard Heyes <richard.heyes@heyes-computing.net>
     */
    function getNumOccurences()
    {
        return $this->occurences;
    }

    // }}}
    // {{{ getLastError()

    /**
     * Accessor for retrieving last error.
     *
     * @access public
     * @return string The last error that occurred, if any.
     *
     * @author Richard Heyes <richard.heyes@heyes-computing.net>
     */
    function getLastError()
    {
        return $this->last_error;
    }

    // }}}
    // {{{ setFind()

    /**
     * Accessor for setting find variable.
     *
     * @access public
     * @param string $find The string/regex to find.
     *
     * @author Richard Heyes <richard.heyes@heyes-computing.net>
     */
    function setFind($find)
    {
        $this->find = $find;
    }

    // }}}
    // {{{ setReplace()

    /**
     * Accessor for setting replace variable.
     *
     * @access public
     * @param string $replace The string/regex to replace the find string/regex with.
     *
     * @author Richard Heyes <richard.heyes@heyes-computing.net>
     */
    function setReplace($replace)
    {
        $this->replace = $replace;
    }

    // }}}
    // {{{ setFiles()

    /**
     * Accessor for setting files variable.
     *
     * @access public
     * @param array $files The file(s) to perform this operation on.
     *
     * @author Richard Heyes <richard.heyes@heyes-computing.net>
     */
    function setFiles($files)
    {
        $this->files = $files;
    }

    // }}}
    // {{{ setDirectories()

    /**
     * Accessor for setting directories variable.
     *
     * @access public
     * @param array $directories The directories to perform this operation on.
     *
     * @author Richard Heyes <richard.heyes@heyes-computing.net>
     */
    function setDirectories($directories)
    {
        $this->directories = $directories;
    }

    // }}}
    // {{{ setIncludeSubdir

    /**
     * Accessor for setting include_subdir variable.
     *
     * @access public
     * @param int $include_subdir Whether to traverse subdirectories or not.
     *
     * @author Richard Heyes <richard.heyes@heyes-computing.net>
     */
    function setIncludeSubdir($include_subdir)
    {
        $this->include_subdir = $include_subdir;
    }

    // }}}
    // {{{ setIgnoreLines()

    /**
     * Accessor for setting ignore_lines variable.
     *
     * @access public
     * @param array $ignore_lines Ignore lines beginning with any of the strings in this array. This
     *                            feature only works with the "normal" search.
     *
     * @author Richard Heyes <richard.heyes@heyes-computing.net>
     */
    function setIgnoreLines($ignore_lines)
    {
        $this->ignore_lines = $ignore_lines;
    }

    // }}}
    // {{{ setSearchFunction()

    /**
     * Function to determine which search function is used.
     *
     * @access public
     * @param string The search function that should be used. Can be any one of:
     *               normal - Default search. Goes line by line. Ignore lines feature only works with this type.
     *               quick  - Uses str_replace for straight replacement throughout file. Quickest of the lot.
     *               preg   - Uses preg_replace(), so any regex valid with this function is valid here.
     *               ereg   - Uses ereg_replace(), so any regex valid with this function is valid here.
     *
     * @author Richard Heyes <richard.heyes@heyes-computing.net>
     */
    function setSearchFunction($search_function)
    {
        switch($search_function) {
        case 'normal': $this->search_function = 'search';
            return TRUE;
            break;

        case 'quick' : $this->search_function = 'quickSearch';
            return TRUE;
            break;

        case 'preg'  : $this->search_function = 'pregSearch';
            return TRUE;
            break;

        case 'ereg'  : $this->search_function = 'eregSearch';
            return TRUE;
            break;

        default      : $this->last_error      = 'Invalid search function specified';
            return FALSE;
            break;
        }
    }

    // }}}
    // {{{ search()

    /**
     * Default ("normal") search routine.
     *
     * @access private
     * @param string $filename The filename to search and replace upon.
     * @return array Will return an array containing the new file contents and the number of occurences.
     *               Will return FALSE if there are no occurences.
     *
     * @author Richard Heyes <richard.heyes@heyes-computing.net>
     */
    function search($filename)
    {

        $occurences = 0;
        $file_array = file($filename);

        for ($i=0; $i<count($file_array); $i++) {

            if (count($this->ignore_lines) > 0) {
                for ($j=0; $j<count($this->ignore_lines); $j++) {
                    if (substr($file_array[$i],0,strlen($this->ignore_lines[$j])) == $this->ignore_lines[$j]) continue 2;
                }
            }

            $occurences += count(explode($this->find, $file_array[$i])) - 1;
            $file_array[$i] = str_replace($this->find, $this->replace, $file_array[$i]);
        }
        if ($occurences > 0) $return = array($occurences, implode('', $file_array)); else $return = FALSE;
        return $return;

    }

    // }}}
    // {{{ quickSearch()

    /**
     * Quick search routine.
     *
     * @access private
     * @param string $filename The filename to search and replace upon.
     * @return array Will return an array containing the new file contents and the number of occurences.
     *               Will return FALSE if there are no occurences.
     *
     * @author Richard Heyes <richard.heyes@heyes-computing.net>
     */
    function quickSearch($filename)
    {

        clearstatcache();

        $file       = fread($fp = fopen($filename, 'r'), filesize($filename)); fclose($fp);
        $occurences = count(explode($this->find, $file)) - 1;
        $file       = str_replace($this->find, $this->replace, $file);

        if ($occurences > 0) $return = array($occurences, $file); else $return = FALSE;
        return $return;

    }

    // }}}
    // {{{ pregSearch()

    /**
     * Preg search routine.
     *
     * @access private
     * @param string $filename The filename to search and replace upon.
     * @return array Will return an array containing the new file contents and the number of occurences.
     *               Will return FALSE if there are no occurences.
     *
     * @author Richard Heyes <richard.heyes@heyes-computing.net>
     */
    function pregSearch($filename)
    {

        clearstatcache();

        $file       = fread($fp = fopen($filename, 'r'), filesize($filename)); fclose($fp);
        $occurences = count($matches = preg_split($this->find, $file)) - 1;
        $file       = preg_replace($this->find, $this->replace, $file);

        if ($occurences > 0) $return = array($occurences, $file); else $return = FALSE;
        return $return;

    }

    // }}}
    // {{{ eregSearch()

    /**
     * Ereg search routine.
     *
     * @access private
     * @param string $filename The filename to search and replace upon.
     * @return array Will return an array containing the new file contents and the number of occurences.
     *               Will return FALSE if there are no occurences.
     *
     * @author Richard Heyes <richard.heyes@heyes-computing.net>
     */
    function eregSearch($filename)
    {

        clearstatcache();

        $file = fread($fp = fopen($filename, 'r'), filesize($filename)); fclose($fp);

        $occurences = count($matches = split($this->find, $file)) -1;
        $file       = ereg_replace($this->find, $this->replace, $file);

        if ($occurences > 0) $return = array($occurences, $file); else $return = FALSE;
        return $return;

    }

    // }}}
    // {{{ writeout()
    
    /**
     * Function to writeout the file contents.
     *
     * @access private
     * @param string $filename The filename of the file to write.
     * @param string $contents The contents to write to the file.
     *
     * @author Richard Heyes <richard.heyes@heyes-computing.net>
     */
    function writeout($filename, $contents)
    {

        if ($fp = @fopen($filename, 'w')) {
            flock($fp,2);
            fwrite($fp, $contents);
            flock($fp,3);
            fclose($fp);
        } else {
            $this->last_error = 'Could not open file: '.$filename;
        }

    }

    // }}}
    // {{{ doFiles()

    /**
     * Function called by doSearch() to go through any files that need searching.
     *
     * @access private
     * @param string $ser_func The search function to use.
     *
     * @author Richard Heyes <richard.heyes@heyes-computing.net>
     */
    function doFiles($ser_func)
    {
        if (!is_array($this->files)) $this->files = explode(',', $this->files);
        for ($i=0; $i<count($this->files); $i++) {
            if ($this->files[$i] == '.' OR $this->files[$i] == '..') continue;
            if (is_dir($this->files[$i]) == TRUE) continue;
            $newfile = $this->$ser_func($this->files[$i]);
            if (is_array($newfile) == TRUE){
                $this->writeout($this->files[$i], $newfile[1]);
                $this->occurences += $newfile[0];
            }
        }
    }

    // }}}
    // {{{ doDirectories()

    /**
     * Function called by doSearch() to go through any directories that need searching.
     *
     * @access private
     * @param string $ser_func The search function to use.
     *
     * @author Richard Heyes <richard.heyes@heyes-computing.net>
     */
    function doDirectories($ser_func)
    {
        if (!is_array($this->directories)) $this->directories = explode(',', $this->directories);
        for ($i=0; $i<count($this->directories); $i++) {
            $dh = opendir($this->directories[$i]);
            while ($file = readdir($dh)) {
                if ($file == '.' OR $file == '..') continue;

                if (is_dir($this->directories[$i].$file) == TRUE) {
                    if ($this->include_subdir == 1) {
                        $this->directories[] = $this->directories[$i].$file.'/';
                        continue;
                    } else {
                        continue;
                    }
                }

                $newfile = $this->$ser_func($this->directories[$i].$file);
                if (is_array($newfile) == TRUE) {
                    $this->writeout($this->directories[$i].$file, $newfile[1]);
                    $this->occurences += $newfile[0];
                }
            }
        }
    }

    // }}}
    // {{{ doSearch()
    
    /**
     * This starts the search/replace off. Call this to do the search.
     * First do whatever files are specified, and/or if directories are specified,
     * do those too.
     *
     * @access public
     *
     * @author Richard Heyes <richard.heyes@heyes-computing.net>
     */
    function doSearch()
    {
        if ($this->find != '') {
            if ((is_array($this->files) AND count($this->files) > 0) OR $this->files != '') $this->doFiles($this->search_function);
            if ($this->directories != '')                                                   $this->doDirectories($this->search_function);
        }
    }
    
    // }}}

}
?>
