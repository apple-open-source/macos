<?php
/* vim: set expandtab tabstop=4 shiftwidth=4: */
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
// | Authors: Adam Daniel <adaniel1@eesus.jnj.com>                        |
// |          Bertrand Mansion <bmansion@mamasam.com>                     |
// +----------------------------------------------------------------------+
//
// $Id: Table.php,v 1.1.1.1 2001/07/19 00:20:49 zarzycki Exp $

require_once "PEAR.php";
require_once "HTML/Common.php";

/**
 * Builds an HTML table
 *
 * @author        Adam Daniel <adaniel1@eesus.jnj.com>
 * @author        Bertrand Mansion <bmansion@mamasam.com>
 * @version       1.5
 * @since         PHP 4.0.3pl1
 */
class HTML_Table extends HTML_Common {

    /**
     * Automatically adds a new row or column if a given row or column index does not exist
     * @var    bool
     * @access    private
     */
    var $_autoGrow = true;

    /**
     * Value to insert into empty cells
     * @var    string
     * @access    private
     */
    var $_autoFill = "&nbsp;";

    /**
     * Array containing the table structure
     * @var    array
     * @access    private
     */
    var $_structure = array();

    /**
     * Number of rows composing in the table
     * @var    int
     * @access    private
     */
    var $_rows = 0;

    /**
     * Number of column composing the table
     * @var    int
     * @access    private
     */
    var $_cols = 0;
    
    /**
     * Tracks the level of nested tables
     * @var       
     * @since     1.5
     * @access    private
     */
    var $_nestLevel = 0;

    /**
     * Class constructor
     * @param    array    $attributes        Associative array of table tag attributes
     * @param    int     $tabOffset
     * @access    public
     */
    function HTML_Table($attributes=null, $tabOffset=0)
    {
        $commonVersion = 1.3;
        if (HTML_Common::apiVersion() < $commonVersion) {
            return new PEAR_Error("HTML_Table version " . $this->apiVersion() . " requires " .
                "HTML_Common version $commonVersion or greater.", 0, PEAR_ERROR_TRIGGER);
        }
        HTML_Common::HTML_Common($attributes, $tabOffset);
    } // end constructor

    /**
     * Returns the API version
     * @access  public
     * @returns double
     */
    function apiVersion()
    {
        return 1.5;
    } // end func apiVersion

    /**
     * Sets the table caption
     * @param   string    $caption
     * @param   mixed    $attributes        Associative array or string of table row attributes
     * @access  public
     */
    function setCaption($caption, $attributes=null)
    {
        $attributes = $this->_parseAttributes($attributes);
        $this->_structure["caption"] = array("attr"=>$attributes, "contents"=>$caption);
    } // end func setCaption

    /**
     * Sets the autoFill value
     * @param   mixed   $fill
     * @access  public
     */
    function setAutoFill($fill)
    {
        $this->_autoFill = $fill;
    } // end func setAutoFill

    /**
     * Returns the autoFill value
     * @access   public
     * @returns  mixed
     */
    function getAutoFill()
    {
        return $this->_autoFill;
    } // end func getAutoFill

    /**
     * Sets the autoGrow value
     * @param    bool   $fill
     * @access   public
     */
    function setAutoGrow($grow)
    {
        $this->_autoGrow = $grow;
    } // end func setAutoGrow

    /**
     * Returns the autoGrow value
     * @access   public
     * @returns  mixed
     */
    function getAutoGrow()
    {
        return $this->_autoGrow;
    } // end func getAutoGrow

    /**
     * Sets the number of rows in the table
     * @param    int     $rows
     * @access   public
     */
    function setRowCount($rows)
    {
        $this->_rows = $rows;
    } // end func setRowCount

    /**
     * Sets the number of columns in the table
     * @param    int     $cols
     * @access   public
     */
    function setColCount($cols)
    {
        $this->_cols = $cols;
    } // end func setColCount

    /**
     * Returns the number of rows in the table
     * @access   public
     * @returns  int
     */
    function getRowCount()
    {
        return $this->_rows;
    } // end func getRowCount

