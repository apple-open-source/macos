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
// $Id: sybase.php,v 1.1.1.3 2001/07/19 00:20:45 zarzycki Exp $
//
// Database independent query interface definition for PHP's Sybase
// extension.
//

require_once 'DB/common.php';

class DB_sybase extends DB_common
{
    // {{{ properties

    var $connection;
    var $phptype, $dbsyntax;
    var $prepare_tokens = array();
    var $prepare_types = array();

    // }}}
    // {{{ constructor

    function DB_sybase()
    {
        $this->DB_common();
        $this->phptype = 'sybase';
        $this->dbsyntax = 'sybase';
        $this->features = array(
            'prepare' => false,
            'pconnect' => true,
            'transactions' => false
        );
    }

    // }}}
    // {{{ connect()

    function connect($dsninfo, $persistent = false)
    {
        $this->dsn = $dsninfo;
        $dbhost = $dsninfo['hostspec'] ? $dsninfo['hostspec'] : 'localhost';
        $connect_function = $persistent ? 'sybase_pconnect' : 'sybase_connect';
        $conn = $dbhost ? $connect_function($dbhost) : false;
        $dsninfo['database'] && @sybase_select_db($dsninfo['database'], $conn);
        $this->connection = $conn;
        return DB_OK;
    }

    // }}}
    // {{{ disconnect()

    function disconnect()
    {
        return @sybase_close($this->connection);
    }

    // }}}
    // {{{ simpleQuery()

    function simpleQuery($query)
    {
        $this->last_query = $query;
        $query = $this->modifyQuery($query);
        $result = @sybase_query($query, $this->connection);
        if (!$result) {
            return $this->raiseError();
        }
        // Determine which queries that should return data, and which
        // should return an error code only.
        return DB::isManip($query) ? DB_OK : $result;
    }

    // }}}
    // {{{ fetchRow()
    function &fetchRow($result, $fetchmode = DB_FETCHMODE_DEFAULT)
    {
        if ($fetchmode == DB_FETCHMODE_DEFAULT) {
            $fetchmode = $this->fetchmode;
        }
        $row = ($fetchmode & DB_FETCHMODE_ASSOC) ? @sybase_fetch_array($result) : @sybase_fetch_row($result);
        if (!$row) {
            if ($errmsg = sybase_get_last_message()) {
                return $this->raiseError($errmsg);
            } else {
                return null;
            }
        }
        return $row;
    }

    // }}}
    // {{{ fetchInto()

    function fetchInto($result, &$ar, $fetchmode, $rownum=null)
    {
        if ($rownum !== null) {
            if (!sybase_data_seek($result, $rownum)) {
                return $this->raiseError();
            }
        }
        if ($fetchmode == DB_FETCHMODE_DEFAULT) {
            $fetchmode = $this->fetchmode;
        }
        $ar = ($fetchmode & DB_FETCHMODE_ASSOC) ? @sybase_fetch_array($result) : @sybase_fetch_row($result);
        if (!$ar) {
            if ($errmsg = sybase_get_last_message()) {
                return $this->raiseError($errmsg);
            } else {
                return null;
            }
        }
        return DB_OK;
    }

    // }}}
    // {{{ freeResult()

    function freeResult($result)
    {
        if (is_resource($result)) {
            return @sybase_free_result($result);
        }
        if (!isset($this->prepare_tokens[(int)$result])) {
            return false;
        }
        unset($this->prepare_tokens[(int)$result]);
        unset($this->prepare_types[(int)$result]);
        return true;
    }

    // }}}
    // {{{ numCols()

    function numCols($result)
    {
        $cols = @sybase_num_fields($result);
        if (!$cols) {
            return $this->raiseError();
        }
        return $cols;
    }

    // }}}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
?>
