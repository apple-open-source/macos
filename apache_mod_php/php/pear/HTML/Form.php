<?php
//
// +----------------------------------------------------------------------+
// | PHP version 4.0                                                      |
// +----------------------------------------------------------------------+
// | Copyright (c) 1997-2001 The PHP Group                                |
// +----------------------------------------------------------------------+
// | This source file is subject to version 2.0 of the PHP license,       |
// | that is bundled with this package in the file LICENSE, and is        |
// | available at through the world-wide-web at                           |
// | http://www.php.net/license/2_02.txt.                                 |
// | If you did not receive a copy of the PHP license and are unable to   |
// | obtain it through the world-wide-web, please send a note to          |
// | license@php.net so we can mail you a copy immediately.               |
// +----------------------------------------------------------------------+
// | Authors: Stig Bakken <ssb@fast.no>                                   |
// |          Urs Gehrig <urs@circle.ch>                                  |
// +----------------------------------------------------------------------+
//
// $Id: Form.php,v 1.1.1.4 2001/12/14 22:14:52 zarzycki Exp $
//
// HTML form utility functions.
//

if (!defined('HTML_FORM_TEXT_SIZE')) {
    define('HTML_FORM_TEXT_SIZE', 20);
}

if (!defined('HTML_FORM_MAX_FILE_SIZE')) {
    define('HTML_FORM_MAX_FILE_SIZE', 1048576); // 1 MB
}

if (!defined('HTML_FORM_PASSWD_SIZE')) {
    define('HTML_FORM_PASSWD_SIZE', 8);
}

class HTML_Form
{
    // {{{ properties

    /** ACTION attribute of <FORM> tag */
    var $action;

    /** METHOD attribute of <FORM> tag */
    var $method;

    /** NAME attribute of <FORM> tag */
    var $name;

    /** an array of entries for this form */
    var $fields;

    /** DB_storage object, if tied to one */
    var $storageObject;

    /** <FORM ENCODING=""> attribute value */
    var $encoding;

    /** TARGET attribute of <FORM> tag */
    var $target;

    /** ENCTYPE attribute of <FORM> tag */
    var $enctype;

    // }}}

    // {{{ constructor

    function HTML_Form($action, $method = 'GET', $name = '', $target = '', $enctype = '')
    {
        $this->action = $action;
        $this->method = $method;
        $this->name = $name;
        $this->fields = array();
        $this->encoding = '';
        $this->target = $target;
        $this->enctype = $enctype;
    }

    // }}}

    // {{{ addText()

    function addText($name, $title, $default = '',
                     $size = HTML_FORM_TEXT_SIZE, $maxlength = '')
    {
        $this->fields[] = array("text", $name, $title, $default, $size, $maxlength);
    }

    // }}}
    // {{{ addPassword()

    function addPassword($name, $title, $default, $size = HTML_FORM_PASSWD_SIZE)
    {
        $this->fields[] = array("password", $name, $title, $default, $size);
    }

    // }}}
    // {{{ addCheckbox()

    function addCheckbox($name, $title, $default)
    {
        $this->fields[] = array("checkbox", $name, $title, $default);
    }

    // }}}
    // {{{ addTextarea()

    function addTextarea($name, $title, $default,
                         $width = HTML_FORM_TEXTAREA_WT,
                         $height = HTML_FORM_TEXTAREA_HT, $maxlength = '')
    {
        $this->fields[] = array("textarea", $name, $title, $default, $width, $height, $maxlength);
    }

    // }}}
    // {{{ addSubmit()

    function addSubmit($name = "submit", $title = "Submit Changes")
    {
        $this->fields[] = array("submit", $name, $title);
    }

    // }}}
    // {{{ addReset()

    function addReset($title = "Discard Changes")
    {
        $this->fields[] = array("reset", $title);
    }

    // }}}
    // {{{ addSelect()

    function addSelect($name, $title, $entries, $default = '', $size = 1,
                       $blank = '', $multiple = false, $attribs = '')
    {
        $this->fields[] = array("select", $name, $title, $entries, $default,
                                $size, $blank, $multiple, $attribs);
    }

    // }}}
    // {{{ addRadio()

    function addRadio($name, $title, $value, $default = false)
    {
        $this->fields[] = array("radio", $name, $title, $value, $default);
    }

    // }}}
    // {{{ addImage()

    function addImage($name, $src)
    {
        $this->fields[] = array("image", $name, $src);
    }

    // }}}
    // {{{ addHidden()

    function addHidden($name, $value)
    {
        $this->fields[] = array("hidden", $name, $value);
    }

