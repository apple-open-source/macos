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
// +----------------------------------------------------------------------+
//
// $Id: Common.php,v 1.1.1.2 2001/12/14 22:14:51 zarzycki Exp $

/**
 * Base class for all HTML classes
 *
 * @author      Adam Daniel <adaniel1@eesus.jnj.com>
 * @version     1.5
 * @since       PHP 4.0.3pl1
 * @abstract
 */
class HTML_Common {

    /**
     * Associative array of table attributes
     * @var  array
     * @access   private
     */
    var $_attributes = array();

    /**
     * Tab offset of the table
     * @var  int
     * @access   private
     */
    var $_tabOffset = 0;
    
    /**
     * HTML comment on the object
     * @var       string
     * @since     1.5
     * @access    private
     */
    var $_comment = "";

    /**
     * Class constructor
     * @param    mixed   $attributes     Associative array of table tag attributes 
     *                                   or HTML attributes name="value" pairs
     * @access   public
     */
    function HTML_Common($attributes=null, $tabOffset=0)
    {
        $this->setTabOffset($tabOffset);
        $this->setAttributes($attributes);
    } // end constructor

    /**
     * Returns the current API version
     * @access   public
     * @returns  double
     */
    function apiVersion()
    {
        return 1.4;
    } // end func apiVersion

    /**
     * Returns a string of \t for the tabOffset property
     * @access   private
     */
    function _getTabs()
    {
        return $this->_tabOffset > 0 ? str_repeat("\t", $this->_tabOffset) : "";
    } // end func _getTabs

    /**
     * Returns an HTML formatted attribute string
     * @param    array   $attributes
     * @return   string
     * @access   private
     */
    function _getAttrString($attributes)
    {
        $strAttr = '';
        if (is_array($attributes)) {
            while (list($key, $value) = each($attributes)) {
                if (is_int($key)) {
                    $strAttr .= ' ' . strtolower($value) . '="' . strtolower($value) . '"';
                } else {
                    $strAttr .= ' ' . strtolower($key) . '="' . htmlentities($value) . '"';
                }
            }
        }
        return $strAttr;
    } // end func _getAttrString

    /**
     * Returns a valid atrributes array from either a string or array
     * @param    mixed   $attributes     Either a typical HTML attribute string or an associative array
     * @access   private
     */
    function _parseAttributes($attributes)
    {
        if (is_array($attributes)) {
            return $attributes;
        } elseif (is_string($attributes)) {
            $preg = "/(([A-Za-z_:]|[^\\x00-\\x7F])([A-Za-z0-9_:.-]|[^\\x00-\\x7F])*)" .
                "([ \\n\\t\\r]+)?(=([ \\n\\t\\r]+)?(\"[^\"]*\"|'[^']*'|[^ \\n\\t\\r]*))?/";
            if (preg_match_all($preg, $attributes, $regs)) {
                $valCounter = 0;
                for ($counter=0; $counter<count($regs[1]); $counter++) {
                    $name = $regs[1][$counter];
                    $check = $regs[0][$counter];
                    $value = $regs[7][$valCounter];
                    if (trim($name) == trim($check)) {
                        $arrAttr[] = strtoupper(trim($name));
                    } else {
                        if (substr($value, 0, 1) == "\"" || substr($value, 0, 1) == "'") {
                            $value = substr($value, 1, -1);
                        }
                        $arrAttr[strtolower(trim($name))] = trim($value);
                        $valCounter++;
                    }
                }
                return $arrAttr;
            }
        }
    } // end func _parseAttributes

    /**
     * Returns the array key for the given non-name-value pair attribute
     * 
     * @param     string    $attr         Attribute
     * @param     array     $attributes   Array of attribute
     * @since     1.0
     * @access    public
     * @return    void
     * @throws    
     */
    function _getAttrKey($attr, $attributes)
    {
        if (is_array($attributes)) {
            $keys = array_keys($attributes);
            for ($counter=0; $counter < count($keys); $counter++) {
                $key = $keys[$counter];
                $value = $attributes[$key];
                if (strtoupper($value) == strtoupper($attr) && is_int($key)) {
                    return $key;
                    break;
                }
            }
        }
    } //end func _getAttrKey

