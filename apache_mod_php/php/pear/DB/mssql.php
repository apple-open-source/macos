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
// $Id: mssql.php,v 1.1.1.4 2001/07/19 00:20:44 zarzycki Exp $
//
// Database independent query interface definition for PHP's Microsoft SQL Server
// extension.
//

require_once 'DB/common.php';

class DB_mssql extends DB_common
{
    var $connection;
    var $phptype, $dbsyntax;
    var $prepare_tokens = array();
    var $prepare_types = array();

    function DB_mssql()
    {
        $this->DB_common();
        $this->phptype = 'mssql';
        $this->dbsyntax = 'mssql';
        $this->features = array(
            'prepare' => false,
            'pconnect' => true,
            'transactions' => false
        );
    }

    function connect($dsninfo, $persistent = false)
    {
        $this->dsn = $dsninfo;
        $user = $dsninfo['username'];
        $pw = $dsninfo['password'];
        $dbhost = $dsninfo['hostspec'] ? $dsninfo['hostspec'] : 'localhost';
        $connect_function = $persistent ? 'mssql_pconnect' : 'mssql_connect';
        if ($dbhost && $user && $pw) {
            $conn = $connect_function($dbhost, $user, $pw);
        } elseif ($dbhost && $user) {
            $conn = $connect_function($dbhost, $user);
        } else {
            $conn = $connect_function($dbhost);
        }
        if ($dsninfo['database']) {
            @mssql_select_db($dsninfo['database'], $conn);
        } else {
            return $this->raiseError(mssql_get_last_message());
        }
        $this->connection = $conn;
        return DB_OK;
    }

    function disconnect()
    {
        return @mssql_close($this->connection);
    }

    function simpleQuery($query)
    {
        $this->last_query = $query;
        $query = $this->modifyQuery($query);
        $result = @mssql_query($query, $this->connection);
        if (!$result) {
            return $this->raiseError(mssql_get_last_message());
        }
        // Determine which queries that should return data, and which
        // should return an error code only.
        return DB::isManip($query) ? DB_OK : $result;
    }

    function &fetchRow($result, $fetchmode = DB_FETCHMODE_DEFAULT, $rownum=null)
    {
        if ($fetchmode == DB_FETCHMODE_DEFAULT) {
            $fetchmode = $this->fetchmode;
        }
        $res = $this->fetchInto ($result, $arr, $fetchmode, $rownum);
        if ($res !== DB_OK) {
            return $res;
        }
        return $arr;
    }

    function fetchInto($result, &$ar, $fetchmode, $rownum=null)
    {
        if ($rownum !== null) {
            if (!@mssql_data_seek($result, $rownum)) {
                return null;
            }
        }
        if ($fetchmode & DB_FETCHMODE_ASSOC) {
            $ar = @mssql_fetch_array($result);
        } else {
            $ar = @mssql_fetch_row($result);
        }
        if (!$ar) {
            if ($msg = mssql_get_last_message()) {
                return $this->raiseError($msg);
            } else {
                return null;
            }
        }
        return DB_OK;
    }

    function freeResult($result)
    {
        if (is_resource($result)) {
            return @mssql_free_result($result);
        }
        if (!isset($this->prepare_tokens[$result])) {
            return false;
        }
        unset($this->prepare_tokens[$result]);
        unset($this->prepare_types[$result]);
        return true;
    }

    function numCols($result)
    {
        $cols = @mssql_num_fields($result);
        if (!$cols) {
            return $this->raiseError(mssql_get_last_message());
        }
        return $cols;
    }

    function numRows($result)
    {
        $rows = @mssql_num_rows($result);
        if (!$rows) {
            return $this->raiseError(mssql_get_last_message());
        }
        return $rows;
    }
}

?>