    // }}}

    // {{{ adding misssing addBlank function

    function addBlank($i,$title = '')
    {
        $this->fields[] = array("blank", $i, $title);
    }

    // {{{ addFile

    function addFile($name, $title, $maxsize = HTML_FORM_MAX_FILE_SIZE,
                     $size = HTML_FORM_TEXT_SIZE, $accept = '') 
    {
        $this->fields[] = array("file", $name, $title, $maxsize, $size, $accept);
    }

    // {{{ start()

    function start()
    {
        print "<FORM ACTION=\"" . basename($this->action) . "\" METHOD=\"$this->method\"";
        if ($this->name) {
            print " NAME=\"$this->name\"";
        }
        if ($this->target) {
            print " TARGET=\"$this->target\"";
        }
        if ($this->enctype) {
            print " ENCTYPE=\"$this->enctype\"";
        }
        print ">\n";
    }

    // }}}
    // {{{ end()

    function end()
    {
        $fields = array();
        reset($this->fields);
        while (list($i, $data) = each($this->fields)) {
            if ($data[0] == 'reset') {
                continue;
            }
            $fields[$data[1]] = true;
        }
        $this->displayHidden("_fields", implode(":", array_keys($fields)));
        print "</FORM>";
    }

    // }}}

    // {{{ displayText()

    function displayText($name, $default = '',
                         $size = HTML_FORM_TEXT_SIZE, $maxlength = '')
    {
        if (!$maxlength) {
            print "<INPUT NAME=\"$name\" VALUE=\"$default\" SIZE=\"$size\"";
        } else {
            print "<INPUT NAME=\"$name\" VALUE=\"$default\" SIZE=\"$size\" MAXLENGTH=\"$maxlength\"";
        }
        print ">";
    }

    // }}}
    // {{{ displayTextRow()

    function displayTextRow($name, $title, $default = '',
                            $size = HTML_FORM_TEXT_SIZE, $maxlength = '')
    {
        print " <TR>\n";
        print "  <TH ALIGN=\"right\">$title</TH>";
        print "  <TD>";
        $this->displayText($name, $default, $size, $maxlength);
        print "</TD>\n";
        print " </TR>\n";
    }

    // }}}
    // {{{ displayPassword()

    function displayPassword($name, $default = '', $size = HTML_FORM_PASSWD_SIZE)
    {
        print "<input name=\"$name\" type=\"password\" value=\"$default\" size=\"$size\" />";
    }

    // }}}
    // {{{ displayPasswordRow()

    function displayPasswordRow($name, $title, $default = '', $size = HTML_FORM_PASSWD_SIZE)
    {
        print "<tr>\n";
        print "  <th align=\"right\">$title:</th>\n";
        print "  <td>";
        $this->displayPassword($name, $default, $size);
        print " repeat: ";
        $this->displayPassword($name."2", null, $size);
        print "</td>\n";
        print "</tr>\n";
    }

    // }}}
    // {{{ displayCheckbox()

    function displayCheckbox($name, $default = false)
    {
        print "<input type=\"checkbox\" name=\"$name\"";
        if ($default && $default != 'off') {
            print " CHECKED";
        }
        print " />";
    }

    // }}}
    // {{{ displayCheckboxRow()

    function displayCheckboxRow($name, $title, $default = false)
    {
        print " <tr>\n";
        print "  <th align=\"right\">$title</th>";
        print "  <td>";
        $this->displayCheckbox($name, $default);
        print "</td>\n";
        print " </tr>\n";
    }

    // }}}
    // {{{ displayTextarea()

    function displayTextarea($name, $default = '', $width = 40,
                             $height = 5, $maxlength  = '')
    {
        if (!$maxlength) {
            print "<TEXTAREA NAME=\"$name\" COLS=\"$width\" ROWS=\"$height\"";
        } else {
            print "<TEXTAREA NAME=\"$name\" COLS=\"$width\" ROWS=\"$height\" MAXLENGTH=\"$maxlength\"";
        }
        print ">";
        print $default;
        print "</TEXTAREA>";
    }

    // }}}
    // {{{ displayTextareaRow()

    function displayTextareaRow($name, $title, $default = '', $width = 40,
                                $height = 5, $maxlength = '')
    {
        print " <TR>\n";
        print "  <TH ALIGN=\"right\" VALIGN=\"top\">$title</TH>\n";
        print "  <TD>";
        $this->displayTextarea($name, &$default, $width, $height, $maxlength);
        print "</TD>\n";
        print " </TR>\n";
    }

    // }}}
    // {{{ displaySubmit()

