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
// | Authors: Rui Hirokawa <louis@cityfujisawa.ne.jp>                     |
// |                                                                      |
// +----------------------------------------------------------------------+
//
// Database independent query interface definition for PHP's PostgreSQL
// extension.
//
// [SSB] Problems in this code:
//
// 1. $this->row is used as a row counter
// for the fetchRow() implementation.  There are two problems with this:
// First of all, $this->row is not reset when the result is freed.  Also,
// If the user creates two DB_result objects on the same DB connection,
// it will break completely because both result sets will be using the
// same row counter.  Solution: change $this->row into an array indexed
// by resource id.
// --> $this->row is changed to an indexed array. (R.Hirokawa)
//
// 2. The pgsql extension currently does not have error codes.  This
// should be fixed so DB_pgsql can do portable error codes.
// --> The error codes are not supported in the current pgsql API (libpq).
//
// 3. The transaction is supported by PostgreSQL, 
//    but the current implementation is not useful to use the transaction.


//
// XXX legend:
//
// XXX ERRORMSG: The error message from the pgsql function should
//				 be registered here.
//

require_once 'DB/common.php';

class DB_pgsql extends DB_common {
    // {{{ properties

	var $connection;
	var $phptype, $dbsyntax;
	var $prepare_tokens = array();
	var $prepare_types = array();
	var $numrows;
	var $row;

    // }}}

    // {{{ constructor

	function DB_pgsql() {
		$this->phptype = 'pgsql';
		$this->dbsyntax = 'pgsql';
		$this->features = array(
			'prepare' => false,
			'pconnect' => true,
			'transactions' => true
		);
		$this->errorcode_map = array();
		$this->numrows = array();
		$this->row = array();
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
		$protocol = $dsninfo['protocol'] ? $dsninfo['protocol'] : 'tcp';
		$user = $dsninfo['username'];
		$pw = $dsninfo['password'];
		$dbname = $dsninfo['database'];
		$options = $dsninfo['options'];
		$tty = $dsninfo['tty'];
		$port = $dsninfo['port'] ? $dsninfo['port'] : '5432';

		$connect_function = $persistent ? 'pg_pconnect' : 'pg_connect';

		if (($protocol == 'unix') && $dbname) {
			$connect_params = "dbname=$dbname";
			if ($user) {
				$connect_params .= " user=$user";
			}
			if ($pw) {
				$connect_params .= " password=$pw";
			}
			$conn = $connect_function($connect_params);
		} elseif ($dbhost && $user && $pw && $dbname) {
			$conn = $connect_function(
				"host=$dbhost port=$port dbname=$dbname user=$user password=$pw");
		} elseif ($dbhost && $dbname && $options && $tty) {
			$conn = $connect_function($dbhost, $port, $options, $tty, $dbname);
		} elseif ($dbhost && $dbname) {
			$conn = $connect_function($dbhost, $port, $dbname);
		} else {
			$conn = false;
		}
		if ($conn == false) {
			return $this->raiseError(); // XXX ERRORMSG
		}
		$this->connection = $conn;
		return DB_OK;
	}

    // }}}
    // {{{ disconnect()

	/**
	 * Log out and disconnect from the database.
	 *
	 * @return bool TRUE on success, FALSE if not connected.
	 */
	function disconnect() {
		return pg_close($this->connection); // XXX ERRORMSG
	}

    // }}}
    // {{{ query()


	/**
	 * Send a query to PostgreSQL and return the results as a DB_result object.
	 *
	 * @param $query the SQL query
	 *
	 * @return object a DB_result object on success, a DB error code
	 * on failure
	 */
	function &query($query) {
		$this->last_query = $query;
		$result = pg_exec($this->connection, $query);
		if (!$result) {
			return pg_errormessage($this->connection);
		}
		// Determine which queries that should return data, and which
		// should return an error code only.
		if (preg_match('/(SELECT|SHOW)/i', $query)) {
			$resultObj = new DB_result($this, $result);
			$this->row[$result] = 0; // reset the row counter. 
			$this->numrows[$result] = pg_numrows($result); 
			return $resultObj;
		} else {
			return DB_OK;
		}
	}

	// }}}
    // {{{ simpleQuery()

