<?php
//
// +----------------------------------------------------------------------+
// | PHP version 4.0                                                      |
// +----------------------------------------------------------------------+
// | Copyright (c) 1997, 1998, 1999, 2000 The PHP Group                   |
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
// $Id: Form.php,v 1.1.1.2 2001/01/25 05:00:30 wsanchez Exp $
//
// HTML form utility functions.
//

if (!defined('HTML_FORM_TEXT_SIZE')) {
    define('HTML_FORM_TEXT_SIZE', 20);
}

if (!defined('HTML_FORM_MAX_FILE_SIZE')) {
    define('HTML_FORM_MAX_FILE_SIZE', 1048576); // 1 MB
}

class HTML_Form {
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

    // }}}

    // {{{ constructor

    function HTML_Form($action, $method = 'GET', $name = '', $target = '') {
		$this->action = $action;
		$this->method = $method;
		$this->name = $name;
		$this->fields = array();
		$this->encoding = '';
		$this->target = $target;
    }

    // }}}

    // {{{ addText()

    function addText($name, $title, $default, $size = HTML_FORM_TEXT_SIZE) {
        $this->fields[] = array("text", $name, $title, $default, $size);
    }

    // }}}
    // {{{ addPassword()

    function addPassword($name, $title, $default, $size = HTML_FORM_PASSWD_SIZE) {
        $this->fields[] = array("password", $name, $title, $default, $size);
    }

    // }}}
    // {{{ addCheckbox()

    function addCheckbox($name, $title, $default) {
        $this->fields[] = array("checkbox", $name, $title, $default);
    }

    // }}}
    // {{{ addTextarea()

    function addTextarea($name, $title, $default,
                         $width = HTML_FORM_TEXTAREA_WT,
                         $height = HTML_FORM_TEXTAREA_HT) {
        $this->fields[] = array("textarea", $name, $title, &$default, $width, $height);
    }

    // }}}
    // {{{ addSubmit

    function addSubmit($name = "submit", $title = "Submit Changes") {
        $this->fields[] = array("submit", $name, $title);
    }

    // }}}
    // {{{ addReset()

    function addReset($title = "Discard Changes") {
        $this->fields[] = array("reset", $title);
    }

    // }}}
    // {{{ addSelect()

    function addSelect($name, $title, $entries, $default = '', $size = 1,
                       $blank = '', $multiple = false, $attribs = '') {
        $this->fields[] = array("select", $name, $title, &$entries, $default,
				$size, $blank, $multiple, $attribs);
    }

    // }}}
    // {{{ addRadio()

    function addRadio($name, $title, $value, $default) {
        $this->fields[] = array("radio", $name, $title, $value, $default);
    }

    // }}}
    // {{{ addImage()

    function addImage($name, $src) {
        $this->fields[] = array("image", $name, $src);
    }

    // }}}
    // {{{ addHidden()

    function addHidden($name, $value) {
        $this->fields[] = array("hidden", $name, $value);
    }

    // }}}

    // {{{ start()

    function start() {
        print "<FORM ACTION=\"" . basename($this->action) . "\" METHOD=\"$this->method\"";
        if ($this->name) {
            print " NAME=\"$this->name\"";
        }
	if ($this->target) {
	    print " TARGET=\"$this->target\"";
	}
        print ">";
    }

    // }}}
    // {{{ end()