    /**
     * Sets the number of columns in the table
     * @access   public
     * @returns  int
     */
    function getColCount()
    {
        return $this->_cols;
    } // end func getColCount

    /**
     * Sets a rows type 'TH' or 'TD'
     * @param    int         $row    Row index
     * @param    string      $type   'TH' or 'TD'
     * @access   public
     */

    function setRowType($row, $type)
    {
        for ($counter=0; $counter < $this->_cols; $counter++) {
            $this->_structure[$row][$counter]["type"] = $type;
        }
    } // end func setRowType

    /**
     * Sets a columns type 'TH' or 'TD'
     * @param    int         $col    Column index
     * @param    string      $type   'TH' or 'TD'
     * @access   public
     */
    function setColType($col, $type)
    {
        for ($counter=0; $counter < $this->_rows; $counter++) {
            $this->_structure[$counter][$col]["type"] = $type;
        }
    } // end func setColType

    /**
     * Sets the cell attributes for an existing cell.
     *
     * If the given indices do not exist and autoGrow is true then the given 
     * row and/or col is automatically added.  If autoGrow is false then an 
     * error is returned.
     * @param    int        $row        Row index
     * @param    int        $col        Column index
     * @param    mixed      $attributes    Associative array or string of table row attributes
     * @access    public
     * @throws   PEAR_Error
     */
    function setCellAttributes($row, $col, $attributes)
    {
        if ($this->_structure[$row][$col] == "SPANNED") return;
        if ($row >= $this->_rows) {
            if ($this->_autoGrow) {
                $this->_rows = $row+1;
            } else {
                return new PEAR_Error("Invalid table row reference[$row] in HTML_Table::setCellAttributes");
            }
        }
        if ($col >= $this->_cols) {
            if ($this->_autoGrow) {
                $this->_cols = $col+1;
            } else {
                return new PEAR_Error("Invalid table column reference[$col] in HTML_Table::setCellAttributes");
            }
        }
        $attributes = $this->_parseAttributes($attributes);
        $this->_structure[$row][$col]["attr"] = $attributes;
        $this->_updateSpanGrid($row, $col);
    } // end func setCellAttributes

    /**
     * Updates the cell attributes passed but leaves other existing attributes in tact
     * @param    int     $row        Row index
     * @param    int     $col        Column index
     * @param    mixed   $attributes    Associative array or string of table row attributes
     * @access   public
     */
    function updateCellAttributes($row, $col, $attributes)
    {
        if ($this->_structure[$row][$col] == "SPANNED") return;
        $attributes = $this->_parseAttributes($attributes);
        $this->_updateAttrArray($this->_structure[$row][$col]["attr"], $attributes);
        $this->_updateSpanGrid($row, $col);
    } // end func updateCellAttributes

    /**
     * Sets the cell contents for an existing cell
     *
     * If the given indices do not exist and autoGrow is true then the given 
     * row and/or col is automatically added.  If autoGrow is false then an 
     * error is returned.
     * @param    int        $row        Row index
     * @param    int        $col        Column index
     * @param    mixed    $contents    May contain html or any object with a toHTML method
     * @param    string    $type       (optional) Cell type either 'TH' or 'TD'
     * @access    public
     * @throws   PEAR_Error
     */
    function setCellContents($row, $col, $contents, $type='TD')
    {
        if ($this->_structure[$row][$col] == "SPANNED") return;
        if ($row >= $this->_rows) {
            if ($this->_autoGrow) {
                $this->_rows = $row+1;
            } else {
                return new PEAR_Error("Invalid table row reference[$row] in HTML_Table::setCellContents");
            }
        }
        if ($col >= $this->_cols) {
            if ($this->_autoGrow) {
                $this->_cols = $col+1;
            } else {
                return new PEAR_Error("Invalid table column reference[$col] in HTML_Table::setCellContents");
            }
        }
        $this->_structure[$row][$col]["contents"] = $contents;
        $this->_structure[$row][$col]["type"] = $type;
    } // end func setCellContents

