<?php
// +----------------------------------------------------------------------+
// | PHP version 4.0                                                      |
// +----------------------------------------------------------------------+
// | Copyright (c) 1997, 1998, 1999, 2000, 2001 The PHP Group             |
// +----------------------------------------------------------------------+
// | This source file is subject to version 2.0 of the PHP license,       |
// | that is bundled with this package in the file LICENSE, and is        |
// | available at through the world-wide-web at                           |
// | http://www.php.net/license/2_02.txt.                                 |
// | If you did not receive a copy of the PHP license and are unable to   |
// | obtain it through the world-wide-web, please send a note to          |
// | license@php.net so we can mail you a copy immediately.               |
// +----------------------------------------------------------------------+
// | Authors: Christian Stocker <chregu@nomad.ch>                         |
// +----------------------------------------------------------------------+
//
// $Id: sql2xml.php,v 1.1.1.1 2001/07/19 00:20:48 zarzycki Exp $

/**
* This class takes a PEAR::DB-Result Object, a sql-query-string or an array
*  and returns a xml-representation of it.
*
* TODO
*   -encoding etc, options for header
*   -ERROR CHECKING
*
* Usage example
*
* include_once ("DB.php");
* include_once("XML/sql2xml.php");
* $db = DB::connect("mysql://root@localhost/xmltest");
* $sql2xml = new xml_sql2xml();
* $result = $db->query("select * from bands");
* $xmlstring = $sql2xml->getXML($result);
*
* or
*
* include_once ("DB.php");
* include_once("XML/sql2xml.php");
* $sql2xml = new xml_sql2xml("mysql://root@localhost/xmltest");
* $sql2xml->Add("select * from bands");
* $xmlstring = $sql2xml->getXML();
*
* More documentation and a tutorial/how-to can be found at
*   http://www.nomad.ch/php/sql2xml
*
* @author   Christian Stocker <chregu@nomad.ch>
* @version  $Id: sql2xml.php,v 1.1.1.1 2001/07/19 00:20:48 zarzycki Exp $
* @package  XML
*/
class XML_sql2xml {

    /**
    * If joined-tables should be output nested.
    *  Means, if you have joined two or more queries, the later
    *   specified tables will be nested within the result of the former
    *   table.
    *   Works at the moment only with mysql automagically. For other RDBMS
    *   you have to provide your table-relations by hand (see user_tableinfo)
    *
    * @var  boolean
    * @see  $user_tableinfo, doSql2Xml(), doArray2Xml();
    */
    var $nested = True;

    /**
    * Name of the tag element for resultsets
    *
    * @var  string
    * @see  insertNewResult()
    */
    var $tagNameResult = "result";

    /**
    * Name of the tag element for rows
    *
    * @var  string
    * @see  insertNewRow()
    */
    var $tagNameRow = "row";

    /**
    *
    * @var   object PEAR::DB
    * @acces private
    */
    var $db = Null;


    /**
    * Options to be used in extended Classes (for example in sql2xml_ext).
    * They are passed with SetOptions as an array (arrary("user_optpaions" = array());
    *  and can then be accessed with $this->user_options["bla"] from your
    *  extended classes for additional features.
    *  This array is not use in this base class, it's only for passing easy parameters
    *  to extended classes.
    *
    * @var      array
    */
    var $user_options = array();


    /**
    * The DomDocument Object to be used in the whole class
    *
    * @var      object  DomDocument
    * @acces    private
    */
    var $xmldoc;


    /**
    * The Root of the domxml object
    * I'm not sure, if we need this as a class variable....
    *
    * @var      object DomNode
    * @acces    private
    */
    var $xmlroot;


    /**
    * This array is used to give the structure of your database to the class.
    *  It's especially useful, if you don't use mysql, since other RDBMS than
    *  mysql are not able at the moment to provide the right information about
    *  your database structure within the query. And if you have more than 2
    *  tables joined in the sql it's also not possible for mysql to find out
    *  your real relations.
    *  The parameters are the same as in fieldInfo from the PEAR::DB and some
    *   additional ones. Here they come:
    *  From PEAR::DB->fieldinfo:
    *
    *    $tableInfo[$i]["table"]    : the table, which field #$i belongs to.
    *           for some rdbms/comples queries and with arrays, it's impossible
    *           to find out to which table the field actually belongs. You can
    *           specify it here more accurate. Or if you want, that one fields
    *           belongs to another table, than it actually says (yes, there's
    *           use for that, see the upcoming tutorial ...)
    *
    *    $tableInfo[$i]["name"]     : the name of field #$i. if you want another
    *           name for the tag, than the query or your array provides, assign
    *           it here.
    *
    *   Additional info
    *     $tableInfo["parent_key"][$table]  : index of the parent key for $table.
    *           this is the field, where the programm looks for changes, if this
    *           field changes, it assumes, that we need a new "rowset" in the
    *           parent table.
    *
    *     $tableInfo["parent_table"][$table]: name of the parent table for $table.
    *
    * @var      array
    * @acces    private
    */
    var $user_tableInfo = array();


