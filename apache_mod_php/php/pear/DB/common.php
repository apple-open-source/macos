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
// Base class for DB implementations.
//

/**
 * DB_common is a base class for DB implementations, and must be
 * inherited by all such.
 */

class DB_common extends PEAR
{
    // {{{ properties

    var $features;      // assoc of capabilities for this DB implementation
    var $errorcode_map; // assoc mapping native error codes to DB ones
    var $type;          // DB type (mysql, oci8, odbc etc.)
    var $prepare_tokens;
    var $prepare_types;
    var $prepare_maxstmt;
    var $last_query = '';
    var $fetchmode = DB_FETCHMODE_ORDERED;
    var $options = array(
        'persistent' => false,       // persistent connection?
        'optimize' => 'performance', // 'performance' or 'portability'
        'debug' => 0,                // numeric debug level
    );
    var $dbh;

    // }}}
    // {{{ toString()

    function toString()
    {
        $info = get_class($this);
        $info .=  ": (phptype=" . $this->phptype .
                  ", dbsyntax=" . $this->dbsyntax .
                  ")";

        if ($this->connection) {
            $info .= " [connected]";
        }

        return $info;
    }

    // }}}
    // {{{ constructor

    function DB_common()
    {
        $this->PEAR('DB_Error');
        $this->features = array();
        $this->errorcode_map = array();
        $this->fetchmode = DB_FETCHMODE_ORDERED;
    }

    // }}}
    // {{{ quoteString()

    /**
     * Quotes a string so it can be safely used within string delimiters
     * in a query.
     *
     * @param $string the input string to quote
     *
     * @return string the quoted string
     */

    function quoteString($string)
    {
        return str_replace("'", "\'", $string);
    }

    // }}}
    // {{{ provides()

    /**
     * Tell whether a DB implementation or its backend extension
     * supports a given feature.
     *
     * @param $feature name of the feature (see the DB class doc)
     *
     * @return bool whether this DB implementation supports $feature
     */

    function provides($feature)
    {
        return $this->features[$feature];
    }

    // }}}
    // {{{ errorCode()

    /**
     * Map native error codes to DB's portable ones.  Requires that
     * the DB implementation's constructor fills in the $errorcode_map
     * property.
     *
     * @param $nativecode the native error code, as returned by the backend
     * database extension (string or integer)
     *
     * @return int a portable DB error code, or FALSE if this DB
     * implementation has no mapping for the given error code.
     */

    function errorCode($nativecode)
    {
        if ($this->errorcode_map[$nativecode]) {
            return $this->errorcode_map[$nativecode];
        }

        //php_error(E_WARNING, get_class($this)."::errorCode: no mapping for $nativecode");
        // Fall back to DB_ERROR if there was no mapping.

        return DB_ERROR;
    }

    // }}}
    // {{{ errorMessage()

    /**
     * Map a DB error code to a textual message.  This is actually
     * just a wrapper for DB::errorMessage().
     *
     * @param $dbcode the DB error code
     *
     * @return string the corresponding error message, of FALSE
     * if the error code was unknown
     */

    function errorMessage($dbcode)
    {
        return DB::errorMessage($this->errorcode_map[$dbcode]);
    }

    // }}}
    // {{{ raiseError()

    /**
     * This method is called by DB to generate an error.
     *
     */

    function &raiseError($code = DB_ERROR, $mode = null, $options = null,
                         $userinfo = null, $nativecode = null)
    {
        if ($userinfo === null) {
            $userinfo = $this->last_query;
        }

        if ($nativecode) {
            $userinfo .= " [nativecode=$nativecode]";
        }

        return parent::raiseError(null, $code, $mode, $options, $userinfo,
                                  null, true);
    }

    // }}}
    // {{{ setFetchMode()

    /**
     * Sets which fetch mode should be used by default on queries
     * on this connection.
     *
     * @param $fetchmode int DB_FETCHMODE_ORDERED or
     *        DB_FETCHMODE_ASSOC, possibly bit-wise OR'ed with
     *        DB_FETCHMODE_FLIPPED.
     *
     * @see DB_FETCHMODE_ORDERED
     * @see DB_FETCHMODE_ASSOC
     * @see DB_FETCHMODE_FLIPPED
     */

    function setFetchMode($fetchmode)
    {
        switch ($fetchmode) {
            case DB_FETCHMODE_ORDERED:
            case DB_FETCHMODE_ASSOC:
                $this->fetchmode = $fetchmode;
                break;
            default:
                return $this->raiseError("invalid get mode");
        }
    }