    /**
     * Returns the cell contents for an existing cell
     * @param    int        $row    Row index
     * @param    int        $col    Column index
     * @access    public
     * @return   mixed
     */
    function getCellContents($row, $col)
    {        
        if ($this->_structure[$row][$col] == "SPANNED") return;
        return $this->_structure[$row][$col]["contents"];
    } // end func getCellContents

    /**
     * Sets the contents of a header cell
     * @param    int     $row
     * @param    int     $col
     * @param    mixed   $contents
     * @access   public
     */
    function setHeaderContents($row, $col, $contents)
    {
        $this->setCellContents($row, $col, $contents, 'TH');
    } // end func setHeaderContents

    /**
     * Adds a table row and returns the row identifier
     * @param    array    $contents   (optional) Must be a indexed array of valid cell contents
     * @param    mixed    $attributes (optional) Associative array or string of table row attributes
     * @param    string    $type       (optional) Cell type either 'TH' or 'TD'
     * @returns    int
     * @access    public
     */
    function addRow($contents=null, $attributes=null, $type='TD') 
    {
        if (isset($contents) && !is_array($contents)) {
            return new PEAR_Error("First parameter to HTML_Table::addRow must be an array");
        }
        $row = $this->_rows++;
        for ($counter=0; $counter < count($contents); $counter++) {
            if ($type == 'TD') {
                $this->setCellContents($row, $counter, $contents[$counter]);
            } elseif ($type == 'TH') {
                $this->setHeaderContents($row, $counter, $contents[$counter]);
            }
        }
        $this->setRowAttributes($row, $attributes);
        return $row;
    } // end func addRow

    /**
     * Sets the row attributes for an existing row
     * @param    int        $row            Row index
     * @param    mixed    $attributes        Associative array or string of table row attributes
     * @access    public
     */
    function setRowAttributes($row, $attributes)
    {
        for ($i = 0; $i < $this->_cols; $i++) {
            $this->setCellAttributes($row,$i,$attributes);
        }
    } // end func setRowAttributes

    /**
     * Updates the row attributes for an existing row
     * @param    int        $row            Row index
     * @param    mixed    $attributes        Associative array or string of table row attributes
     * @access    public
     */
    function updateRowAttributes($row, $attributes=null)
    {
        for ($i = 0; $i < $this->_cols; $i++) {
            $this->updateCellAttributes($row,$i,$attributes);
        }
    } // end func updateRowAttributes

    /**
     * Alternates the row attributes starting at $start
     * @param    int        $start            Row index of row in which alternatign begins
     * @param    mixed    $attributes1    Associative array or string of table row attributes
     * @param    mixed    $attribute2        Associative array or string of table row attributes
     * @access    public
     */
    function altRowAttributes($start, $attributes1, $attributes2) 
    {
        for ($row = $start ; $row < $this->_rows ; $row++) {
            $attributes = (($row+1+$start)%2==0) ? $attributes1 : $attributes2;
            $this->updateRowAttributes($row, $attributes);
        }
    } // end func altRowAttributes

    /**
     * Adds a table column and returns the column identifier
     * @param    array    $contents   (optional) Must be a indexed array of valid cell contents
     * @param    mixed    $attributes (optional) Associative array or string of table row attributes
     * @param    string    $type       (optional) Cell type either 'TH' or 'TD'
     * @returns    int
     * @access    public
     */
    function addCol($contents=null, $attributes=null, $type='TD')
    {
        if (isset($contents) && !is_array($contents)) {
            return new PEAR_Error("First parameter to HTML_Table::addCol must be an array");
        }
        $col = $this->_cols++;
        for ($counter=0; $counter < count($contents); $counter++) {
            $this->setCellContents($counter, $col, $contents[$counter], $type);
        }
        $this->setColAttributes($col, $attributes);
        return $col;
    } // end func addCol

    /**
     * Sets the column attributes for an existing column
     * @param    int        $col            Column index
     * @param    mixed    $attributes        (optional) Associative array or string of table row attributes
     * @access    public
     */
    function setColAttributes($col, $attributes=null)
    {
        for ($i = 0; $i < $this->_rows; $i++) {
            $this->setCellAttributes($i,$col,$attributes);
        }
    } // end func setColAttributes

