<?php
/**
* Function_Cache
* 
* Purpose:
* 
*   Caching the result and output of functions.
* 
* Example:
* 
*     require_once "Cache/Function.php";
*
*     $FUNCTION_CACHE_CONTAINER = "file";
*     $FUNCTION_CACHE_CONTAINER_OPTIONS = array(
*         "cache_dir" => "/tmp/",
*         "filename_prefix" => "cache_");
*
*     function foo($string)
*     {
*         print $string . "<br>";
*         for($i=1;$i<10000000;$i++){}
*         return strrev($string);
*     }
* 
*     print cached_function_call("foo", "test");
* 
* Note:
* 
*   You cannot cache every function. You should only cache 
*   functions that only depend on their arguments and don't use
*   global or static variables, don't rely on database queries or 
*   files, and so on.
* 
* @author       Sebastian Bergmann <sb@sebastian-bergmann.de>
* @module       Function_Cache
* @modulegroup  Function_Cache
* @package      Cache
* @version      $Revision: 1.1.1.1 $
* @access       public
*/

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
// | Authors: Sebastian Bergmann <sb@sebastian-bergmann.de>               |
// +----------------------------------------------------------------------+
//
// $Id: Function.php,v 1.1.1.1 2001/07/19 00:20:43 zarzycki Exp $

require_once 'Cache.php';

/**
* Calls a cacheable function or method.
*
* @return mixed $result
* @access public
*/
function cached_function_call()
{
    global $FUNCTION_CACHE_CONTAINER, $FUNCTION_CACHE_CONTAINER_OPTIONS;
    static $cache;

    // create Cache object, if needed
    if (!is_object($cache))
    {
        $cache = new Cache($FUNCTION_CACHE_CONTAINER, $FUNCTION_CACHE_CONTAINER_OPTIONS);
    }

    // get arguments
    $arguments = func_get_args();

    // generate cache id
    $id = md5(serialize($arguments));

    // query cache
    $cached_object = $cache->get($id);

    // cache hit
    if ($cached_object != NULL)
    {
        $output = $cached_object[0];
        $result = $cached_object[1];
    }

    // cache miss
    else
    {
        $function_name = array_shift($arguments);

        // call function, save output and result
        ob_start();
        $result = call_user_func_array($function_name, $arguments);
        $output = ob_get_contents();
        ob_end_clean();

        // store output and result of function call in cache
        $cache->save($id, array($output, $result));
    }

    print $output;
    return $result;
}
?>