    // }}}
    // {{{ setOption()

    function setOption($option, $value)
    {
        if (isset($this->options[$option])) {
            $this->options[$option] = $value;
            return DB_OK;
        }
        return $this->raiseError("unknown option $option");
    }

    // }}}
    // {{{ getOption()

    function getOption($option)
    {
        if (isset($this->options[$option])) {
            return $this->options[$option];
        }
        return $this->raiseError("unknown option $option");
    }

    // }}}
    // {{{ prepare()

    /**
     * Prepares a query for multiple execution with execute().  With
     * some database backends, this is emulated.
     */

    function prepare($query)
    {
        $tokens = split("[\&\?]", $query);
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

    // }}}
    // {{{ execute()

    function execute($stmt, $data = false)
    {
        $realquery = $this->executeEmulateQuery($stmt, $data);
        $result = $this->simpleQuery($realquery);
        if (DB::isError($result) || $result === DB_OK) {
            return $result;
        } else {
            return new DB_result($this, $result);
        }
    }

    // }}}
    // {{{ executeEmulateQuery()

    /**
     * @return a string containing the real query run when emulating
     * prepare/execute.  A DB error code is returned on failure.
     */

    function executeEmulateQuery($stmt, $data = false)
    {
        $p = &$this->prepare_tokens;
        $stmt = (int)$this->prepare_maxstmt++;

        if (!isset($this->prepare_tokens[$stmt]) ||
            !is_array($this->prepare_tokens[$stmt]) ||
            !sizeof($this->prepare_tokens[$stmt])) {
            return $this->raiseError(DB_ERROR_INVALID);
        }

        $qq = &$this->prepare_tokens[$stmt];
        $qp = sizeof($qq) - 1;

        if ((!$data && $qp > 0) ||
            (!is_array($data) && $qp > 1) ||
            (is_array($data) && $qp > sizeof($data))) {
            return $this->raiseError(DB_ERROR_NEED_MORE_DATA);
        }

        $realquery = $qq[0];

        for ($i = 0; $i < $qp; $i++) {
            if ($this->prepare_types[$stmt][$i] == DB_PARAM_OPAQUE) {
                if (is_array($data)) {
                    $fp = fopen($data[$i], "r");
                } else {
                    $fp = fopen($data, "r");
                }

                $pdata = "";

                if ($fp) {
                    while (($buf = fread($fp, 4096)) != false) {
                        $pdata .= $buf;
                    }
                }
            } else {
                if (is_array($data)) {
                    $pdata = &$data[$i];
                } else {
                    $pdata = &$data;
                }
            }

            $realquery .= "'" . $this->quoteString($pdata) . "'";
            $realquery .= $qq[$i + 1];
        }

        return $realquery;
    }

    // }}}
    // {{{ executeMultiple()

    /**
     * This function does several execute() calls on the same
     * statement handle.  $data must be an array indexed numerically
     * from 0, one execute call is done for every "row" in the array.
     *
     * If an error occurs during execute(), executeMultiple() does not
     * execute the unfinished rows, but rather returns that error.
     */

    function executeMultiple( $stmt, &$data )
    {
        for($i = 0; $i < sizeof( $data ); $i++) {
            $res = $this->execute($stmt, $data[$i]);
            if (DB::isError($res)) {
                return $res;
            }
        }
        return DB_OK;
    }

    // }}}
    // {{{ modifyQuery()

    /**
     * This method is used by backends to alter queries for various
     * reasons.  It is defined here to assure that all implementations
     * have this method defined.
     *
     * @access private
     *
     * @param query to modify
     *
     * @return the new (modified) query
     */
    function modifyQuery($query) {
        return $query;
    }

    // }}}
    // {{{ query()

    /**
     * Send a query to the database and return any results with a
     * DB_result object.
     *
     * @access public
     *
     * @param the SQL query
     *
     * @return object a DB_result object or DB_OK on success, a DB
     * error on failure
     *
     * @see DB::isError
     */
    function &query($query) {
        $result = $this->simpleQuery($query);
        if (DB::isError($result) || $result === DB_OK) {
            return $result;
        } else {
            return new DB_result($this, $result);
        }
    }

    // }}}
    // {{{ getOne()

    /**
     * Fetch the first column of the first row of data returned from
     * a query.  Takes care of doing the query and freeing the results
     * when finished.
     *
     * @param $query the SQL query
     * @param $params if supplied, prepare/execute will be used
     *        with this array as execute parameters
     * @access public
     */

    function &getOne($query, $params = array())
    {
        settype($params, "array");
        if (sizeof($params) > 0) {
            $sth = $this->prepare($query);
            if (DB::isError($sth)) {
                return $sth;
            }
            $res = $this->execute($sth, $params);
        } else {
            $res = $this->query($query);
        }

        if (DB::isError($res)) {
            return $res;
        }

        $err = $res->fetchInto($row, DB_FETCHMODE_ORDERED);
        if ($err !== DB_OK) {
            return $err;
        }

        $res->free();

        if (isset($sth)) {
            $this->freeResult($sth);
        }

        return $row[0];
    }

    // }}}
    // {{{ getRow()

    /**
     * Fetch the first row of data returned from a query.  Takes care
     * of doing the query and freeing the results when finished.
     *
     * @param $query the SQL query
     * @param $fetchmode the fetch mode to use
     * @param $params array if supplied, prepare/execute will be used
     *        with this array as execute parameters
     * @access public
     * @return array the first row of results as an array indexed from
     * 0, or a DB error code.
     */

    function &getRow($query, $fetchmode = DB_FETCHMODE_DEFAULT,
                     $params = array())
    {
        settype($params, "array");
        if (sizeof($params) > 0) {
            $sth = $this->prepare($query);
            if (DB::isError($sth)) {
                return $sth;
            }
            $res = $this->execute($sth, $params);
        } else {
            $res = $this->query($query);
        }

        if (DB::isError($res)) {
            return $res;
        }

        $err = $res->fetchInto($row, $fetchmode);

        if ($err !== DB_OK) {
            return $err;
        }
        $res->free();

        if (isset($sth)) {
            $this->freeResult($sth);
        }

        return $row;
    }

    // }}}
    // {{{ getCol()

    /**
     * Fetch a single column from a result set and return it as an
     * indexed array.
     *
     * @param $query the SQL query
     *
     * @param $col which column to return (integer [column number,
     * starting at 0] or string [column name])
     *
     * @param $params array if supplied, prepare/execute will be used
     *        with this array as execute parameters
     * @access public
     *
     * @return array an indexed array with the data from the first
     * row at index 0, or a DB error code.
     */

    function &getCol($query, $col = 0, $params = array())
    {
        settype($params, "array");
        if (sizeof($params) > 0) {
            $sth = $this->prepare($query);

            if (DB::isError($sth)) {
                return $sth;
            }

            $res = $this->execute($sth, $params);
        } else {
            $res = $this->query($query);
        }

        if (DB::isError($res)) {
            return $res;
        }

        $fetchmode = is_int($col) ? DB_FETCHMODE_ORDERED : DB_FETCHMODE_ASSOC;
        $ret = array();

        while (is_array($row = $res->fetchRow($fetchmode))) {
            $ret[] = $row[$col];
        }
        if (DB::isError($row)) {
            $ret = $row;
        }
        $res->free();

        if (isset($sth)) {
            $this->freeResult($sth);
        }

        return $ret;
    }

    // }}}
    // {{{ getAssoc()

    /**
     * Fetch the entire result set of a query and return it as an
     * associative array using the first column as the key.
     *
     * @param $query the SQL query
     *
     * @param $force_array (optional) used only when the query returns
     * exactly two columns.  If true, the values of the returned array
     * will be one-element arrays instead of scalars.
     *
     * @access public
     *
     * @return array associative array with results from the query.
     * If the result set contains more than two columns, the value
     * will be an array of the values from column 2-n.  If the result
     * set contains only two columns, the returned value will be a
     * scalar with the value of the second column (unless forced to an
     * array with the $force_array parameter).  A DB error code is
     * returned on errors.  If the result set contains fewer than two
     * columns, a DB_ERROR_TRUNCATED error is returned.
     *
     * For example, if the table "mytable" contains:
     *
     *  ID      TEXT       DATE
     * --------------------------------
     *  1       'one'      944679408
     *  2       'two'      944679408
     *  3       'three'    944679408
     *
     * Then the call getAssoc('SELECT id,text FROM mytable') returns:
     *   array(
     *     '1' => 'one',
     *     '2' => 'two',
     *     '3' => 'three',
     *  )
     *
     * ...while the call getAssoc('SELECT id,text,date FROM mytable') returns:
     *   array(
     *     '1' => array('one', '944679408'),
     *     '2' => array('two', '944679408'),
     *     '3' => array('three', '944679408')
     *  )
     *
     * Keep in mind that database functions in PHP usually return string
     * values for results regardless of the database's internal type.
     */

    function &getAssoc($query, $force_array = false, $params = array())
    {
        settype($params, "array");
        if (sizeof($params) > 0) {
            $sth = $this->prepare($query);

            if (DB::isError($sth)) {
                return $sth;
            }

            $res = $this->execute($sth, $params);
        } else {
            $res = $this->query($query);
        }

        if (DB::isError($res)) {
            return $res;
        }

        $cols = $res->numCols();

        if ($cols < 2) {
            return $this->raiseError(DB_ERROR_TRUNCATED);
        }

        $results = array();

        if ($cols > 2 || $force_array) {
            // return array values
            // XXX this part can be optimized
            while (is_array($row = $res->fetchRow(DB_FETCHMODE_ORDERED))) {
                reset($row);
                // we copy the row of data into a new array
                // to get indices running from 0 again
                $results[$row[0]] = array_slice($row, 1);
            }
            if (DB::isError($row)) {
                $results = $row;
            }
        } else {
            // return scalar values
            while (is_array($row = $res->fetchRow(DB_FETCHMODE_ORDERED))) {
                $results[$row[0]] = $row[1];
            }
            if (DB::isError($row)) {
                $results = $row;
            }
        }

        $res->free();

        if (isset($sth)) {
            $this->freeResult($sth);
        }

        return $results;
    }

    // }}}
    // {{{ getAll()

    /**
     * Fetch all the rows returned from a query.
     *
     * @param $query the SQL query
     * @access public
     * @return array an nested array, or a DB error
     */

    function &getAll($query, $fetchmode = DB_FETCHMODE_DEFAULT,
                     $params = array())
    {
        settype($params, "array");
        if (sizeof($params) > 0) {
            $sth = $this->prepare($query);

            if (DB::isError($sth)) {
                return $sth;
            }

            $res = $this->execute($sth, $params);
        } else {
            $res = $this->query($query);
        }

        if (DB::isError($res)) {
            return $res;
        }

        $results = array();

        while (is_array($row = $res->fetchRow($fetchmode))) {
            if ($fetchmode & DB_FETCHMODE_FLIPPED) {
                foreach ($row as $key => $val) {
                    $results[$key][] = $val;
                }
            } else {
                $results[] = $row;
            }
        }
        if (DB::isError($row)) {
            $results = $row;
        }
        $res->free();

        if (isset($sth)) {
            $this->freeResult($sth);
        }

        return $results;
    }

    // }}}
    // {{{ autoCommit()

    function autoCommit($onoff=false)
    {
        return $this->raiseError(DB_ERROR_NOT_CAPABLE);
    }

    // }}}
    // {{{ commit()

    function commit()
    {
        return $this->raiseError(DB_ERROR_NOT_CAPABLE);
    }

    // }}}
    // {{{ rollback()

    function rollback()
    {
        return $this->raiseError(DB_ERROR_NOT_CAPABLE);
    }

    // }}}
    // {{{ numRows()

    function numRows($result)
    {
        return $this->raiseError(DB_ERROR_NOT_CAPABLE);
    }

    // }}}
    // {{{ affectedRows()

    function affectedRows()
    {
        return $this->raiseError(DB_ERROR_NOT_CAPABLE);
    }

    // }}}
    // {{{ errorNative()

    function errorNative()
    {
        return $this->raiseError(DB_ERROR_NOT_CAPABLE);
    }

    // }}}
    // {{{ nextId()

    function nextId($seq_name, $ondemand = true)
    {
        return $this->raiseError(DB_ERROR_NOT_CAPABLE);
    }

    // }}}
    // {{{ createSequence()

    function createSequence($seq_name)
    {
        return $this->raiseError(DB_ERROR_NOT_CAPABLE);
    }

    // }}}
    // {{{ dropSequence()

    function dropSequence($seq_name)
    {
        return $this->raiseError(DB_ERROR_NOT_CAPABLE);
    }

    // }}}
    // {{{ tableInfo()

    function tableInfo($result, $mode = null)
    {
        return $this->raiseError(DB_ERROR_NOT_CAPABLE);
    }

    // }}}
}

?>