    /**
     * Updates the column attributes for an existing column
     * @param    int        $col            Column index
     * @param    mixed    $attributes        (optional) Associative array or string of table row attributes
     * @access    public
     */
    function updateColAttributes($col, $attributes=null)
    {
        for ($i = 0; $i < $this->_rows; $i++) {
            $this->updateCellAttributes($i,$col,$attributes);
        }
    } // end func updateColAttributes

    /**
     * Returns the table structure as HTML
     * @access  public
     * @return  string
     */      
    function toHtml()
    {
        $tabs = $this->_getTabs();
        $strHtml =
            "\n" . $tabs . "<!-- BEGIN TABLE LEVEL: $this->_nestLevel -->\n";
        if ($this->_comment) {
            $strHtml .= $tabs . "<!-- $this->_comment -->\n";
        }
        $strHtml .= 
            $tabs . "<TABLE" . $this->_getAttrString($this->_attributes) . ">\n";
        if ($this->_structure["caption"]) {
            $attr = $this->_structure["caption"]["attr"];
            $contents = $this->_structure["caption"]["contents"];
            $strHtml .= $tabs . "\t<CAPTION" . $this->_getAttrString($attr) . ">";
            if (is_array($contents)) $contents = implode(", ",$contents);
            $strHtml .= $contents;
            $strHtml .= "</CAPTION>\n";
        }
        for ($i = 0 ; $i < $this->_rows ; $i++) {
            $strHtml .= $tabs ."\t<TR>\n";
            for ($j = 0 ; $j < $this->_cols ; $j++) {
                if ($this->_structure[$i][$j] == "SPANNED") {
                    $strHtml .= $tabs ."\t\t<!-- span -->\n";
                    continue;
                }
                $type = ($this->_structure[$i][$j]["type"] == "TH" ? "TH" : "TD");
                $attr = $this->_structure[$i][$j]["attr"];
                $contents = $this->_structure[$i][$j]["contents"];
                $strHtml .= $tabs . "\t\t<$type" . $this->_getAttrString($attr) . ">";
                if (is_object($contents)) {
                    if (is_subclass_of($contents, "html_common")) {
                        $contents->setTabOffset($this->_tabOffset + 3);
                        $contents->_nestLevel = $this->_nestLevel + 1;
                    }
                    if (method_exists($contents, "toHtml")) {
                        $contents = $contents->toHtml();
                    } elseif (method_exists($contents, "toString")) {
                        $contents = $contents->toString();
                    }
                }
                if (is_array($contents)) $contents = implode(", ",$contents);
                if (isset($this->_autoFill) && $contents == "") $contents = $this->_autoFill;
                $strHtml .= $contents;
                $strHtml .= "</$type>\n";
            }
            $strHtml .= $tabs ."\t</TR>\n";
        }
        $strHtml .= 
            $tabs . "</TABLE><!-- END TABLE LEVEL: $this->_nestLevel -->";
        return $strHtml;
    } // end func toHtml

    /**
     * Checks if rows or columns are spanned
     * @param    int        $row            Row index
     * @param    int        $col            Column index
     * @access   private
     */
    function _updateSpanGrid($row, $col)
    {
        $colspan = $this->_structure[$row][$col]["attr"]["colspan"];
        $rowspan = $this->_structure[$row][$col]["attr"]["rowspan"];
        if ($colspan) {
            for ($j = $col+1; (($j < $this->_cols) && ($j <= ($col + $colspan - 1))); $j++) {
                $this->_structure[$row][$j] = "SPANNED";
            }
        }
        if ($rowspan) {
            for ($i = $row+1; (($i < $this->_rows) && ($i <= ($row + $rowspan - 1))); $i++) {
                $this->_structure[$i][$col] = "SPANNED";
            }
        }
        if ($colspan && $rowspan) {
            for ($i = $row+1; (($i < $this->_rows) && ($i <= ($row + $rowspan - 1))); $i++) {
                for ($j = $col+1; (($j <= $this->_cols) && ($j <= ($col + $colspan - 1))); $j++) {
                    $this->_structure[$i][$j] = "SPANNED";
                }
            }
        }
    } // end func _updateSpanGrid

} // end class HTML_Table
?>
