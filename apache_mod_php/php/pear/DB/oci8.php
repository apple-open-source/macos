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
// | Authors: James L. Pine <jlp@valinux.com>                             |
// |                                                                      |
// +----------------------------------------------------------------------+
//
// Database independent query interface definition for PHP's Oracle 8
// call-interface extension.
//

//
// be aware...  OCIError() only appears to return anything when given a
// statement, so functions return the generic DB_ERROR instead of more
// useful errors that have to do with feedback from the database.
//


require_once 'DB/common.php';

class DB_oci8 extends DB_common {
    // {{{ properties

	var $connection;
	var $phptype, $dbsyntax;
	var $select_query = array();
	var $prepare_types = array();
	var $autoCommit = 1;
	var $last_stmt = false;

    // }}}

    // {{{ constructor

	function DB_oci8() {
		$this->phptype = 'oci8';
		$this->dbsyntax = 'oci8';
		$this->features = array(
			'prepare' => false,
			'pconnect' => true,
			'transactions' => true
		);
		$this->errorcode_map = array();
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
			return $this->raiseError();
		}
		$user = $dsninfo['username'];
		$pw = $dsninfo['password'];
		$hostspec = $dsninfo['hostspec'];

		$connect_function = $persistent ? 'OCIPLogon' : 'OCILogon';
		if ($user && $pw && $hostspec) {
			$conn = $connect_function($user,$pw,$hostspec);
		} elseif ($user && $pw) {
			$conn = $connect_function($user,$pw);
		} else {
			$conn = false;
		}
		if ($conn == false) {
			return $this->raiseError();
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
		return OCILogOff($this->connection);
	}

    // }}}
    // {{{ query()