    /**
    * Constructor
    * The Constructor can take a Pear::DB "data source name" (eg.
    *  "mysql://user:passwd@localhost/dbname") and will then connect
    *  to the DB, or a PEAR::DB object link, if you already connected
    *  the db before.
    "  If you provide nothing as $dsn, you only can later add stuff with
    *   a pear::db-resultset or as an array. providing sql-strings will
    *   not work.
    * the $root param is used, if you want to provide another name for your
    *  root-tag than "root". if you give an empty string (""), there will be no
    *  root element created here, but only when you add a resultset/array/sql-string.
    *  And the first tag of this result is used as the root tag.
    *
    * @param  $dsn string with PEAR::DB "data source name" or object DB object
    * @param  $root string of the name of the xml-doc root element.
    */
    function XML_sql2xml ($dsn=Null,$root = "root") {

        // if it's a string, then it must be a dsn-identifier;
        if (is_string($dsn))
        {
            include_once ("DB.php");
            $this->db = DB::Connect($dsn);
            if (DB::isError($this->db))
            {
                print "The given dsn for XML_sql2xml was not valid in file ".__FILE__." at line ".__LINE__."<br>\n";
                return new DB_Error($this->db->code,PEAR_ERROR_DIE);
            }

        }

        elseif (DB::isError($dsn))
        {
            print "The given param for XML_sql2xml was not valid in file ".__FILE__." at line ".__LINE__."<br>\n";
            return new DB_Error($dsn->code,PEAR_ERROR_DIE);
        }

        // if parent class is db_common, then it's already a connected identifier
        elseif (get_parent_class($dsn) == "db_common")
        {
            $this->db = $dsn;
        }


        $this->xmldoc = domxml_new_xmldoc('1.0');
        if ($root) {
            $this->xmlroot = $this->xmldoc->add_root($root);
        }

    }

    /**
    * General method for adding new resultsets to the xml-object
    *  Give a sql-query-string, a pear::db_result object or an array as
    *  input parameter, and the method calls the appropriate method for this
    *  input and adds this to $this->xmldoc
    *
    * @param    $resultset string sql-string, or object db_result, or array
    * @access   public
    * @see      addResult(), addSql(), addArray()
    */
    function add ($resultset)
    {
        // if string, then it's a query...
        if (is_string($resultset)) {
            $this->AddSql($resultset);
        }
        // if array, then it's an array...
        elseif (is_array($resultset)) {
            $this->AddArray($resultset);
        }

        if (get_class($resultset) == "db_result") {
            $this->AddResult($resultset);
        }
    }

    /**
    * Adds an additional pear::db_result resultset to $this->xmldoc
    *
    * @param    Object db_result result from a DB-query
    * @see      doSql2Xml()
    * @access   public
    */
    function addResult($result)
    {
        $this->doSql2Xml($result);
    }

    /**
    * Adds an aditional resultset generated from an sql-statement
    *  to $this->xmldoc
    *
    * @param    string sql a string containing an sql-statement.
    * @access   public
    * @see      doSql2Xml()
    */
    function addSql($sql)
    {
        $result = $this->db->query($sql);
        $this->doSql2Xml($result);
    }

    /**
    * Adds an aditional resultset generated from an Array
    *  to $this->xmldoc
    * TODO: more explanation, how arrays are transferred
    *
    * @param    array multidimensional array.
    * @access   public
    * @see      doArray2Xml()
    */
    function addArray ($array)
    {
        $parent_row = $this->insertNewResult(&$metadata);
        $this->DoArray2Xml($array,$parent_row);
    }

    /**
    * Returns an xml-string with a xml-representation of the resultsets.
    *
    * The resultset can be directly provided here, or if you need more than one
    * in your xml, then you have to provide each of them with add() before you
    * call getXML, but the last one can also be provided here.
    *
    * @param    Object  result result from a DB-query
    * @return   string  xml
    * @access   public
    */
    function getXML($result = Null)
    {
        return domxml_dumpmem($this->getXMLObject($result));
    }