    function end() {
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

    function displayText($name, $default = '', $size = HTML_FORM_TEXT_SIZE) {
        print "<INPUT NAME=\"$name\" VALUE=\"$default\" SIZE=\"$size\">";
    }

    // }}}
    // {{{ displayTextRow()

    function displayTextRow($name, $title, $default = '', $size = HTML_FORM_TEXT_SIZE) {
        print " <TR>\n";
        print "  <TH ALIGN=\"right\">$title</TH>";
        print "  <TD>";
        $this->displayText($name, $default, $size);
        print "</TD>\n";
        print " </TR>\n";
    }

    // }}}
    // {{{ displayPassword()

    function displayPassword($name, $default = '', $size = HTML_FORM_PASSWD_SIZE) {
        print "<INPUT NAME=\"$name\" TYPE=\"password\" VALUE=\"$default\" SIZE=\"$size\">";
    }

    // }}}
    // {{{ displayPasswordRow()

    function displayPasswordRow($name, $title, $default = '', $size = HTML_FORM_PASSWD_SIZE) {
        print "<TR>\n";
        print "  <TH ALIGN=\"right\">$title</TH>\n";
        print "  <TD>";
        $this->displayPassword($name, $default, $size);
        print " repeat: ";
        $this->displayPassword($name."2", $default, $size);
        print "</TD>\n";
        print "</TR>\n";
    }

    // }}}
    // {{{ displayCheckbox()

    function displayCheckbox($name, $default = false) {
        print "<INPUT TYPE=\"checkbox\" NAME=\"$name\"";
        if ($default && $default != 'off') {
            print " CHECKED";
        }
        print ">";
    }

    // }}}
    // {{{ displayCheckboxRow()

    function displayCheckboxRow($name, $title, $default = false) {
        print " <TR>\n";
        print "  <TH ALIGN=\"left\">$title</TH>";
        print "  <TD>";
        $this->displayCheckbox($name, $default);
        print "</TD>\n";
        print " </TR>\n";
    }

    // }}}
    // {{{ displayTextarea()

    function displayTextarea($name, $default = '', $width = 40, $height = 5) {
        print "<TEXTAREA NAME=\"$name\" COLS=\"$width\" ROWS=\"$height\">";
        print $default;
        print "</TEXTAREA>";
    }

    // }}}
    // {{{ displayTextareaRow()

    function displayTextareaRow($name, $title, $default = '', $width = 40, $height = 5) {
        print " <TR>\n";
        print "  <TH ALIGN=\"right\">$title</TH>\n";
        print "  <TD>";
        $this->displayTextarea($name, &$default, $width, $height);
        print "</TD>\n";
        print " </TR>\n";
    }

    // }}}
    // {{{ displaySubmit()

    function displaySubmit($title = 'Submit Changes', $name = "submit") {
	print $this->returnSubmit($title, $name);
    }

    // }}}
    // {{{ displaySubmitRow()

    function displaySubmitRow($name = "submit", $title = 'Submit Changes') {
    print $this->returnSubmitRow($name, $title);
    }

    // }}}
    // {{{ displayReset()

    function displayReset($title = 'Clear contents') {
        print $this->returnReset($title);
    }

    // }}}
    // {{{ displayResetRow()

    function displayReset($title = 'Clear contents') {
        print $this->returnReset($title);
    }

    // }}}
    // {{{ displaySelect()

    function displaySelect($name, $entries, $default = '', $size = 1,
                           $blank = '', $multiple = false, $attribs = '') {
		print $this->returnSelect($name, $entries, $default, $size, $blank,
								  $multiple, $attribs);
    }

    // }}}
    // {{{ displaySelectRow()

    function displaySelectRow($name, $title, &$entries, $default = '',
                  $size = 1, $blank = '', $multiple = false)
    {
    print $this->returnSelectRow($name, $title, $entries, $default, $size,
                     $blank, $multiple);
    }

    // }}}
    // {{{ displayHidden()

    function displayHidden($name, $value) {
    print $this->returnHidden($name, $value);
    }

    // }}}

    // XXX missing: displayRadio displayRadioRow

    // {{{ returnText()

    function returnText($name, $default = '', $size = HTML_FORM_TEXT_SIZE) {
        return "<INPUT NAME=\"$name\" VALUE=\"$default\" SIZE=\"$size\">";
    }

    // }}}
    // {{{ returnTextRow()

    function returnTextRow($name, $title, $default = '', $size = HTML_FORM_TEXT_SIZE) {
        $str .= " <TR>\n";
        $str .= "  <TH ALIGN=\"right\">$title</TH>";
        $str .= "  <TD>";
        $str .= $this->returnText($name, $default, $size);
        $str .= "</TD>\n";
        $str .= " </TR>\n";

        return $str;
    }

    // }}}
    // {{{ returnPassword()

    function returnPassword($name, $default = '', $size = HTML_FORM_PASSWD_SIZE) {
        return "<INPUT NAME=\"$name\" TYPE=\"password\" VALUE=\"$default\" SIZE=\"$size\">";
    }

    // }}}
    // {{{ returnPasswordRow()

    function returnPasswordRow($name, $title, $default = '', $size = HTML_FORM_PASSWD_SIZE) {
        $str .= "<TR>\n";
        $str .= "  <TH ALIGN=\"right\">$title</TH>\n";
        $str .= "  <TD>";
        $str .= $this->returnPassword($name, $default, $size);
        $str .= " repeat: ";
        $str .= $this->returnPassword($name."2", $default, $size);
        $str .= "</TD>\n";
        $str .= "</TR>\n";

        return $str;
    }

    // }}}
    // {{{ returnCheckbox()

    function returnCheckbox($name, $default = false) {
        $str .= "<INPUT TYPE=\"checkbox\" NAME=\"$name\"";
        if ($default && $default != 'off') {
            $str .= " CHECKED";
        }
        $str .= ">";

        return $str;
    }

    // }}}
    // {{{ returnCheckboxRow()

    function returnCheckboxRow($name, $title, $default = false) {
        $str .= " <TR>\n";
        $str .= "  <TH ALIGN=\"right\">$title</TH>\n";
        $str .= "  <TD>";
        $str .= $this->returnCheckbox($name, $default);
        $str .= "</TD>\n";
        $str .= " </TR>\n";

        return $str;
    }

    // }}}
    // {{{ returnTextarea()

    function returnTextarea($name, $default = '', $width = 40, $height = 5) {
        $str .= "<TEXTAREA NAME=\"$name\" COLS=\"$width\" ROWS=\"$height\">";
        $str .= $default;
        $str .= "</TEXTAREA>";

        return $str;
    }

    // }}}
    // {{{ returnTextareaRow()

    function returnTextareaRow($name, $title, $default = '', $width = 40, $height = 5) {
        $str .= " <TR>\n";
        $str .= "  <TH ALIGN=\"right\">$title</TH>\n";
        $str .= "  <TD>";
        $str .= $this->returnTextarea($name, &$default, $width, $height);
        $str .= "</TD>\n";
        $str .= " </TR>\n";

        return $str;
    }

    // }}}
    // {{{ returnSubmit()

    function returnSubmit($title = 'Submit Changes', $name = "submit") {
        return "<INPUT NAME=\"$name\" TYPE=\"submit\" VALUE=\"$title\">";
    }

    // }}}
    // {{{ returnSubmitRow()

    function returnSubmitRow($name = "submit", $title = 'Submit Changes') {
        $str .= " <TR>\n";
        $str .= "  <TD>&nbsp</TD>\n";
        $str .= "  <TD>";
        $str .= $this->returnSubmit($title, $name);
        $str .= "</TD>\n";
        $str .= " </TR>\n";

        return $str;
    }

    // }}}
    // {{{ returnReset()

    function returnReset($title = 'Clear contents') {
        return "<INPUT TYPE=\"reset\" VALUE=\"$title\">";
    }

    // }}}
    // {{{ returnResetRow()

    function returnResetRow($title = 'Clear contents') {
        $str .= " <TR>\n";
        $str .= "  <TD>&nbsp</TD>\n";
        $str .= "  <TD>";
        $str .= $this->returnReset($title);
        $str .= "</TD>\n";
        $str .= " </TR>\n";

        return $str;
    }

    // }}}
    // {{{ returnSelect()

    function returnSelect($name, $entries, $default = '', $size = 1,
                           $blank = '', $multiple = false, $attrib = '') {
		if ($multiple && substr($name, -2) != "[]") {
			$name .= "[]";
		}
        $str .= "   <SELECT NAME=\"$name\"";
        if ($size) {
            $str .= " SIZE=\"$size\"";
        }
        if ($multiple) {
            $str .= " MULTIPLE";
        }
		if ($attrib) {
			$str .= " $attrib";
		}
        $str .= ">\n";
        if ($blank) {
            $str .= "    <OPTION VALUE=\"\">$blank\n";
        }
        while (list($val, $text) = each($entries)) {
            $str .= '    <OPTION ';
			if ($default) {
				if ($multiple && is_array($default)) {
					if ((is_string(key($default)) && $default[$val]) ||
						(is_int(key($default)) && in_array($val, $default))) {
						$str .= 'SELECTED ';
					}
				} elseif ($default == $val) {
					$str .= 'SELECTED ';
				}
			}
            $str .= "VALUE=\"$val\">$text\n";
        }
        $str .= "   </SELECT>\n";

        return $str;
    }

    // }}}
    // {{{ returnSelectRow()

    function returnSelectRow($name, $title, &$entries, $default = '', $size = 1,
                              $blank = '', $multiple = false)
    {
        $str .= " <TR>\n";
        $str .= "  <TH ALIGN=\"right\">$title:</TH>\n";
        $str .= "  <TD>\n";
        $str .= $this->returnSelect($name, &$entries, $default, $size, $blank, $multiple);
        $str .= "  </TD>\n";
        $str .= " </TR>\n";

        return $str;
    }

    // }}}
    // {{{ returnHidden()

    function returnHidden($name, $value) {
        return "<INPUT TYPE=\"hidden\" NAME=\"$name\" VALUE=\"$value\">";
    }

    // }}}
    // {{{ returnFile()

    function returnFile($name = 'userfile', $maxsize = HTML_FORM_MAX_FILE_SIZE, $size = HTML_FORM_TEXT_SIZE) {
        $str .= " <INPUT TYPE=\"hidden\" NAME=\"MAX_FILE_SIZE\" VALUE=\"$maxsize\">";
        $str .= " <INPUT TYPE=\"file\" NAME=\"$name\" SIZE=\"$size\">";
        return $str;
    }

    // }}}
    // {{{ returnMultipleFiles()

    function returnMultipleFiles($name = 'userfile[]', $maxsize = HTML_FORM_MAX_FILE_SIZE, $files = 3, $size = HTML_FORM_TEXT_SIZE) {
        $str .= " <INPUT TYPE=\"hidden\" NAME=\"MAX_FILE_SIZE\" VALUE=\"$maxsize\">";
        for($i=0; $i<$files; $i++) {
           $str .= " <INPUT TYPE=\"file\" NAME=\"$name\" SIZE=\"$size\"><br>";
        }
        return $str;
    }

    // }}}
    // {{{ returnStart()

    function returnStart($multipartformdata = false) {
        $str .= "<FORM ACTION=\"" . basename ($this->action) . "\" METHOD=\"$this->method\"";
        if ($this->name) {
            $str .= " NAME=\"$this->name\"";
        }
        if ($multipartformdata) {
            $str .= " ENCTYPE=\"multipart/form-data\"";
        }
        $str .= ">";

        return $str;
    }

    // }}}
    // {{{ returnEnd()

    function returnEnd() {
        $fields = array();
        reset($this->fields);
        while (list($i, $data) = each($this->fields)) {
            if ($data[0] == 'reset') {
                continue;
            }
            $fields[$data[1]] = true;
        }
    $ret = $this->returnHidden("_fields", implode(":", array_keys($fields)));
    $ret .= "</FORM>";
    return $ret;
    }

    // }}}

    // {{{ display()

    function display() {
        $this->start();
        print "<TABLE>\n";
        reset($this->fields);
        $hidden = array();
        $call_cache = array();
        while (list($i, $data) = each($this->fields)) {
            switch ($data[0]) {
                case "hidden":
                    $hidden[] = $i;
                    continue 2;
                case "reset":
                    $params = 1;
                    break;
                case "submit":
                case "image":
                    $params = 2;
                    break;
                case "checkbox":
                    $params = 3;
                    break;
                case "text":
                case "password":
                case "radio":
                    $params = 4;
                    break;
                case "textarea":
                    $params = 5;
                    break;
                case "select":
                    $params = 8;
                    break;
                default:
                    // unknown field type
                    continue 2;
            }
            $str = $call_cache[$data[0]];
            if (empty($str)) {
                $str = '$this->display'.ucfirst($data[0])."Row(";
                for ($i = 1; $i <= $params; $i++) {
                    $str .= '$data['.$i.']';
                    if ($i < $params) $str .= ', ';
                }
                $str .= ');';
                $call_cache[$data[0]] = $str;
            }
            eval($str);
        }
        print "</TABLE>\n";
        for ($i = 0; $i < sizeof($hidden); $i++) {
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
