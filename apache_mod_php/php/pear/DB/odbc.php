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
// | Authors: Stig Bakken <ssb@fast.no>                                   |
// |                                                                      |
// +----------------------------------------------------------------------+
//
// Database independent query interface definition for PHP's ODBC
// extension.
//

//
// XXX legend:
//
// XXX ERRORMSG: The error message from the odbc function should
//                 be registered here.
//
// TODO:
// - change fetchrow to use fetchInto
// - implement $rownum param in FetchInto (supported by odbc functions)
//

require_once 'DB/common.php';

class DB_odbc extends DB_common
{
    // {{{ properties

    var $connection;
    var $phptype, $dbsyntax;

    // }}}
    // {{{ constructor

    function DB_odbc()
    {
        $this->DB_common();
        $this->phptype = 'odbc';
        $this->dbsyntax = 'sql92';
        $this->features = array(
            'prepare' => true,
            'pconnect' => true,
            'transactions' => false
        );
        $this->errorcode_map = array(
            "01004" => DB_ERROR_TRUNCATED,
            "07001" => DB_ERROR_MISMATCH,
            "21S01" => DB_ERROR_MISMATCH,
            "21S02" => DB_ERROR_MISMATCH,
            "22003" => DB_ERROR_INVALID_NUMBER,
            "22008" => DB_ERROR_INVALID_DATE,
            "22012" => DB_ERROR_DIVZERO,
            "23000" => DB_ERROR_CONSTRAINT,
            "24000" => DB_ERROR_INVALID,
            "34000" => DB_ERROR_INVALID,
            "37000" => DB_ERROR_SYNTAX,
            "42000" => DB_ERROR_SYNTAX,
            "IM001" => DB_ERROR_UNSUPPORTED,
            "S0001" => DB_ERROR_NOT_FOUND,
            "S0002" => DB_ERROR_NOT_FOUND,
            "S0011" => DB_ERROR_ALREADY_EXISTS,
            "S0012" => DB_ERROR_NOT_FOUND,
            "S0021" => DB_ERROR_ALREADY_EXISTS,
            "S0022" => DB_ERROR_NOT_FOUND,
            "S1009" => DB_ERROR_INVALID,
            "S1090" => DB_ERROR_INVALID,
            "S1C00" => DB_ERROR_NOT_CAPABLE
        );
    }

    // }}}
    // {{{ connect()

    /**
     * Connect to a database and log in as the specified user.
     *
     * @param $dsn the data source name (see DB::parseDSN for syntax)
     * @param $persistent (optional) whether the connection should
     *        be persistent
     *
     * @return int DB_OK on success, a DB error code on failure
     */
    function connect($dsninfo, $persistent = false)
    {
        $this->dsn = $dsninfo;
        if (!empty($dsninfo['dbsyntax'])) {
            $this->dbsyntax = $dsninfo['dbsyntax'];
        }
        switch ($this->dbsyntax) {
            case 'solid':
                $this->features = array(
                    'prepare' => true,
                    'pconnect' => true,
                    'transactions' => true
                );
                $default_dsn = 'localhost';
                break;
            default:
                break;
        }
        $dbhost = $dsninfo['hostspec'] ? $dsninfo['hostspec'] : 'localhost';
        $user = $dsninfo['username'];
        $pw = $dsninfo['password'];
        DB::assertExtension("odbc");
        if ($this->provides('pconnect')) {
            $connect_function = $persistent ? 'odbc_pconnect' : 'odbc_connect';
        } else {
            $connect_function = 'odbc_connect';
        }
        $conn = @$connect_function($dbhost, $user, $pw);
        if (!is_resource($conn)) {
            return $this->raiseError(DB_ERROR_CONNECT_FAILED);
        }
        $this->connection = $conn;
        return DB_OK;
    }

    // }}}
    // {{{ disconnect()

    function disconnect()
    {
        $err = odbc_close($this->connection); // XXX ERRORMSG
        return $err;
    }

    // }}}
    // {{{ simpleQuery()