	/**
	 * Send a query to PostgreSQL and return the results as a PostgreSQL resource
	 * identifier.
	 *
	 * @param $query the SQL query
	 *
	 * @return int returns a valid PostgreSQL result for successful SELECT
	 * queries, DB_OK for other successful queries.  A DB error code
	 * is returned on failure.
	 */
	function simpleQuery($query) {
		$this->last_query = $query;
		$result = pg_exec($this->connection, $query);
		if (!$result) {
			return pg_errormessage($this->connection);
		}
		// Determine which queries that should return data, and which
		// should return an error code only.
		if (preg_match('/(SELECT|SHOW)/i', $query)) {
			$this->row[$result] = 0; // reset the row counter.
			$this->numrows[$result] = pg_numrows($result);  
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
	 * @param $result PostgreSQL result identifier
	 * @param $fetchmode how the resulting array should be indexed
	 *
	 * @return int an array on success, a DB error code on failure, NULL
	 *             if there is no more data
	 */
	function &fetchRow($result, $fetchmode = DB_FETCHMODE_DEFAULT) {
		if ($fetchmode == DB_FETCHMODE_DEFAULT) {
			$fetchmode = $this->fetchmode;
		}
		if ($this->row[$result]>=$this->numrows[$result]){
			return NULL;
		}
		if ($fetchmode & DB_FETCHMODE_ASSOC) {
			$row = pg_fetch_array($result, $this->row[$result]);
		} else {
			$row = pg_fetch_row($result, $this->row[$result]);
		}
		if (!$row) {
			$err = pg_errormessage($this->connection);
			if (!$err) {
				return NULL;
			}
			return $err;
		}
		$this->row[$result]++;
		return $row;
	}

	// }}}
    // {{{ fetchInto()

	/**
	 * Fetch a row and insert the data into an existing array.
	 *
	 * @param $result PostgreSQL result identifier
	 * @param $arr (reference) array where data from the row is stored
	 * @param $fetchmode how the array data should be indexed
	 *
	 * @return int DB_OK on success, a DB error code on failure
	 */
	function fetchInto($result, &$arr, $fetchmode = DB_FETCHMODE_DEFAULT) {
		if ($fetchmode == DB_FETCHMODE_DEFAULT) {
			$fetchmode = $this->fetchmode;
		}
		if ($this->row[$result]>=$this->numrows[$result]){
			return NULL;
		}
		if ($fetchmode & DB_FETCHMODE_ASSOC) {
			$arr = pg_fetch_array($result, $this->row[$result]);
		} else {
			$arr = pg_fetch_row($result, $this->row[$result]);
		}
		if (!$arr) {
			/* 
			 $errno = pg_errormessage($this->connection);
			 if (!$errno) {
				return NULL;
			 }
			 return $errno;
			*/
			// the error codes are not supported in pgsql. 
			return $this->raiseError(DB_ERROR_NOT_CAPABLE); 
		}
		$this->row[$result]++;
		return DB_OK;
	}

	// }}}
    // {{{ freeResult()

	/**
	 * Free the internal resources associated with $result.
	 *
	 * @param $result PostgreSQL result identifier or DB statement identifier
	 *
	 * @return bool TRUE on success, FALSE if $result is invalid
	 */
	function freeResult($result) {
		if (is_resource($result)) {
			return pg_freeresult($result);
		}
		if (!isset($this->prepare_tokens[$result])) {
			return false;
		}
		unset($this->prepare_tokens[$result]);
		unset($this->prepare_types[$result]);
		unset($this->row[$result]);
		unset($this->numrows[$result]);
		return true; 
	}

	// }}}
    // {{{ numCols()

	/**
	 * Get the number of columns in a result set.
	 *
	 * @param $result PostgreSQL result identifier
	 *
	 * @return int the number of columns per row in $result
	 */
	function numCols($result) {
		$cols = pg_numfields($result);
		if (!$cols) {
			return pg_errormessage($this->connection);
		}
		return $cols;
	}

    // }}}
    // {{{ numRows()

	/**
	 * Get the number of rows in a result set.
	 *
	 * @param $result PostgreSQL result identifier
	 *
	 * @return int the number of rows in $result
	 */
	function numRows($result) {
		$rows = pg_numrows($result);
		if (!$rows) {
			return pg_errormessage($this->connection);
		}
		return $rows;
	}

    // }}}
    // {{{ errorNative()

	/**
	 * Get the native error code of the last error (if any) that
	 * occured on the current connection.
	 *
	 * @return int native PostgreSQL error code
	 */
	function errorNative() {
	/*	return pg_errormessage($this->connection); */
		// the error codes are not supported in pgsql. 
		return $this->raiseError(); 
	}

    // }}}
    // {{{ prepare()

	/**
	 * Prepares a query for multiple execution with execute().  With
	 * PostgreSQL, this is emulated.
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
	 * @return int returns a PostgreSQL result resource for successful
	 * SELECT queries, DB_OK for other successful queries.  A DB error
	 * code is returned on failure.
	 */
	function execute($stmt, $data = false) {
		$realquery = $this->execute_emulate_query($stmt, $data);
		$this->last_query = $realquery;
		$result = pg_exec($this->connection, $realquery);
		if (!$result) {
			return pg_errormessage($this->connection);
		}
		if (preg_match('/(SELECT|SHOW)/i', $realquery)) {
			$this->row[$result] = 0; // reset the row counter.
			$this->numrows[$result] = pg_numrows($result);
			return $result;
		} else {
			return DB_OK;
		}
	}

    // }}}
    // {{{ autoCommit()

	/**
	 * Enable/disable automatic commits [not supported by PostgreSQL]
	 */
	function autoCommit($onoff = false) {
		return $this->raiseError(DB_ERROR_NOT_CAPABLE);
	}

    // }}}
    // {{{ commit()

	/**
	 * Commit transactions on the current connection
	 */
	function commit() {
		$result = pg_exec($this->connection, "end;");
		if (!$result) {
			return pg_errormessage($this->connection);
		}
		return DB_OK;
	}

    // }}}
    // {{{ rollback()

	/**
	 * Roll back all uncommitted transactions on the current connection.
	 */
	function rollback() {
		$result = pg_exec($this->connection, "abort;");
		if (!$result) {
			return pg_errormessage($this->connection);
		}
		return DB_OK;
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
