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
// $Id: sql2xml_ext.php,v 1.1.1.1 2001/07/19 00:20:48 zarzycki Exp $

require_once ("XML/sql2xml.php");

/**
 *  This class shows with one example, how the base sql2xml-class
 *   could be extended.
 *
 * Usage example
 *
 * include_once ("DB.php");
 * include_once("XML/sql2xml_ext.php");
 * $xml = new xml_sql2xml_ext;
 * $options= array( user_options => array (xml_seperator =>"_",
 *                                       element_id => "id"),
 * );
 * $db = DB::connect("mysql://root@localhost/xmltest");
 * $xml = new xml_sql2xml;
 * $result = $db->query("select * from bands");
 * $xmlstring = $xml->getxml($result,$options));

 * more examples and outputs on
 *   http://www.nomad.ch/php/sql2xml/
 *   for the time being
 *
 * @author   Christian Stocker <chregu@nomad.ch>
 * @version  $Id: sql2xml_ext.php,v 1.1.1.1 2001/07/19 00:20:48 zarzycki Exp $
 */
class XML_sql2xml_ext extends XML_sql2xml {


    function insertNewRow($parent_row, $res, $key, &$tableInfo)
    {

        $new_row= $parent_row->new_child($tableInfo[$key]["table"],Null);
        /* make an unique ID attribute in the row element with tablename.id if there's an id
               otherwise just make an unique id with the php-function, just that there's a unique id for this row.
                CAUTION: This ID changes every time ;) (if no id from db-table)
               */
        $new_row->set_attribute("type","row");
        if ($res[$tableInfo["id"][$tableInfo[$key]["table"]]])
        {
            if ($res[$tableInfo["id"][$tableInfo[$key]["table"]]] == $this->user_options[id])
            {
                $new_row->set_attribute("selected", "selected");
            }
            $new_row->set_attribute("ID", utf8_encode($tableInfo[$key]["table"] . $res[$tableInfo["id"][$tableInfo[$key]["table"]]]));
        }
        else
        {
            $this->IDcounter[$tableInfo[$key]["table"]]++;
            $new_row->set_attribute("ID", $tableInfo[$key]["table"].$this->IDcounter[$tableInfo[$key]["table"]]);

        }

        return $new_row;
    }


    function insertNewResult(&$tableInfo) {
        
        if ($this->user_options["result_root"]) 
            $result_root = $this->user_options["result_root"];
        else 
            $result_root = $tableInfo[0]["table"];
        
        if ($this->xmlroot)
            $xmlroot=$this->xmlroot->new_child($result_root,Null);
        else
            $xmlroot= $this->xmldoc->add_root($result_root);
        $xmlroot->set_attribute("type","resultset");
        return $xmlroot;
    }
    

    function insertNewElement($parent, $res, $key, &$tableInfo, &$subrow) {

        if (is_array($this->user_options["attributes"]) && in_array($tableInfo[$key]["name"],$this->user_options["attributes"])) {
            $subrow=$parent->set_attribute($tableInfo[$key]["name"],$this->xml_encode($res[$key]));
        }
        elseif ($this->user_options["xml_seperator"])
        {
            //the preg should be only done once....
            $i = 0;
            preg_match_all("/([^" . $this->user_options["xml_seperator"] . "]+)" . $this->user_options[xml_seperator] . "*/", $tableInfo[$key]["name"], $regs);
            $subrow[$regs[1][-1]] = $parent;

            // here we separate db fields to subtags.
            for ($i = 0; $i < (count($regs[1]) - 1); $i++)
            {
                if ( ! $subrow[$regs[1][$i]]) {
                    $subrow[$regs[1][$i]] = $subrow[$regs[1][$i - 1]]->new_child($regs[1][$i], NULL);
                }
            }
            $subrows = $subrow[$regs[1][$i - 1]]->new_child($regs[1][$i], $this->xml_encode($res[$key]));
        }
        else
        {
            $subrow=$parent->new_child($tableInfo[$key]["name"], $this->xml_encode($res[$key]));
        }

    }

    function addTableinfo($key, $value, &$tableInfo) {
        if (!$tableInfo[id][$value["table"]] && $value["name"] == $this->user_options["element_id"] )
        {
            $tableInfo[id][$value["table"]]= $key;
        }
        if ($this->user_options["field_translate"][$value["name"]]) {
            $tableInfo[$key]["name"] = $this->user_options["field_translate"][$value["name"]];
        }
    }
}
?>