    function displaySubmit($title = 'Submit Changes', $name = "submit")
    {
        print $this->returnSubmit($title, $name);
    }

    // }}}
    // {{{ displaySubmitRow()

    function displaySubmitRow($name = "submit", $title = 'Submit Changes')
    {
        print $this->returnSubmitRow($name, $title);
    }

    // }}}
    // {{{ displayReset()

    function displayReset($title = 'Clear contents')
    {
        print $this->returnReset($title);
    }

    // }}}
    // {{{ displayResetRow()

    function displayResetRow($title = 'Clear contents')
    {
        print $this->returnResetRow($title);
    }

    // }}}
    // {{{ displaySelect()

    function displaySelect($name, $entries, $default = '', $size = 1,
                           $blank = '', $multiple = false, $attribs = '')
    {
        print $this->returnSelect($name, $entries, $default, $size, $blank,
                                  $multiple, $attribs);
    }

    // }}}
    // {{{ displaySelectRow()

    function displaySelectRow($name, $title, &$entries, $default = '',
                              $size = 1, $blank = '', $multiple = false, $attribs = '')
    {
        print $this->returnSelectRow($name, $title, $entries, $default, $size,
                                     $blank, $multiple, $attribs);
    }

    // }}}
    // {{{ displayHidden()

    function displayHidden($name, $value)
    {
        print $this->returnHidden($name, $value);
    }

    // }}}

    // assuming that $default is the 'checked' attribut of the radio tag

    // {{{ displayRadio()

    function displayRadio($name, $value, $default = false)
    {
        if ($default == false) {
            print "<INPUT type='radio' name=\"$name\" value=\"$value\">";
        } else {
            print "<INPUT type='radio' name=\"$name\" checked value=\"$value\">";
        }
    }

    // }}}displayRadio()
    // {{{ displayRadioRow()

    function displayRadioRow($name, $title, $value, $default = false)
    {
        print " <TR>\n";
        print "<TH ALIGN=\"right\">$title</TH>";
        print "  <TD>";
        $this->displayRadio($name, $value, $default);
        print "</TD>\n";
        print " </TR>\n";
    }

    // }}}displayRadioRow()
    // {{{ displayBlank()

    function displayBlank()
    {
        print "&nbsp;";
    }


    // {{{ displayBlankRow()

    function displayBlankRow($i, $title= '')
    {
        if (!$title) {
            for ($j = 0;$j < $i;$j++) {
                print " <TR>\n";
                print "  <TH ALIGN=\"right\">&nbsp;</TH>";
                print "  <TD>";
                $this->displayBlank();
                print "</TD>\n";
                print " </TR>\n";
            }
        } else {
            print " <TR>\n";
            print "  <TH ALIGN=\"right\">$title</TH>";
            print "  <TD>";
            $this->displayBlank();
            print "</TD>\n";
            print " </TR>\n";
        }
    }

    // }}}displayBlankRow()
    // {{{ displayFile()

    function displayFile($name, $maxsize = HTML_FORM_MAX_FILE_SIZE,
                         $size = HTML_FORM_TEXT_SIZE, $accept = '')
    {
        print "<INPUT TYPE=\"file\" NAME=\"$name\" MAXSIZE=\"$maxsize\" SIZE=\"$size\"";
        if ($accept) {
            print " ACCEPT=\"$accept\"";
        }
        print ">";

    }

    // }}}displayFile()

    // {{{ displayFileRow()

    function displayFileRow($name, $title, $maxsize = HTML_FORM_MAX_FILE_SIZE,
                            $size = HTML_FORM_TEXT_SIZE, $accept = '')
    {
        print " <TR>\n";
        print "  <TH ALIGN=\"right\">$title</TH>";
        print "  <TD>";
        $this->displayFile($name, $maxsize, $size, $accept);
        print "</TD>\n";
        print " </TR>\n";
    }

    // }}}displayFileRow()


    // {{{ returnText()

    function returnText($name, $default = '', $size = HTML_FORM_TEXT_SIZE)
    {
        return "<input name=\"$name\" value=\"$default\" size=\"$size\" />";
    }

    // }}}
    // {{{ returnTextRow()

    function returnTextRow($name, $title, $default = '', $size = HTML_FORM_TEXT_SIZE)
    {
        $str  = " <tr>\n";
        $str .= "  <th align=\"right\">$title:</th>";
        $str .= "  <td>";
        $str .= $this->returnText($name, $default, $size);
        $str .= "</td>\n";
        $str .= " </tr>\n";

        return $str;
    }

