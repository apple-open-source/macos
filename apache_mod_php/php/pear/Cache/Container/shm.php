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
// | Authors: Ulf Wendel <ulf.wendel@phpdoc.de>                           |
// |          Sebastian Bergmann <sb@sebastian-bergmann.de>               |
// |          Björn Schotte <bjoern@php.net>                              |
// +----------------------------------------------------------------------+
//
// $Id: shm.php,v 1.1.1.1 2001/07/19 00:20:43 zarzycki Exp $

require_once 'Cache/Container.php';

/**
* Stores cache data into shared memory.
*
* @author   Björn Schotte <bjoern@php.net>
* @version  $Id: shm.php,v 1.1.1.1 2001/07/19 00:20:43 zarzycki Exp $
* @package  Cache
*/
class Cache_Container_shm extends Cache_Container {
    
    function Cache_Container_shm($options = "")
    {
    }
    
    function fetch($id, $group)
    {
    } // end func fetch
    
    function save($id, $data, $expire, $group, $userdata)
    {
        $this->flushPreload($id, $group);
    } // end func save
    
    function delete($id, $group)
    {
        $this->flushPreload($id, $group);
    } // end func delete
    
    function flush($group = "")
    {
        $this->flushPreload();
    } // end func flush
    
    function idExists($id, $group)
    {
    } // end func isExists
    
    function garbageCollection()
    {
    } // end func garbageCollection
    
}
?>