    /**
    * Returns an xml DomDocument Object with a xml-representation of the resultsets.
    *
    * The resultset can be directly provided here, or if you need more than one
    * in your xml, then you have to provide each of them with add() before you
    * call getXMLObject, but the last one can also be provided here.
    *
    * @param    Object result result from a DB-query
    * @return   Object DomDocument
    * @access   public
    */
    function getXMLObject($result = Null)
    {
        if ($result) {
            $this->add ($result);
        }
        return $this->xmldoc;
    }

    /**
    * For adding db_result-"trees" to $this->xmldoc
    * @param    Object db_result
    * @access   private
    * @see      addResult(),addSql()
    */
    function doSql2Xml($result)
    {

        if (DB::IsError($result)) {
            print "Error in file ".__FILE__." at line ".__LINE__."<br>\n";
            new DB_Error($result->code,PEAR_ERROR_DIE);
        }

        // the method_exists is here, cause tableInfo is only in the cvs at the moment
        // (should be in 4.0.6)
        // BE CAREFUL: if you have fields with the same name in different tables, you will get errors
        // later, since DB_FETCHMODE_ASSOC doesn't differentiate that stuff.

        if (!method_exists($result,"tableInfo") || ! ($tableInfo = $result->tableInfo(False)))
        {
            //emulate tableInfo. this can go away, if every db supports tableInfo
            $fetchmode = DB_FETCHMODE_ASSOC;
            $res = $result->FetchRow($fetchmode);
            $this->nested = False;
            $i = 0;

            while (list($key, $val) = each($res))
            {
                $tableInfo[$i]["table"]= $this->tagNameResult;
                $tableInfo[$i]["name"] = $key;
                $resFirstRow[$i] = $val;
                $i++;
            }
            $res  = $resFirstRow;
            $FirstFetchDone = True;
            $fetchmode = DB_FETCHMODE_ORDERED;
        }
        else
        {
            $fetchmode = DB_FETCHMODE_ORDERED;
        }

        // initialize db hierarchy...
        $parenttable = "root";
        $tableInfo["parent_key"]["root"] = 0;
        foreach ($tableInfo as $key => $value)
        {
            if (is_int($key))
            {
                if (is_null($tableInfo["parent_table"][$value["table"]]))
                {
                    $tableInfo["parent_key"][$value["table"]] = $key;
                    $tableInfo["parent_table"][$value["table"]] = $parenttable;
                    $parenttable = $value["table"] ;
                }
            }
            //if you need more tableInfo for later use you can write a function addTableInfo..
            $this->addTableInfo($key, $value, &$tableInfo);
        }

        // end initialize

        // if user made some own tableInfo data, merge them here.
        if ($this->user_tableInfo)
        {
            $tableInfo = $this->array_merge_clobber($tableInfo,$this->user_tableInfo);
        }
        $parent[root] = $this->insertNewResult(&$tableInfo);

        while ($FirstFetchDone || $res = $result->FetchRow($fetchmode))
        {
            //FirstFetchDone is only for emulating tableInfo, as long as not all dbs support tableInfo. can go away later
            $FirstFetchDone = False;
            while (list($key, $val) = each($res))
            {
                if ($resold[$tableInfo["parent_key"][$tableInfo[$key]["table"]]] != $res[$tableInfo["parent_key"][$tableInfo[$key]["table"]]] || !$this->nested)
                {
                    if ($tableInfo["parent_key"][$tableInfo[$key]["table"]] == $key )
                    {
                        if ($this->nested || $key == 0)
                        {

                            $parent[$tableInfo[$key]["table"]] =  $this->insertNewRow($parent[$tableInfo["parent_table"][$tableInfo[$key]["table"]]], $res, $key, &$tableInfo);
                        }
                        else
                        {
                            $parent[$tableInfo[$key]["table"]]= $parent[$tableInfo["parent_table"][$tableInfo[$key]["table"]]];
                        }

                        //set all children entries to somethin stupid
                        foreach($tableInfo["parent_table"] as $pkey => $pvalue)
                        {
                            if ($pvalue == $tableInfo[$key]["table"])
                            {
                                $resold[$tableInfo["parent_key"][$pkey]]= "ThisIsJustAPlaceHolder";
                            }
                        }

                    }

                    $this->insertNewElement($parent[$tableInfo[$key]["table"]], $res, $key, &$tableInfo, &$subrow);


                }
            }

            $resold = $res;
            unset ($subrow);
        }
        return $this->xmldoc;
    }

