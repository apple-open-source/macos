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
// DB_storage: a class that lets you return SQL data as objects that
// can be manipulated and that updates the database accordingly.
//

require_once "DB.php";

function DB_storage_destructor() {
    global $DB_storage_object_list;

    if (is_array($DB_storage_object_list)) {
	reset($DB_storage_object_list);
	while (list($ind, $obj) = each($DB_storage_object_list)) {
	    $obj->destroy();
	}
	$DB_storage_object_list = null;
    }
}

class DB_storage {
    /** the name of the table (or view, if the backend database supports
        updates in views) we hold data from */
    var $_table = null;
    /** which column in the table contains primary keys */
    var $_keycolumn = null;
    /** DB connection handle used for all transactions */
    var $_dbh = null;
    /** an assoc with the names of database fields stored as properties
	in this object */
    var $_properties = array();
    /** an assoc with the names of the properties in this object that
	have been changed since they were fetched from the database */
    var $_changes = array();
    /** flag that decides if data in this object can be changed.
	objects that don't have their table's key column in their
	property lists will be flagged as read-only. */
    var $_readonly = false;

    /**
     * Constructor, adds itself to the DB_storage class's list of
     * objects that should have their "destroy" method called when
     * PHP shuts down (poor man's destructors).
     */
    function DB_storage($table, $keycolumn, &$dbh) {
	global $DB_storage_object_list;
	if (is_array($DB_storage_object_list)) {
	    $DB_storage_object_list[] = &$this;
	} else {
	    $DB_storage_object_list = array(&$this);
	}
	$this->_table = $table;
	$this->_keycolumn = $keycolumn;
	$this->_dbh = $dbh;
	$this->_readonly = false;
    }

    /**
     * Method used to initialize a DB_storage object from the
     * configured table.
     * @param $keyval the key of the row to fetch
     * @return int DB_OK on success, DB error if not
     */
    function setup($keyval) {
	if (is_int($keyval)) {
	    $qval = "$keyval";
	} else {
	    $qval = "'" . $this->_dbh->quoteString($keyval) . "'";
	}
	$sth = $this->_dbh->query("SELECT * FROM " .
				  $this->_table . " WHERE " .
				  $this->_keycolumn . " = $qval");
	if (DB::isError($sth)) {
	    return $sth;
	}
	while ($row = $sth->fetchRow(DB_FETCHMODE_ASSOC)) {
	    reset($row);
	    while (list($key, $value) = each($row)) {
		$this->_properties[$key] = true;
		$this->$key = &$value;
		unset($value);
	    }
	}
    }

    /**
     * Create a new (empty) row in the configured table for this
     * object.
     */
    function insert($newid = false) {
	if (is_int($newid)) {
	    $qid = "$newid";
	} else {
	    $qid = "'" . $this->_dbh->quoteString($newid) . "'";
	}
	$sth = $this->_dbh->query("INSERT INTO " .
				  $this->_table . " (" .
				  $this->_keycolumn .
				  ") VALUES($qid)");
	if (DB::isError($sth)) {
	    return $sth;
	}
	$this->setup($newid);
    }

    /**
     * Output a simple description of this DB_storage object.
     * @return string object description
     */
    function toString() {
	$info = get_class(&$this);
	$info .= " (table=";
	$info .= $this->_table;
	$info .= ", keycolumn=";
	$info .= $this->_keycolumn;
	$info .= ", dbh=";
	if (is_object($this->_dbh)) {
	    $info .= $this->_dbh->toString();
	} else {
	    $info .= "null";
	}
	$info .= ")";
	if (sizeof($this->_properties)) {
	    $keyname = $this->_keycolumn;
	    $key = $this->$keyname;
	    $info .= " [loaded, key=$key]";
	}
	if (sizeof($this->_changes)) {
	    $info .= " [modified]";
	}
	return $info;
    }

    /**
     * Dump the contents of this object to "standard output".
     */
    function dump() {
	reset($this->_properties);
	while (list($prop, $foo) = each($this->_properties)) {
	    print "$prop = ";
	    print htmlentities($this->$prop);
	    print "<BR>\n";
	}
    }

    /**
     * Static method used to create new DB storage objects.
     * @param $data assoc. array where the keys are the names
     *              of properties/columns
     * @return object a new instance of DB_storage or a subclass of it
     */
    function &create($table, &$data) {
	$classname = get_class(&$this);
	$obj = new $classname($table);
	reset($data);
	while (list($name, $value) = each($data)) {
	    $obj->_properties[$name] = true;
	    $obj->$name = &$value;
	}
	return $obj;
    }

    /**
     * Loads data into this object from the given query.  If this
     * object already contains table data, changes will be saved and
     * the object re-initialized first.
     *
     * @param $query SQL query
     *
     * @param $params parameter list in case you want to use
     * prepare/execute mode
     *
     * @return int DB_OK on success, DB_WARNING_READ_ONLY if the
     * returned object is read-only (because the object's specified
     * key column was not found among the columns returned by $query),
     * or another DB error code in case of errors.
     */
    function loadFromQuery($query, $params = false) {
	if (sizeof($this->_properties)) {
	    if (sizeof($this->_changes)) {
		$this->store();
		$this->_changes = array();
	    }
	    $this->_properties = array();
	}
	$rowdata = $this->_dbh->getRow($query, DB_FETCHMODE_ASSOC, $params);
	if (DB::isError($rowdata)) {
	    return $rowdata;
	}
	reset($rowdata);
	$found_keycolumn = false;
	while (list($key, $value) = each($rowdata)) {
	    if ($key == $this->_keycolumn) {
		$found_keycolumn = true;
	    }
	    $this->_properties[$key] = true;
	    $this->$key = &$value;
	    unset($value); // have to unset, or all properties will
	    		   // refer to the same value
	}
	if (!$found_keycolumn) {
	    $this->_readonly = true;
	    return DB_WARNING_READ_ONLY;
	}
	return DB_OK;
    }

    function set($property, &$newvalue) {
	// only change if $property is known and object is not
	// read-only
	if (!$this->_readonly && isset($this->_properties[$property])) {
	    $this->$property = $newvalue;
	    $this->_changes[$property]++;
	    return true;
	}
	return false;
    }

    function &get($property) {
	// only return if $property is known
	if (isset($this->_properties[$property])) {
	    return $this->$property;
	}
	return null;
    }

    function destroy($discard = false) {
	if (!$discard && sizeof($this->_changes)) {
	    $this->store();
	}
	$this->_properties = array();
	$this->_changes = array();
	$this->_table = null;
    }

    function store() {
	while (list($name, $changed) = each($this->_changes)) {
	    $params[] = &$this->$name;
	    $vars[] = $name . ' = ?';
	}
	if ($vars) {
	    $query = 'UPDATE ' . $this->_table . ' SET ' .
		implode(', ', $vars) . ' WHERE id = ?';
	    $params[] = $this->id;
	    $stmt = $this->_dbh->prepare($query);
	    $res = $this->_dbh->execute($stmt, &$params);
	    if (DB::isError($res)) {
		return $res;
	    }
	    $this->_changes = array();
	}
	return DB_OK;
    }
}

?>