    /**
     * Send a query to ODBC and return the results as a ODBC resource
     * identifier.
     *
     * @param $query the SQL query
     *
     * @return int returns a valid ODBC result for successful SELECT
     * queries, DB_OK for other successful queries.  A DB error code
     * is returned on failure.
     */
    function simpleQuery($query)
    {
        $this->last_query = $query;
        $query = $this->modifyQuery($query);
        $result = odbc_exec($this->connection, $query);
        if (!$result) {
            return $this->raiseError(); // XXX ERRORMSG
        }
        // Determine which queries that should return data, and which
        // should return an error code only.
        return DB::isManip($query) ? DB_OK : $result;
    }

    // }}}
    // {{{ fetchRow()

    /**
     * Fetch a row and return as array.
     *
     * @param $result result identifier
     * @param $fetchmode how the resulting array should be indexed
     *
     * @return mixed an array on success (associative or ordred, depending on
     *               fetchmode), a false on failure, false if there is no more
     *               data
     */
    function fetchRow($result, $fetchmode = DB_FETCHMODE_DEFAULT)
    {
        if ($fetchmode == DB_FETCHMODE_DEFAULT) {
            $fetchmode = $this->fetchmode;
        }
        $cols = odbc_fetch_into($result, &$row);
        if (!$cols) {
            if ($errno = odbc_error($this->connection)) {
                return $this->raiseError($errno);
            } else {
                return null;
            }
        }
        if ($fetchmode == DB_FETCHMODE_ORDERED) {
            return $row;
        } elseif ($fetchmode == DB_FETCHMODE_ASSOC) {
            for ($i = 0; $i < count($row); $i++) {
                $colName = odbc_field_name($result, $i+1);
                $a[$colName] = $row[$i];
            }
            return $a;
        } else {
            return $this->raiseError(); // XXX ERRORMSG
        }
    }

    function fetchInto($result, &$row, $fetchmode, $rownum=null)
    {
        if ($rownum !== null) {
            return $this->raiseError(DB_ERROR_UNSUPPORTED);
        }
        if (is_array($row = $this->fetchRow($result, $fetchmode))) {
            return DB_OK;
        }
        return $row;
    }

    // }}}
    // {{{ freeResult()

    function freeResult($result)
    {
        $err = odbc_free_result($result); // XXX ERRORMSG
        return $err;
    }

    // }}}
    // {{{ quoteString()

    function quoteString($string)
    {
        return str_replace("'", "''", $string);
    }

    // }}}
    // {{{ numCols()

    function numCols($result)
    {
        $cols = @odbc_num_fields($result);
        if (!$cols) {
            return $this->raiseError($php_errormsg);
        }
        return $cols;
    }

    // }}}
    // {{{ numRows()

    /**
     * ODBC does not support counting rows in the result set of
     * SELECTs.
     *
     * @param $result the odbc result resource
     * @return a DB error
     */
    function numRows($result)
    {
        return $this->raiseError(DB_ERROR_NOT_CAPABLE);
    }

    // }}}
    // {{{ errorNative()

    /**
     * Get the native error code of the last error (if any) that
     * occured on the current connection.
     *
     * @access public
     *
     * @return int ODBC error code
     */

    function errorNative()
    {
        if (is_resource($this->connection)) {
            return odbc_error($this->connection);
        } else {
            return odbc_error();
        }
    }

    // }}}
    // {{{ autoCommit()

    function autoCommit($onoff = false)
    {
        if (!@odbc_autocommit($this->connection, $onoff)) {
            return $this->raiseError($php_errormsg);
        }
        return DB_OK;
    }

    // }}}
    // {{{ commit()

    function commit()
    {
        if (!@odbc_commit($this->connection)) {
            return $this->raiseError($php_errormsg);
        }
        return DB_OK;
    }

    // }}}
    // {{{ rollback()

    function rollback()
    {
        if (!@odbc_commit($this->connection)) {
            return $this->raiseError($php_errormsg);
        }
        return DB_OK;
    }

    // }}}
    // {{{ odbcRaiseError()

    function odbcRaiseError($errno = null)
    {
        if (is_resource($this->connection)) {
            $message = odbc_errormsg($this->connection);
            $code = odbc_error($this->connection);
        } else {
            $message = odbc_errormsg();
            $code = odbc_error();
        }
        if ($errno === null) {
            return $this->raiseError($this->errorCode($code));
        }
        return $this->raiseError($this->errorCode($errno));
    }

    // }}}
}

// Local variables:
// tab-width: 4
// c-basic-offset: 4
// End:
?>
