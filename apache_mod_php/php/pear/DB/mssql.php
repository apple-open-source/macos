<?php
//
// +----------------------------------------------------------------------+
// | PHP version 4.0                                                      |
// +----------------------------------------------------------------------+
// | Copyright (c) 1997, 1998, 1999, 2000 The PHP Group                   |
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
// $Id: mssql.php,v 1.1.1.3 2001/01/25 05:00:29 wsanchez Exp $
//
// Database independent query interface definition for PHP's Microsoft SQL Server
// extension.
//

require_once 'DB/common.php';

class DB_mssql extends DB_common {

    var $connection;
    var $phptype, $dbsyntax;
    var $prepare_tokens = array();
    var $prepare_types = array();

    function DB_mssql() {
        $this->phptype = 'mssql';
        $this->dbsyntax = 'mssql';
        $this->features = array(
            'prepare' => false,
            'pconnect' => true,
            'transactions' => false
        );
    }

    function connect (&$dsn, $persistent=false) {
        if(is_array($dsn)) {
            $dsninfo = &$dsn;
        } else {
            $dsninfo = DB::parseDSN($dsn);
        }
        if (!$dsninfo || !$dsninfo['phptype']) {
            return $this->raiseError(); 
        }

        $user = $dsninfo['username'];
        $pw = $dsninfo['password'];
        $dbhost = $dsninfo['hostspec'] ? $dsninfo['hostspec'] : 'localhost';
        $connect_function = $persistent ? 'mssql_pconnect' : 'mssql_connect';
        if ($dbhost && $user && $pw) {
            $conn = $connect_function($dbhost, $user, $pw);
        } elseif ($dbhost && $user) {
            $conn = $connect_function($dbhost,$user);
        } else {
            $conn = $connect_function($dbhost);
        }
        if ($dsninfo['database']) {
            @mssql_select_db($dsninfo['database'], $conn);
        } else {
            return $this->raiseError();
        }
        $this->connection = $conn;
        return DB_OK;
    }

    function disconnect() {
        return @mssql_close($this->connection);
    }

    function &query( $stmt ) {
	$this->last_query = $query;
        $result = @mssql_query($stmt, $this->connection);
        if (!$result) {
            return $this->raiseError();
        }
        // Determine which queries that should return data, and which
        // should return an error code only.
        if (preg_match('/(SELECT|SHOW|LIST|DESCRIBE)/i', $stmt)) {
            $resultObj = new DB_result($this, $result);
            return $resultObj;
        } else {
            return DB_OK;
        }
    }

    function simpleQuery($stmt) {
	$this->last_query = $query;
        $result = @mssql_query($stmt, $this->connection);
        if (!$result) {
            return $this->raiseError();
        }
        // Determine which queries that should return data, and which
        // should return an error code only.
        if ( preg_match('/(SELECT|SHOW|LIST|DESCRIBE)/i', $stmt) ) {
            return $result;
        } else {
            return DB_OK;
        }
    }

    function &fetchRow($result, $fetchmode=DB_FETCHMODE_DEFAULT) {
	if ($fetchmode == DB_FETCHMODE_DEFAULT) {
	    $fetchmode = $this->fetchmode;
	}
        if ($fetchmode & DB_FETCHMODE_ASSOC) {
            $row = @mssql_fetch_array($result);
        } else {
            $row = @mssql_fetch_row($result);
        }
        if (!$row) {
            return $this->raiseError();
        }
        return $row;
    }

    function fetchInto($result, &$ar, $fetchmode=DB_FETCHMODE_DEFAULT) {
	if ($fetchmode == DB_FETCHMODE_DEFAULT) {
	    $fetchmode = $this->fetchmode;
	}
        if ($fetchmode & DB_FETCHMODE_ASSOC) {
            $ar = @mssql_fetch_array($result);
        } else {
            $ar = @mssql_fetch_row($result);
        }
        if (!$ar) {
            return $this->raiseError();
        }
        return DB_OK;
    }

    function freeResult($result) {
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

    function numCols($result) {
        $cols = @mssql_num_fields($result);
        if (!$cols) {
            return $this->raiseError();
        }
        return $cols;
    }

    function prepare($query) {
        $tokens = split('[\&\?]', $query);
        $token = 0;
        $types = array();
        for ($i = 0; $i < strlen($query); $i++) {
            switch ($query[$i]) {
                case '?':
                    $types[$token++] = DB_PARAM_SCALAR;
                    break;
                case '&':
                    $types[$token++] = DB_PARAM_OPAQUE;
                    break;
            }
        }
        $this->prepare_tokens[] = &$tokens;
        end($this->prepare_tokens);
        $k = key($this->prepare_tokens);
        $this->prepare_types[$k] = $types;
        return $k;
    }

    function execute($stmt, $data = false) {
        $realquery = $this->execute_emulate_query($stmt, $data);
	$this->last_query = $realquery;
        $result = @mssql_query($realquery, $this->connection);
        if (!$result) {
            return $this->raiseError();
        }
        if ( preg_match('/(SELECT|SHOW|LIST|DESCRIBE)/i', $realquery) ) {
            return $result;
        } else {
            return DB_OK;
        }
    }

    function autoCommit($onoff=false) {
        return $this->raiseError(DB_ERROR_NOT_CAPABLE);
    }

    function commit() {
        return $this->raiseError(DB_ERROR_NOT_CAPABLE);
    }

    function rollback() {
        return $this->raiseError(DB_ERROR_NOT_CAPABLE);
    }
}

?>
