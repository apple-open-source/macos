<?php

require_once "PEAR.php";

/*

Tests that need to be made:
- error class
- mixing character encodings
- a test using all expat handlers
- options (folding, output charset)

 */

class XML_Parser extends PEAR {
    var $parser;
    var $fp;
    var $folding = true;

    function XML_Parser($charset = 'UTF-8') {
	$this->PEAR();
	$xp = @xml_parser_create($charset);
	if (is_resource($xp)) {
	    $this->parser = $xp;
	    xml_parser_set_option($xp, XML_OPTION_CASE_FOLDING,
				  $this->folding);
	    xml_set_object($xp, $this);
	    if (method_exists($this, "startHandler") ||
		method_exists($this, "endHandler")) {
		xml_set_element_handler($xp, "startHandler", "endHandler");
	    }
	    if (method_exists($this, "cdataHandler")) {
		xml_set_character_data_handler($xp, "cdataHandler");
	    }
	    if (method_exists($this, "defaultHandler")) {
		xml_set_default_handler($xp, "defaultHandler");
	    }
	    if (method_exists($this, "piHandler")) {
		xml_set_processing_instruction_handler($xp, "piHandler");
	    }
	    if (method_exists($this, "unparsedHandler")) {
		xml_set_unparsed_entity_decl_handler($xp, "unparsedHandler");
	    }
	    if (method_exists($this, "notationHandler")) {
		xml_set_notation_decl_handler($xp, "notationHandler");
	    }
	    if (method_exists($this, "entityrefHandler")) {
		xml_set_external_entity_ref_handler($xp, "entityrefHandler");
	    }
	}
    }

    function setInputFile($file) {
	$fp = @fopen($file, "r");
	if (is_resource($fp)) {
	    $this->fp = $fp;
	    return $fp;
	}
	return new XML_Parser_Error($php_errormsg);
    }

    function setInput($fp) {
	if (is_resource($fp)) {
	    $this->fp = $fp;
	    return true;
	}
	return new XML_Parser_Error("not a file resource");
    }

    function parse() {
	if (!is_resource($this->fp)) {
	    return new XML_Parser_Error("no input");
	}
	if (!is_resource($this->parser)) {
	    return new XML_Parser_Error("no parser");
	}
	while ($data = fread($this->fp, 2048)) {
	    $err = $this->parseString($data, feof($this->fp));
	    if (PEAR::isError($err)) {
		return $err;
	    }
	}
	return true;
    }

    function parseString($data, $eof = false) {
	if (!is_resource($this->parser)) {
	    return new XML_Parser_Error("no parser");
	}
	if (!xml_parse($this->parser, $data, $eof)) {
	    $err = new XML_Parser_Error($this->parser);
	    xml_parser_free($this->parser);
	    return $err;
	}
	return true;
    }

}

class XML_Parser_Error extends PEAR_Error {
    var $error_message_prefix = 'XML_Parser: ';
    function XML_Parser_Error($msgorparser = 'unknown error',
			      $code = 0,
			      $mode = PEAR_ERROR_RETURN,
			      $level = E_USER_NOTICE) {
	if (is_resource($msgorparser)) {
	    $msgorparser =
		sprintf("%s at XML input line %d",
			xml_error_string(xml_get_error_code($msgorparser)),
			xml_get_current_line_number($msgorparser));
	}
	$this->PEAR_Error($msgorparser, $code, $mode, $level);
    }
}

?>