	/**
	 * Send a query to the database and return the results as a DB_result object.
	 *
	 * @param $query the SQL query
	 *
	 * @return object a DB_result object on success, a DB error code
	 * on failure
	 */
	function &query($query) {
		$this->last_query = $query;
		$result = OCIParse($this->connection, $query);
		if (!$result) {
			return $this->raiseError();
		}
		if ($this->autoCommit) {
			$success=OCIExecute($result,OCI_COMMIT_ON_SUCCESS);
		}
		else {
			$success=OCIExecute($result,OCI_DEFAULT);
		}
		if (!$success) {
			return $this->raiseError();
		}
		$this->last_stmt=$result;
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
	 * Send a query to oracle and return the results as an oci8 resource
	 * identifier.
	 *
	 * @param $query the SQL query
	 *
	 * @return int returns a valid oci8 result for successful SELECT
	 * queries, DB_OK for other successful queries.  A DB error code
	 * is returned on failure.
	 */
	function simpleQuery($query) {
		$this->last_query = $query;
		$result = OCIParse($this->connection, $query);
		if (!$result) {
			return $this->raiseError();
		}
		if ($this->autoCommit) {
			$success=OCIExecute($result,OCI_COMMIT_ON_SUCCESS);
		}
		else {
			$success=OCIExecute($result,OCI_DEFAULT);
		}
		if (!$success) {
			return $this->raiseError();
		}
		$this->last_stmt=$result;
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
	 * @param $result oci8 result identifier
	 * @param $fetchmode how the resulting array should be indexed
	 *
	 * @return int an array on success, a DB error code on failure, NULL
	 *             if there is no more data
	 */
	function &fetchRow($result, $fetchmode = DB_FETCHMODE_DEFAULT) {
		if ($fetchmode == DB_FETCHMODE_DEFAULT) {
			$fetchmode = $this->fetchmode;
		}
		if ($fetchmode & DB_FETCHMODE_ASSOC) {
			$moredata=OCIFetchInto($result,$row,OCI_ASSOC+OCI_RETURN_NULLS+OCI_RETURN_LOBS);
		} else {
			$moredata=OCIFetchInto($result,$row,OCI_RETURN_NULLS+OCI_RETURN_LOBS);
		}
		if (!$row) {
			return $this->raiseError();
		}
		if ($moredata==NULL) {
			return NULL;
		}
		return $row;
	}

	// }}}
    // {{{ fetchInto()

	/**
	 * Fetch a row and insert the data into an existing array.
	 *
	 * @param $result oci8 result identifier
	 * @param $arr (reference) array where data from the row is stored
	 * @param $fetchmode how the array data should be indexed
	 *
	 * @return int DB_OK on success, a DB error code on failure
	 */
	function fetchInto($result, &$arr, $fetchmode = DB_FETCHMODE_DEFAULT) {
		if ($fetchmode == DB_FETCHMODE_DEFAULT) {
			$fetchmode = $this->fetchmode;
		}
		if ($fetchmode & DB_FETCHMODE_ASSOC) {
			$moredata=OCIFetchInto($result,$arr,OCI_ASSOC+OCI_RETURN_NULLS+OCI_RETURN_LOBS);
		} else {
			$moredata=OCIFetchInto($result,$arr,OCI_RETURN_NULLS+OCI_RETURN_LOBS);
		}
		if (!($arr && $moredata)) {
			return $this->raiseError();
		}
		return DB_OK;
	}

	// }}}
    // {{{ freeResult()

	/**
	 * Free the internal resources associated with $result.
	 *
	 * @param $result oci8 result identifier or DB statement identifier
	 *
	 * @return bool TRUE on success, FALSE if $result is invalid
	 */
	function freeResult($result) {
		if (is_resource($result)) {
			return OCIFreeStatement($result);
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
	 * @param $result oci8 result identifier
	 *
	 * @return int the number of columns per row in $result
	 */
	function numCols($result) {
		$cols = OCINumCols($result);
		if (!$cols) {
			return $this->raiseError();
		}
		return $cols;
	}

    // }}}
    // {{{ errorNative()

	/**
	 * Get the native error code of the last error (if any) that occured
	 * on the current connection.  This does not work, as OCIError does
	 * not work unless given a statement.  If OCIError does return
	 * something, so will this.
	 *
	 * @return int native oci8 error code
	 */
	function errorNative() {
		$error=OCIError($this->connection);
		if (is_array($error)) {
			return $error['code'];
		}
		return false;
	}

    // }}}
    // {{{ prepare()

	/**
	 * Prepares a query for multiple execution with execute().  With
	 * oci8, this is emulated.
	 * @param $query query to be prepared
	 *
	 * @return DB statement resource
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
		$binds=sizeof($tokens)-1;
		for ($i=0;$i<$binds;$i++) {
			$newquery.=$tokens[$i].":bind".$i;
		}
		$newquery.=$tokens[$i];
		$this->last_query = $query;
		$stmt=OCIParse($this->connection,$newquery);
		$this->prepare_types[$stmt] = $types;
		$this->select_query[$stmt] = preg_match('/(SELECT|SHOW)/i', $newquery);
		return $stmt;
	}

    // }}}
    // {{{ execute()

	/**
	 * Executes a DB statement prepared with prepare().
	 *
	 * @param $stmt a DB statement resource (returned from prepare())
	 * @param $data data to be used in execution of the statement
	 *
	 * @return int returns an oci8 result resource for successful
	 * SELECT queries, DB_OK for other successful queries.  A DB error
	 * code is returned on failure.
	 */
	function execute($stmt, $data = false) {
		$types=&$this->prepare_types[$stmt];
		if (($size=sizeof($types))!=sizeof($data)) {
			return $this->raiseError();
		}
		for ($i=0;$i<$size;$i++) {
			if (is_array($data)) {
				$pdata[$i]=$data[$i];
			}
			else {
				$pdata[$i]=$data;
			}
			if ($types[$i]==DB_PARAM_OPAQUE) {
				$fp = fopen($pdata[$i], "r");
				$pdata = '';
				if ($fp) {
					while (($buf = fread($fp, 4096)) != false) {
						$pdata[$i] .= $buf;
					}
				}
			}
			if (!OCIBindByName($stmt,":bind".$i,$pdata[$i],-1)) {
				return $this->raiseError();
			}
		}
		if ($this->autoCommit) {
			$success=OCIExecute($stmt,OCI_COMMIT_ON_SUCCESS);
		}
		else {
			$success=OCIExecute($stmt,OCI_DEFAULT);
		}
		if (!$success) {
			return $this->raiseError();
		}
		$this->last_stmt=$stmt;
		if ($this->select_query[$stmt]) {
			return $stmt;
		}
		else {
			return $DB_OK;
		}
	}

    // }}}
    // {{{ autoCommit()

	/**
	 * Enable/disable automatic commits
	 * 
	 * @param $onoff true/false whether to autocommit
	 */
	function autoCommit($onoff = false) {
		if (!$onoff) {
			$this->autoCommit=0;
		}
		else {
			$this->autoCommit=1;
		}
		return DB_OK;
	}

    // }}}
    // {{{ commit()

	/**
	 * Commit transactions on the current connection
	 *
	 * @return DB_ERROR or DB_OK
	 */
	function commit() {
		$result = OCICommit($this->connection);
		if (!$result) {
			return $this->raiseError();
		}
		return DB_OK;
	}

    // }}}
    // {{{ rollback()

	/**
	 * Roll back all uncommitted transactions on the current connection.
	 *
	 * @return DB_ERROR or DB_OK
	 */
	function rollback() {
		$result = OCIRollback($this->connection);
		if (!$result) {
			return $this->raiseError();
		}
		return DB_OK;
	}

    // }}}
    // {{{ affectedRows()

	/**
	 * Gets the number of rows affected by the last query.
	 * if the last query was a select, returns 0.
	 *
	 * @return number of rows affected by the last query or DB_ERROR
	 */
	function affectedRows() {
		if ($this->last_stmt === false) {
			return $this->raiseError();
		}
		$result = OCIRowCount($this->last_stmt);
		if ($result === false) {
 			return $this->raiseError();
		}
		return $result;
	}

    // }}}

}

// Local variables:
// tab-width: 4
// c-basic-offset: 4
// End:
?>