    // }}}
    // {{{ returnPassword()

    function returnPassword($name, $default = '', $size = HTML_FORM_PASSWD_SIZE)
    {
        return "<input name=\"$name\" type=\"password\" value=\"$default\" size=\"$size\" />";
    }

    // }}}
    // {{{ returnPasswordRow()

    function returnPasswordRow($name, $title, $default = '', $size = HTML_FORM_PASSWD_SIZE)
    {
        $str  = "<tr>\n";
        $str .= "  <th align=\"right\">$title:</th>\n";
        $str .= "  <td>";
        $str .= $this->returnPassword($name, $default, $size);
        $str .= " repeat: ";
        $str .= $this->returnPassword($name."2", $default, $size);
        $str .= "</td>\n";
        $str .= "</tr>\n";

        return $str;
    }

    // }}}
    // {{{ returnCheckbox()

    function returnCheckbox($name, $default = false)
    {
        $str = "<input type=\"checkbox\" name=\"$name\"";
        if ($default && $default != 'off') {
            $str .= " checked";
        }
        $str .= " />";

        return $str;
    }

    // }}}
    // {{{ returnCheckboxRow()

    function returnCheckboxRow($name, $title, $default = false)
    {
        $str  = " <tr>\n";
        $str .= "  <th align=\"right\">$title:</th>\n";
        $str .= "  <td>";
        $str .= $this->returnCheckbox($name, $default);
        $str .= "</td>\n";
        $str .= " </tr>\n";

        return $str;
    }

    // }}}
    // {{{ returnTextarea()

    function returnTextarea($name, $default = '', $width = 40, $height = 5)
    {
        $str  = "<textarea name=\"$name\" cols=\"$width\" rows=\"$height\">";
        $str .= $default;
        $str .= "</textarea>";

        return $str;
    }

    // }}}
    // {{{ returnTextareaRow()

    function returnTextareaRow($name, $title, $default = '', $width = 40, $height = 5)
    {
        $str  = " <tr>\n";
        $str .= "  <th align=\"right\">$title:</th>\n";
        $str .= "  <td>";
        $str .= $this->returnTextarea($name, $default, $width, $height);
        $str .= "</td>\n";
        $str .= " </tr>\n";

        return $str;
    }

    // }}}
    // {{{ returnSubmit()

    function returnSubmit($title = 'Submit Changes', $name = "submit")
    {
        return "<input name=\"$name\" type=\"submit\" value=\"$title\" />";
    }

    // }}}
    // {{{ returnSubmitRow()

    function returnSubmitRow($name = "submit", $title = 'Submit Changes')
    {
        $str  = " <tr>\n";
        $str .= "  <td>&nbsp;</td>\n";
        $str .= "  <td>";
        $str .= $this->returnSubmit($title, $name);
        $str .= "</td>\n";
        $str .= " </tr>\n";

        return $str;
    }

    // }}}
    // {{{ returnReset()

    function returnReset($title = 'Clear contents')
    {
        return "<input type=\"reset\" value=\"$title\" />";
    }

    // }}}
    // {{{ returnResetRow()

    function returnResetRow($title = 'Clear contents')
    {
        $str  = " <tr>\n";
        $str .= "  <td>&nbsp;</td>\n";
        $str .= "  <td>";
        $str .= $this->returnReset($title);
        $str .= "</td>\n";
        $str .= " </tr>\n";

        return $str;
    }

    // }}}
    // {{{ returnSelect()

    function returnSelect($name, $entries, $default = '', $size = 1,
                           $blank = '', $multiple = false, $attrib = '')
    {
        if ($multiple && substr($name, -2) != "[]") {
            $name .= "[]";
        }
        $str = "   <select name=\"$name\"";
        if ($size) {
            $str .= " size=\"$size\"";
        }
        if ($multiple) {
            $str .= " multiple";
        }
        if ($attrib) {
            $str .= " $attrib";
        }
        $str .= ">\n";
        if ($blank) {
            $str .= "    <option value=\"\">$blank</option>\n";
        }
        while (list($val, $text) = each($entries)) {
            $str .= '    <option ';
                if ($default) {
                    if ($multiple && is_array($default)) {
                        if ((is_string(key($default)) && $default[$val]) ||
                            (is_int(key($default)) && in_array($val, $default))) {
                            $str .= 'selected ';
                        }
                    } elseif ($default == $val) {
                        $str .= 'selected ';
                    }
                }
            $str .= "value=\"$val\">$text</option>\n";
        }
        $str .= "   </select>\n";

        return $str;
    }

    // }}}
    // {{{ returnSelectRow()