    /**
    * For adding whole arrays to $this->xmldoc
    *
    * @param    array
    * @param    Object domNode
    * @access   private
    * @see      addArray()
    */

    function DoArray2Xml ($array, $parent) {
        while (list($key, $val) = each($array))
            {
                $tableInfo[$key]["table"]= $this->tagNameResult;
                $tableInfo[$key]["name"] = $key;
            }
        if ($this->user_tableInfo)
        {
            $tableInfo = $this->array_merge_clobber($tableInfo,$this->user_tableInfo);
        }

        foreach ($array as $key=>$value)
        {
            if (is_array($value) ) {
                if (is_int($key) )
                {
                    $valuenew = array_slice($value,0,1);
                    $keynew = array_keys($valuenew);
                    $keynew = $keynew[0];
                }
                else
                {
                    $valuenew = $value;
                    $keynew = $key;
                }
                $rec2 = $this->insertNewRow($parent, $valuenew, $keynew, &$tableInfo);
                $this->DoArray2xml($value,$rec2);
            }
            else {
                $this->insertNewElement($parent, $array, $key, &$tableInfo,&$subrow);
            }
        }

    }



    /**
    * This method sets the options for the class
    *  One can only set variables, which are defined at the top of
    *  of this class.
    *
    * @param    array   options to be passed to the class
    * @access   public
    * @see      $nested,$user_options,$user_tableInfo
    */

    function setOptions($options) {
    //set options
        if (is_array($options))
        {
            foreach ($options as $option => $value)
            {
                if (isset($this->$option)) {
                    $this->$option = $value;
                }

            }
        }
    }

    // these are the functions, which are intended to be overriden in user classes

    /**
    *
    * @param
    * @return   object  DomNode
    * @abstract
    * @access   private
    */
    function insertNewResult(&$metadata)
    {
        if ($this->xmlroot)
            return $this->xmlroot->new_child($this->tagNameResult, NULL);
        else
            return $this->xmldoc->add_root($this->tagNameResult);
    }


    /**
    *
    * @param    object  DomNode
    * @param
    * @param
    * @param
    * @return
    * @abstract
    * @acces private
    */
    function insertNewRow($parent_row, $res, $key, &$metadata)
    {
        return  $parent_row->new_child($this->tagNameRow, NULL);
    }


    /**
    *
    * @param    object  DomNode
    * @param
    * @param
    * @param
    * @param
    * @return
    * @abstract
    * @acces private
    */
    function insertNewElement($parent, $res, $key, &$metadata, &$subrow)
    {
        return  $parent->new_child($metadata[$key]["name"], $this->xml_encode($res[$key]));
    }


    /**
    *
    * @param
    * @param
    * @param
    * @abstract
    * @acces private
    */
    function addTableInfo($key, $value, &$metadata) {

    }

    // end functions, which are intended to be overriden in user classes

    // here come some helper functions...

    /**
    * make utf8 out of the input data and escape & with &amp;
    *  I'm not sure, if this is the standard way, but it works for me.
    *
    * @param    string text to be utfed.
    * @acces private
    */
    function xml_encode ($text)
    {
        $text = utf8_encode(ereg_replace("&","&amp;",$text));
        return $text;
    }

    //taken from kc@hireability.com at http://www.php.net/manual/en/function.array-merge-recursive.php
    /**
    * There seemed to be no built in function that would merge two arrays recursively and clobber
    *   any existing key/value pairs. Array_Merge() is not recursive, and array_merge_recursive
    *   seemed to give unsatisfactory results... it would append duplicate key/values.
    *
    *   So here's a cross between array_merge and array_merge_recursive
    **/
    /**
    *
    * @param    array first array to be merged
    * @param    array second array to be merged
    * @return   array merged array
    * @acces private
    */
    function array_merge_clobber($a1,$a2)
    {
        if(!is_array($a1) || !is_array($a2)) return false;
        $newarray = $a1;
        while (list($key, $val) = each($a2))
        {
            if (is_array($val) && is_array($newarray[$key]))
            {
                $newarray[$key] = $this->array_merge_clobber($newarray[$key], $val);
            }
            else
            {
                $newarray[$key] = $val;
            }
        }
        return $newarray;
    }
}
?>
