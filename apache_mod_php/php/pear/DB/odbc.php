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
// Database independent query interface definition for PHP's ODBC
// extension.
//

//
// XXX legend:
//
// XXX ERRORMSG: The error message from the odbc function should
//				 be registered here.
// XXX ADDREF:	 As soon as Zend/PHP gets support for returning
//				 references, this return value should be made into
//				 a reference.
//

require_once 'DB/common.php';

class DB_odbc extends DB_common {
    // {{{ properties

	var $connection;
	var $phptype, $dbsyntax;

    // }}}

    // {{{ constructor

	function DB_odbc() {
		$this->phptype = 'odbc';
		$this->dbsyntax = 'unknown';
		// Let's be very pessimistic about the features of an odbc
		// backend.  The user has to specify the dbsyntax to access
		// other features.
		$this->features = array(
			'prepare' => false,
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
	function connect(&$dsn, $user, $pw) {
		if (is_array($dsn)) {
			$dsninfo = &$dsn;
		} else {
			$dsninfo = DB::parseDSN($dsn);
		}
		if (!$dsninfo || !$dsninfo['phptype']) {
			return $this->raiseError(); // XXX ERRORMSG
		}
		$this->dbsyntax = $dsninfo['dbsyntax'];
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
		if ($this->provides('pconnect')) {
			$connect_function = $persistent ? 'odbc_pconnect' : 'odbc_connect';
		} else {
			$connect_function = 'odbc_connect';
		}
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
		$this->connection = $conn;
		return DB_OK;



		$this->connection = odbc_connect($dsn, $user, $pw);
	}

    // }}}
    // {{{ disconnect()

	function disconnect() {
		$err = odbc_close($this->connection); // XXX ERRORMSG
		return $err;
	}

    // }}}
    // {{{ query()

	function query($query) {
		$this->last_query = $query;
		$result = odbc_exec($this->connection, $query);
		if (!$result) {
			return $this->raiseError(); // XXX ERRORMSG
		}
		// Determine which queries that should return data, and which
		// should return an error code only.
		if (preg_match('/SELECT/i', $query)) {
			$resultObj = new DB_result($this, $result);
			return $resultObj; // XXX ADDREF
		} else {
			return DB_OK;
		}
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
	function simpleQuery($query) {
		$this->last_query = $query;
		$result = odbc_exec($this->connection, $query);
		if (!$result) {
			return $this->raiseError(); // XXX ERRORMSG
		}
		// Determine which queries that should return data, and which
		// should return an error code only.
		if (preg_match('/SELECT/i', $query)) {
			return $result;
		} else {
			return DB_OK;
		}
	}

	// }}}
    // {{{ fetchRow()

	function fetchRow($result) {
		$cols = odbc_fetch_into($result, &$row);
		if ($cols == 0) {
			// XXX ERRORMSG
			return false;
		}
		return $row; // XXX ADDREF
	}

	// }}}
    // {{{ freeResult()

	function freeResult($result) {
		$err = odbc_free_result($result); // XXX ERRORMSG
		return $err;
	}

	// }}}
    // {{{ quoteString()

	function quoteString($string) {
		return str_replace("'", "''", $string);
	}

	// }}}

    // prepare
    // execute
    // errorCode
    // errorMsg
    // errorNative
    // longReadlen
    // binMode
    // autoCommit
    // commit
    // rollback
}

// Local variables:
// tab-width: 4
// c-basic-offset: 4
// End:
?>
