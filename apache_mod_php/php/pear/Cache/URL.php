<?php
/**
* URL_Cache
* 
* Purpose:
* 
*   Caching the contents of a remote URL.
* 
* Example:
* 
*     require_once "Cache/URL.php";
*
*     $FUNCTION_CACHE_CONTAINER = "file";
*     $FUNCTION_CACHE_CONTAINER_OPTIONS = array(
*         "cache_dir" => "/tmp/",
*         "filename_prefix" => "cache_");
*
*     $data = get_cached_url("http://www.test.com/");
* 
* @author       Sebastian Bergmann <sb@sebastian-bergmann.de>
* @module       URL_Cache
* @modulegroup  URL_Cache
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
// $Id: URL.php,v 1.1.1.1 2001/07/19 00:20:43 zarzycki Exp $

require_once 'Cache.php';

/**
* Caches the contents of a remote URL.
*
* @param  string $url
* @param  mixed  $expires
* @return string $data
* @access public
*/
function get_cached_url($url, $expires = 0) {
    global $FUNCTION_CACHE_CONTAINER, $FUNCTION_CACHE_CONTAINER_OPTIONS;
    static $cache;

    // create Cache object, if needed
    if (!is_object($cache)) {
        $cache = new Cache($FUNCTION_CACHE_CONTAINER, $FUNCTION_CACHE_CONTAINER_OPTIONS);
    }

    // generate Cache ID
    $id = md5($url);

    // query Cache
    $data = $cache->get($id);

    // Cache miss: Retrieve document at given URL and store it
    if ($data == NULL) {
        $fp = @fopen($url, "r");

        if($fp) {
          $data = fread($fp, 65536);
          fclose($fp);
          $cache->save($id, $data, $expires);
          
        }
    }

    return $data;
}
?>
