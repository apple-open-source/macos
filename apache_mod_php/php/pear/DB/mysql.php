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
// | Authors: Stig Bakken <ssb@fast.no>                                   |
// |                                                                      |
// +----------------------------------------------------------------------+
//
// Database independent query interface definition for PHP's MySQL
// extension.
//

//
// XXX legend:
//
// XXX ERRORMSG: The error message from the mysql function should
//				 be registered here.
//

require_once 'DB/common.php';

class DB_mysql extends DB_common {
    // {{{ properties

	var $connection;
	var $phptype, $dbsyntax;
	var $prepare_tokens = array();
	var $prepare_types = array();

    // }}}

    // {{{ constructor

	/**
	 * DB_mysql constructor.
	 *
	 * @access public
	 */
	function DB_mysql() {
		$this->phptype = 'mysql';
		$this->dbsyntax = 'mysql';
		$this->features = array(
			'prepare' => false,
			'pconnect' => true,
			'transactions' => false
		);
		$this->errorcode_map = array(
			1004 => DB_ERROR_CANNOT_CREATE,
			1005 => DB_ERROR_CANNOT_CREATE,
			1006 => DB_ERROR_CANNOT_CREATE,
			1007 => DB_ERROR_ALREADY_EXISTS,
			1008 => DB_ERROR_CANNOT_DROP,
			1046 => DB_ERROR_NODBSELECTED,
			1146 => DB_ERROR_NOSUCHTABLE,
			1064 => DB_ERROR_SYNTAX,
			1054 => DB_ERROR_NOSUCHFIELD,
			1062 => DB_ERROR_ALREADY_EXISTS,
			1051 => DB_ERROR_NOSUCHTABLE,
			1100 => DB_ERROR_NOT_LOCKED,
			1136 => DB_ERROR_VALUE_COUNT_ON_ROW,
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
	 * @access public
	 * @return int DB_OK on success, a DB error on failure
	 */
	function connect(&$dsn, $persistent = false) {
		if (is_array($dsn)) {
			$dsninfo = &$dsn;
		} else {
			$dsninfo = DB::parseDSN($dsn);
		}
		if (!$dsninfo || !$dsninfo['phptype']) {
			return $this->raiseError(); // XXX ERRORMSG
		}
		$dbhost = $dsninfo['hostspec'] ? $dsninfo['hostspec'] : 'localhost';
		$user = $dsninfo['username'];
		$pw = $dsninfo['password'];
		$connect_function = $persistent ? 'mysql_pconnect' : 'mysql_connect';
		if ($dbhost && $user && $pw) {
			$conn = $connect_function($dbhost, $user, $pw);
		} elseif ($dbhost && $user) {
			$conn = $connect_function($dbhost, $user);
		} elseif ($dbhost) {
			$conn = $connect_function($dbhost);
		} else {
			$conn = false;
		}
		if ($conn == false) {
			return $this->raiseError(); // XXX ERRORMSG
		}
		if ($dsninfo['database']) {
			if (!mysql_select_db($dsninfo['database'], $conn)) {
				return $this->raiseError(); // XXX ERRORMSG
			}
		}
		$this->connection = $conn;
		return DB_OK;
	}

    // }}}
    // {{{ disconnect()

	/**
	 * Log out and disconnect from the database.
	 *
	 * @access public
	 *
	 * @return bool TRUE on success, FALSE if not connected.
	 */
	function disconnect() {
		return mysql_close($this->connection); // XXX ERRORMSG
	}

    // }}}
    // {{{ query()


	/**
	 * Send a query to MySQL and return the results as a DB_result object.
	 *
	 * @param $query the SQL query
	 *
	 * @access public
	 *
	 * @return object a DB_result object on success, a DB error
	 * on failure
	 */
	function &query($query) {
		$this->last_query = $query;
		$result = @mysql_query($query, $this->connection);
		if (!$result) {
			return $this->raiseError($this->errorCode(mysql_errno($this->connection)));
		}
		// Determine which queries that should return data, and which
		// should return an error code only.
		if (preg_match('/(SELECT|SHOW)/i', $query)) {
			$resultObj = new DB_result($this, $result);
			return $resultObj;
		} else {
			return DB_OK;
		}
	}

	// }}}
    // {{{ simpleQuery()

	/**
	 * Send a query to MySQL and return the results as a MySQL resource
	 * identifier.
	 *
	 * @param $query the SQL query
	 *
	 * @access public
	 *
	 * @return int returns a valid MySQL result for successful SELECT
	 * queries, DB_OK for other successful queries.  A DB error is
	 * returned on failure.
	 */
	function simpleQuery($query) {
		$this->last_query = $query;
		$result = mysql_query($query, $this->connection);
		if (!$result) {
			return $this->raiseError($this->errorCode(mysql_errno($this->connection)));
		}
		// Determine which queries that should return data, and which
		// should return an error code only.
		if (preg_match('/(SELECT|SHOW)/i', $query)) {
			return $result;
		} else {
			return DB_OK;
		}
	}

	// }}}
    // {{{ fetchRow()

	/**
	 * Fetch a row and return as array.
	 *
	 * @param $result MySQL result identifier
	 * @param $fetchmode how the resulting array should be indexed
	 *
	 * @access public
	 *
	 * @return mixed an array on success, a DB error on failure, NULL
	 *               if there is no more data
	 */
	function &fetchRow($result, $fetchmode = DB_FETCHMODE_DEFAULT) {
		if ($fetchmode == DB_FETCHMODE_DEFAULT) {
			$fetchmode = $this->fetchmode;
		}
		if ($fetchmode & DB_FETCHMODE_ASSOC) {
			$row = mysql_fetch_array($result, MYSQL_ASSOC);
		} else {
			$row = mysql_fetch_row($result);
		}
		if (!$row) {
			$errno = mysql_errno($this->connection);
			if (!$errno) {
				return NULL;
			}
			return $this->raiseError($this->errorCode($errno));
		}
		return $row;
	}

	// }}}
    // {{{ fetchInto()

	/**
	 * Fetch a row and insert the data into an existing array.
	 *
	 * @param $result MySQL result identifier
	 * @param $arr (reference) array where data from the row is stored
	 * @param $fetchmode how the array data should be indexed
	 *
	 * @access public
	 *
	 * @return int DB_OK on success, a DB error on failure
	 */
	function fetchInto($result, &$arr, $fetchmode = DB_FETCHMODE_DEFAULT) {
		if ($fetchmode == DB_FETCHMODE_DEFAULT) {
			$fetchmode = $this->fetchmode;
		}
		if ($fetchmode & DB_FETCHMODE_ASSOC) {
			$arr = mysql_fetch_array($result, MYSQL_ASSOC);
		} else {
			$arr = mysql_fetch_row($result);
		}
		if (!$arr) {
			$errno = mysql_errno($this->connection);
			if (!$errno) {
				return NULL;
			}
			return $this->raiseError($this->errorCode($errno));
		}
		return DB_OK;
	}

	// }}}
    // {{{ freeResult()

	/**
	 * Free the internal resources associated with $result.
	 *
	 * @param $result MySQL result identifier or DB statement identifier
	 *
	 * @access public
	 *
	 * @return bool TRUE on success, FALSE if $result is invalid
	 */
	function freeResult($result) {
		if (is_resource($result)) {
			return mysql_free_result($result);
		}
		if (!isset($this->prepare_tokens[$result])) {
			return false;
		}
		unset($this->prepare_tokens[$result]);
		unset($this->prepare_types[$result]);
		return true; 
	}

	// }}}
    // {{{ numCols()

	/**
	 * Get the number of columns in a result set.
	 *
	 * @param $result MySQL result identifier
	 *
	 * @access public
	 *
	 * @return int the number of columns per row in $result
	 */
	function numCols($result) {
		$cols = mysql_num_fields($result);
		if (!$cols) {
			return $this->raiseError($this->errorCode(mysql_errno($this->connection)));
		}
		return $cols;
	}

    // }}}
    // {{{ errorNative()

	/**
	 * Get the native error code of the last error (if any) that
	 * occured on the current connection.
	 *
	 * @access public
	 *
	 * @return int native MySQL error code
	 */
	function errorNative() {
		return mysql_errno($this->connection);
	}

    // }}}
    // {{{ prepare()

	/**
	 * Prepares a query for multiple execution with execute().  With
	 * MySQL, this is emulated.
	 *
	 * @param $query the SQL query to prepare for execution.  The
	 * characters "?" and "&" have special meanings. "?" is a scalar
	 * placeholder, the value provided for it when execute()ing is
	 * inserted as-is.  "&" is an opaque placeholder, the value
	 * provided on execute() is a file name, and the contents of that
	 * file will be used in the query.
	 *
	 * @access public
	 *
	 * @return int identifier for this prepared query
	 */
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

    // }}}
    // {{{ execute()

	/**
	 * Executes a prepared query and substitutes placeholders with
	 * provided values.
	 *
	 * @return int returns a MySQL result resource for successful
	 * SELECT queries, DB_OK for other successful queries.  A DB error
	 * is returned on failure.
	 *
	 * @see DB_mysql::prepare
	 */
	function execute($stmt, $data = false) {
		$realquery = $this->execute_emulate_query($stmt, $data);
		$this->last_query = $realquery;
		$result = mysql_query($realquery, $this->connection);
		if (!$result) {
			return $this->raiseError($this->errorCode(mysql_errno($this->connection)));
		}
		if (preg_match('/(SELECT|SHOW)/i', $realquery)) {
			return $result;
		} else {
			return DB_OK;
		}
	}

    // }}}
    // {{{ autoCommit()

	/**
	 * Enable/disable automatic commits [not supported by MySQL]
	 *
	 * @access public
	 *
	 * @return object DB error code (not capable)
	 */
	function autoCommit($onoff = false) {
		return $this->raiseError(DB_ERROR_NOT_CAPABLE);
	}

    // }}}
    // {{{ commit()

	/**
	 * Commit transactions on the current connection [not supported by MySQL]
	 *
	 * @access public
	 *
	 * @return object DB error code (not capable)
	 */
	function commit() {
		return $this->raiseError(DB_ERROR_NOT_CAPABLE);
	}

    // }}}
    // {{{ rollback()

	/**
	 * Roll back all uncommitted transactions on the current connection.
	 * [not supported by MySQL]
	 *
	 * @access public
	 *
	 * @return object DB error code (not capable)
	 */
	function rollback() {
		return $this->raiseError(DB_ERROR_NOT_CAPABLE);
	}

    // }}}

	// TODO/wishlist:
	// simpleFetch
	// simpleGet
	// affectedRows
    // longReadlen
    // binmode
}

// Local variables:
// tab-width: 4
// c-basic-offset: 4
// End:
?>