    function returnSelectRow($name, $title, &$entries, $default = '', $size = 1,
                              $blank = '', $multiple = false, $attribs = '')
    {
        $str  = " <tr>\n";
        $str .= "  <th align=\"right\">$title:</th>\n";
        $str .= "  <td>\n";
        $str .= $this->returnSelect($name, $entries, $default, $size, $blank, $multiple, $attribs);
        $str .= "  </td>\n";
        $str .= " </tr>\n";

        return $str;
    }

    // }}}
    // {{{ returnHidden()

    function returnHidden($name, $value)
    {
        return "<input type=\"hidden\" name=\"$name\" value=\"$value\" />";
    }

    // }}}
    // {{{ returnFile()

    function returnFile($name = 'userfile',
                        $maxsize = HTML_FORM_MAX_FILE_SIZE,
                        $size = HTML_FORM_TEXT_SIZE)
    {
        $str  = " <input type=\"hidden\" name=\"MAX_FILE_SIZE\" value=\"$maxsize\" />";
        $str .= " <input type=\"file\" name=\"$name\" size=\"$size\" />";
        return $str;
    }

    // }}}
    // {{{ returnMultipleFiles()

    function returnMultipleFiles($name = 'userfile[]',
                                 $maxsize = HTML_FORM_MAX_FILE_SIZE,
                                 $files = 3,
                                 $size = HTML_FORM_TEXT_SIZE)
    {
        $str  = " <input type=\"hidden\" name=\"MAX_FILE_SIZE\" value=\"$maxsize\" />";
        for($i=0; $i < $files; $i++) {
           $str .= " <input type=\"file\" name=\"$name\" size=\"$size\" /><br />";
        }
        return $str;
    }

    // }}}
    // {{{ returnStart()

    function returnStart($multipartformdata = false)
    {
        $str = "<form action=\"" . basename ($this->action) . "\" method=\"$this->method\"";
        if ($this->name) {
            $str .= " name=\"$this->name\"";
        }
        if ($multipartformdata) {
            $str .= " enctype=\"multipart/form-data\"";
        }
        $str .= ">";

        return $str;
    }

    // }}}
    // {{{ returnEnd()

    function returnEnd()
    {
        $fields = array();
        reset($this->fields);
        while (list($i, $data) = each($this->fields)) {
            if ($data[0] == 'reset') {
                continue;
            }
            $fields[$data[1]] = true;
        }
        $ret = $this->returnHidden("_fields", implode(":", array_keys($fields)));
        $ret .= "</form>";
        return $ret;
    }

    // }}}

    // {{{ display()

    function display()
    {
        $arrname = 'HTTP_'.strtoupper($this->method).'_VARS';
        $arr = &$GLOBALS[$arrname];
        $this->start();
        print "<table>\n";
        reset($this->fields);
        $hidden = array();
        foreach ($this->fields as $i => $data) {
            switch ($data[0]) {
                case "hidden":
                    $hidden[] = $i;
                    $defind = 0;
                    continue 2;
                case "reset":
                    $params = 1;
                    $defind = 0;
                    break;
                case "submit":
                case "blank": // new
                    $params = 2;
                    $defind = 0;
                    break;
                case "image":
                    $params = 2;
                    $defind = 0;
                    break;
                case "checkbox":
                    $params = 3;
                    $defind = 2;
                    break;
                case "file":  //new
                case "text":
                    $params = 5;
                    $defind = 3;
                    break;
                case "password":
                case "radio":
                    $params = 4;
                    $defind = 3;
                    break;
                case "textarea":
                    $params = 6;
                    $defind = 3;
                    break;
                case "select":
                    $params = 8;
                    $defind = 4;
                    break;
                default:
                    // unknown field type
                    continue 2;
            }
            $str = '$this->display'.ucfirst($data[0])."Row(";
            for ($i = 1;$i <= $params;$i++) {
                if ($i == $defind && $data[$defind] === null && isset($arr[$data[1]])) {
                    $str .= "\$arr['$data[1]']";
                } else {
                    $str .= '$'."data[$i]";
                }
                if ($i < $params) $str .= ', ';
            }
            $str .= ');';
            eval($str);
        }
        print "</table>\n";
        for ($i = 0;$i < sizeof($hidden);$i++) {
            $this->displayHidden($this->fields[$hidden[$i]][1],
                                 $this->fields[$hidden[$i]][2]);
        }
        $this->end();
    }

    // }}}
}

/*
* Local variables:
* tab-width: 4
* c-basic-offset: 4
* End:
*/
?>