    /**
     * Updates the attributes in $attr1 with the values in $attr2 without changing the other existing attributes
     * @param    array   $attr1      Original attributes array
     * @param    array   $attr2      New attributes array
     * @access   private
     * @return   array
     */
    function _updateAttrArray(&$attr1, $attr2)
    {
        if (!is_array($attr2)) {
            return false;
        }
        while (list($key, $value) = each($attr2)) {
            if (!is_int($key)) {
                $attr1[$key] = $value;
            } else {
                $key = $this->_getAttrKey($value, $attr1);
                if (isset($key)) {
                    $attr1[$key] = $value;
                } else {
                    if (is_array($attr1)) {
                        array_unshift($attr1, $value);
                    } else {
                        $attr1[] = $value;
                    }
                }
            }
        }
    } // end func _updateAtrrArray
    
    /**
     * Removes the given attribute from the given array
     * 
     * @param     string    $attr           Attribute name
     * @param     array     $attributes     Attribute array
     * @since     1.4
     * @access    public
     * @return    void
     * @throws    
     */
    function _removeAttr($attr, &$attributes)
    {
        if (in_array(strtolower($attr), array_keys($attributes))) {
            unset($attributes[$attr]);
        } else {
            unset($attributes[$this->_getAttrKey($attr, $attributes)]);
        }
    } //end func _removeAttr

    /**
     * Returns the value of the given attribute
     * 
     * @param     string    $attr   Attribute name
     * @since     1.5
     * @access    public
     * @return    void
     * @throws    
     */
    function getAttribute($attr)
    {
        if (is_array($this->_attributes)) {
            while (list($key, $value) = each($this->_attributes)) {
                if (is_int($key) && strtoupper($value) == strtoupper($attr)) {
                    return true;
                } elseif (strtolower($key) == strtolower($attr)){
                    return $value;
                }
            }
        }
        return false;
    } //end func getAttribute

    /**
     * Sets the HTML attributes
     * @param    mixed   $attributes     Either a typical HTML attribute string or an associative array
     * @access   public
     */
    function setAttributes($attributes) 
    {
        $this->_attributes = $this->_parseAttributes($attributes);
    } // end func _setAttributes

    /**
     * Updates the passed attributes without changing the other existing attributes
     * @param    mixed   $attributes     Either a typical HTML attribute string or an associative array
     * @access   public
     */
    function updateAttributes($attributes)
    {
        $attributes = $this->_parseAttributes($attributes);
        $this->_updateAttrArray($this->_attributes, $attributes);
    } // end func updateAttributes

    /**
     * Removes an attribute from 
     * 
     * @param     string    $attr   Attribute name
     * @since     1.4
     * @access    public
     * @return    void
     * @throws    
     */
    function removeAttribute($attr)
    {
        $this->_removeAttr($attr, $this->_attributes);
    } //end func removeAttribute

    /**
     * Sets the tab offset
     * @param    int     $offset
     * @access   public
     */
    function setTabOffset($offset)
    {
        $this->_tabOffset = $offset;
    } // end func setTabOffset
    
    /**
     * Returns the tabOffset
     * 
     * @since     1.5
     * @access    public
     * @return    void
     * @throws    
     */
    function getTabOffset()
    {
        return $this->_tabOffset;
    } //end func getTabOffset

    /**
     * Sets the HTML comment to be displayed at the beginning of the HTML string
     *
     * @param     string
     * @since     1.4
     * @access    public
     * @return    void
     * @throws    
     */
    function setComment($comment)
    {
        $this->_comment = $comment;
    } // end func setHtmlComment
    
    /**
     * Returns the HTML comment
     * 
     * @since     1.5
     * @access    public
     * @return    void
     * @throws    
     */
    function getComment()
    {
        return $this->_comment;
    } //end func getComment

    /**
     * Abstract method.  Must be extended to return the objects HTML
     *
     * @access    public
     * @return    string
     * @abstract    
     */
    function toHtml()
    {
        return "";
    } // end func toHtml

    /**
     * Displays the HTML to the screen
     *
     * @access    public
     */
    function display()
    {
        print $this->toHtml();
    } // end func display

} // end class HTML_Common
?>
