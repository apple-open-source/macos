<?php
/**
* Defines all regular expressions.
*
* This class defines all regular expressions. To make the 
* configuration and customization of PHPDoc as simple as
* possible I decided to define all regular expressions in one class.
* From a programming point of view there's no need to do so. 
*
* @author		Ulf Wendel <ulf@redsys.de>
* @version  0.1
*/
class PhpdocParserRegExp extends PhpdocObject {

	/**
	* Array of phpdoc tags, indexed by the tagname.
	*
	* ... grepping information is really not a parser. Don't 
	* change the order the tags are listed. If you introduce
	* new tags write the long variant of the tagname (parameter)
	* in front of the shortcut (param).
	*
	* @var		array		List of all PHPDoc documentation tags.
	*/
	var $PHPDOC_TAGS = array(
														"@parameter"		=> '@param[eter] (object objectname|type) [$varname] [description]',
														"@param" 				=> '@param[eter] (object objectname|type) [$varname] [description]',
														
														"@return" 			=> '@return	(object objectname|type) [$varname] [description]',
														
														"@access"				=> '@access',
														"@abstract"			=> '@abstract',
														"@static"				=> '@static',
														"@final"				=> '@final',
														
														"@throws"				=> '@throws exception [, exception]',
														
														"@see"					=> '@see (function()|$varname|(module|class)(function()|$varname)) [, (funtion()|$varname|(module|class)(function()|$varname))]',
														"@link"					=> '@link URL [description]',
														
														"@var"					=> '@var 	(object objectname|type) [$varname]',
														"@global"				=> '@global (object objectname|type) $varname [description]',
														
														"@constant"			=> '@const[ant] label [description]',
														"@const"				=> '@const[ant] label [description]',
														
														"@author"				=> '@author Name [<email>] [, Name [<email>]',
														"@copyright"		=> '@copyright description',
														
														"@version"			=> '@version label',
														"@since"				=> '@since label',

														"@deprecated"		=> '@deprec[ated] description',														
														"@deprec"				=> '@deprec[ated] description',
														
														"@brother"			=> '@(brother|sister) (function()|$varname)',
														"@sister"				=> '@(brother|sister) (function()|$varname)',
																												
														"@include"			=> '@include description',	
														
														"@exclude"			=> '@exclude label',
														
														"@modulegroup"	=> '@modulegroup label',
														"@module"				=> '@module label',
														
														"@package"			=> '@package label',
														
														"@magic"				=> '@magic description',
														"@todo"					=> '@todo description'
													);

	/**
	* Basis regular expressions used to compose complex expressions to grep doc comments.
	*
	* PHPDoc tries to compose all complex regular expressions
	* from a list of basic ones. This array contains all expressions
	* used grep complex doc comments and the surrounding keywords.
	*
	* @var	array List of basic regular expressions matching parts of doc comments: 
	*							module names, module separator, vartypes, accesstypes.
	* @final
	* @see	buildComplexRegExps(), $C_COMPLEX
	*/
	var $C_BASE = array(
												#"block"						=> '/\*\*((?:(?!\*).)*(?:\n(?!\s*\*/)\s*\*(?:(?!\*/).)*)*)\*/',
												"module"								=> "[^\s]+",
												"module_separator"			=> "::",
												"module_tags"						=> "(@modulegroup|@module)",
																		
												"vartype"								=> "(string|integer|int|long|real|double|float|boolean|bool|mixed|array|object)",
												"access"								=> "(private|public)"
											);

	/**
	* List of regular expressions used to grep complex doc comments.
	* 
	* As with $PHP_COMPLEX all complex expressions are build using basic
	* ones in buildComplexRegExps().
	*
	* @var	array		Regular expressions matching @see and optional objectnames.
	* @final
	* @see	buildComplexRegexps(), $C_BASE
	*/															
	var $C_COMPLEX = array(															
														"objectname_optional"	=> "",
																		
														"see_var"							=> "",
														"see_function"				=> "",
														"see_moduleclass"			=> "",
														
														"module_doc"					=> "",
														"module_tags"					=> "",
														"module_separator"		=> "",
														"module_separator_len"			=> 0,
														"module_separator_len_neg"	=> 0
														
												);
	
	/**
	* Basic RegExps used to analyse PHP Code.
	*
	* PHPDoc tries to compose all complex regular expressions
	* from some basic expressions. This array contains
	* all expressions used to build $PHP_COMPLEX. 
	* There're some differences to the RegExps in zend-scanner.l, 
	* e.g. I decided to write "\s+" instead of "[ \n\r\t]+" which
	* should be identical as long as perl compatible regular 
	* expressions are used. Another point is that I did not break 
	* down numbers to LNUM/DNUM.
	* 
	* @var		array		List of basis regular expressions matching php code elements:
	*									spaces, optional spaces, linebreaks, labels, use (include and friends),
	*									optional argument assignment, boolean, several variable types.
	* @final
	* @see		$PHP_COMPLEX
	*/
	var $PHP_BASE = array (

													"space"						=> "\s+",
													"space_optional"	=> "\s*",
													"break"						=> "[\n\r]",
													
													"php_open_long"		=> "<\?php\s", # zend_scanner.l use {WHITESPACE} (space in our case) eighter. Might be slightly faster.
													"php_open_short"	=> "<\?",
													"php_open_asp"		=> "<%",
													"php_open_short_print" 	=> "<\?=",
													"php_open_asp_print"		=> "<%=",
													
													 # do not change the single quotes to double ones
													"label"						=> '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\xzf-\xff]*', 
													"use"							=> "(include_once|include|require_once|require)",
													"assignment"			=> "\s*([,=])\s*",
													
													"boolean"					=> "(true|false)",
													
													"string"					=> "[^\s]+",
													"string_enclosed"	=> "(['\"])(?:\\\\\\1|[^\\1])*?\\1",

													"int_oct"					=> "[+-]?\s*0[0-7]+",
													"int_hex"					=> "[+-]?\s*0[xX][0-9A-Fa-f]+",
													
													"float"						=> "[+-]?\s*\d*\.\d+",
													"float_exponent"	=> "[+-]?\s*\d*(?:\.\d+)*[eE][+-]?\d+",
													
													"number"					=> "[+-]?\s*\d+",
													
													"array"						=> "array\s*\(",
													"empty_array"			=> "array\s*\(\s*\)\s*"
												);

	/**
	* List of regular expressions used to grep complex php code elements.
	*
	*	The RegExp of the variable types is slightly changed to that
	* one in $PHP_BASE, getVariableTypeAndValue() needs this.
	*	"undoc_*" is used to grep all keywords those who have a doc 
	* comment in front and those without. See getPhodocParagraphs() 
	* for more details on this.
	*
	* @var	array	RegExps to match: variablenames, functionnames, classnames,
	*							class variable declarations, function declarations,
	*             class declarations, defines, uses (include and friends), 
	* 						function arguments, several variables types. 
	* @see	buildComplexRegExps(), getVariableTypeAndValue(), getPhpdocParagraphs(), $PHP_BASE
	*/																	
	var $PHP_COMPLEX = array (
															"varname"					=> "",
															"functionname"		=> "",
															"classname"				=> "",
															
															"php_open_script"	=> "",
													
															"var"							=> "",
															"undoc_var"				=> "",
																			
															"function"				=> "",
															"undoc_function"	=> "",
																			
															"class"						=> "",
															"undoc_class"			=> "",
																			
															"class_extends"				=> "",
															"undoc_class_extends"	=> "",
																			
															"const"						=> "",
															"undoc_const"			=> "",
																			
															"use"							=> "",
															"undoc_use"				=> "",
																		
															"argument"				=> "",
															
															"type_boolean"		=> "",
															
															"type_string"						=> "",
															"type_string_enclosed"	=> "",
															
															"type_int_oct"		=> "",
															"type_int_hex"		=> "",
															
															"type_float"			=> "",
															"type_float_exponent"	=> "",
															
															"type_number"			=> "",
															
															"type_array"				=> "",
															"type_empty_array"	=> ""
														);																	
	
	/**
	* Array of RegExp matching the syntax of several complex tags.
	*
	* The array is filled by the constructor.
	*
	* @var	array		Used to analyse return, var, param, 
	*								global, see and to find tags in general
	* @see	PhpdocParserObject()
	*/
	var $TAGS = array ( 
											"return"				=> "", 
											"var"						=> "", # @var, @param
											"global"				=> "", 
											"access"				=> "", 
											
											"module"				=> "", # @module, @modulegroup
											
											"const"					=> "", # @const, @constant
											
											"see_var"				=> "", # @see
											"see_function"	=> "", # @see
											"see_class"			=> "", # @see
											"see_module"		=> "", # @see
											
											"link"					=> "@([^\s]+)(.*)@is", # @link
											
											"brother"				=> "",
											
											"author"				=> "<\s*([a-z]([-a-z0-9_.])*@([-a-z0-9_]*\.)+[a-z]{2,})\s*>", # @author <email> part
											
											"all"						=> ""	 # list of all known tags
										);
	
	/**
	* Builds complex regular expressions for the parser.
	*
	* PHPDoc has a small set of basic regular expressions. All complex
	* regular expressions are made out of the basic ones. The composition 
	* in done in this method. Note: every derived class must 
	* call this method in it's constructor!
	* @see	$PHP_BASE, $PHP_COMPLEX, $C_BASE, $C_COMPLEX
	*/													
	function buildComplexRegExps() {
	
		//
		// Do not change the order of the variable initializations there're dependencies.
		// It starts with some php names.
		// 
		
		// some names
		$this->PHP_COMPLEX["varname"] = sprintf("[&]?[$]%s", $this->PHP_BASE["label"] );
		$this->PHP_COMPLEX["functionname"] = sprintf("[&]?%s", $this->PHP_BASE["label"]	);
		$this->PHP_COMPLEX["classname"] = $this->PHP_BASE["label"];					
		
		// 
		// Now build all regexps used to grep doc comment elements.
		// 
		
		// optional object name
 		$this->C_COMPLEX["objectname_optional"] = sprintf("(?:object%s%s)?", 
																												$this->PHP_BASE["space"],
																												$this->PHP_COMPLEX["classname"] 
																											);
		
		$this->C_COMPLEX["module_separator"] = sprintf("(?:%s)", $this->C_BASE["module_separator"]);
		$this->C_COMPLEX["module_separator_len"] = strlen($this->C_BASE["module_separator"]);
		$this->C_COMPLEX["module_separator_len_neg"] = -1*strlen($this->C_BASE["module_separator"]);

		// References to other elements
		$this->C_COMPLEX["see_var"] = sprintf("(%s%s)?([$][^:]%s)",
																							$this->C_BASE["module"],
																							$this->C_COMPLEX["module_separator"],
																							$this->PHP_BASE["label"]
																						);
																						
		$this->C_COMPLEX["see_function"] = sprintf("(%s%s)?([^:]%s\(%s\))",
																									$this->C_BASE["module"],
																									$this->C_COMPLEX["module_separator"],
																									$this->PHP_BASE["label"],
																									$this->PHP_BASE["space_optional"]
																								);

		$this->C_COMPLEX["see_moduleclass"] = sprintf("(%s)",  $this->C_BASE["module"]	);

		//
		// RegExps used to grep certain php code elements.
		//
		
		// var statements
		$this->PHP_COMPLEX["var"] =  sprintf("|^%svar%s([$]%s)%s(=?)|is",
																							$this->PHP_BASE["space_optional"],
																							$this->PHP_BASE["space"],
																							$this->PHP_BASE["label"],
										                          $this->PHP_BASE["space_optional"],
																							$this->PHP_BASE["space_optional"]
																					);	
		$this->PHP_COMPLEX["undoc_var"] = sprintf("|%s|isS", substr($this->PHP_COMPLEX["var"], 2, -3) );

		// function statements
		$this->PHP_COMPLEX["function"] = sprintf("|^%sfunction%s(%s)%s\(|is",
																									$this->PHP_BASE["space_optional"],
																									$this->PHP_BASE["space"],
																									$this->PHP_COMPLEX["functionname"],
                          												$this->PHP_BASE["space_optional"]
																								);	 																	
		$this->PHP_COMPLEX["undoc_function"] = sprintf("|%s|isS",	substr($this->PHP_COMPLEX["function"], 2, -3) );

		// class statements
		$this->PHP_COMPLEX["class"] = sprintf("|^%sclass%s(%s)%s{|is",
																								$this->PHP_BASE["space_optional"],
																								$this->PHP_BASE["space"],
																								$this->PHP_COMPLEX["classname"],
																								$this->PHP_BASE["space_optional"]
																							);									
		$this->PHP_COMPLEX["undoc_class"] = sprintf("|%s|isS", substr($this->PHP_COMPLEX["class"], 2, -3) );
		
		$this->PHP_COMPLEX["class_extends"] = sprintf("|^%sclass%s(%s)%sextends%s(%s)%s{|is",
																											$this->PHP_BASE["space_optional"],	
																											$this->PHP_BASE["space"],
																											$this->PHP_COMPLEX["classname"],
																											$this->PHP_BASE["space"],
																											$this->PHP_BASE["space"],
																											$this->PHP_COMPLEX["classname"],
																											$this->PHP_BASE["space_optional"]
																										);		
		$this->PHP_COMPLEX["undoc_class_extends"] = sprintf("|%s|isS", substr($this->PHP_COMPLEX["class_extends"], 2, -3) );
		
		// 
		// RegExp used to grep define statements.
		// NOTE: the backticks do not allow the usage of $this->PHP_BASE
		//
		$this->PHP_COMPLEX["const"] = sprintf("@^%sdefine%s\(%s(%s)%s,%s(%s)%s(?:,%s(%s))?%s\)%s;@is", 
																			$this->PHP_BASE["space_optional"],
																			$this->PHP_BASE["space_optional"],
																			$this->PHP_BASE["space_optional"],
																			"[$]?\w[\w-_]*|(['\"])(?:\\\\\\2|[^\\2])*?\\2",
																			$this->PHP_BASE["space_optional"],
																			$this->PHP_BASE["space_optional"],
																			"(['\"])(?:\\\\\\4|[^\\4])*?\\4|(?:true|false)|[+-]?\s*0[0-7]+|[+-]?\s*0[xX][0-9A-Fa-f]+|[+-]?\s*\d*(?:\.\d+)*[eE][+-]?\d+|[+-]?\s*\d*\.\d+|[+-]?\s*\d+|&?[$]?\w[\w-_]*",
																			$this->PHP_BASE["space_optional"],
																			$this->PHP_BASE["space_optional"],
																			"(?:true|false)|[+-]?\s*0[0-7]+|[+-]?\s*0[xX][0-9A-Fa-f]+|[+-]?\s*\d*(?:\.\d+)*[eE][+-]?\d+|[+-]?\s*\d*\.\d+|[+-]?\s*\d+|&?[$]?\w[\w-_]*|(['])(?:\\\\\\6|[^\\6])*?\\6",
																			$this->PHP_BASE["space_optional"],
																			$this->PHP_BASE["space_optional"]
																		);		
		$this->PHP_COMPLEX["undoc_const"] = sprintf("@%s@isS", substr($this->PHP_COMPLEX["const"], 2, -3) );
		
		//
		// include, include_once, require, require_once and friends 
		//
// ? removed!
		$this->PHP_COMPLEX["use"] = sprintf("@^%s%s[\(]%s((['\"])((?:\\\\\\3|[^\\3])*?)\\3|([^\s]+))%s[\)]%s;@is",
																						$this->PHP_BASE["use"],
																						$this->PHP_BASE["space_optional"],
																						$this->PHP_BASE["space_optional"],
																						$this->PHP_BASE["space_optional"],
																						$this->PHP_BASE["space_optional"]
																				);
		$this->PHP_COMPLEX["undoc_use"] = sprintf("@%s@isS", substr($this->PHP_COMPLEX["use"], 2, -3) );
						
		//										
		// Variable name with an optional assignment operator. This one is used
		// to analyse function heads [parameter lists] as well as class variable
		// declarations.
		//
		$this->PHP_COMPLEX["argument"] = sprintf("|(%s)(%s)?|s", 
																												$this->PHP_COMPLEX["varname"],
																												$this->PHP_BASE["assignment"]
																										);


		//
		// <script language="php"> syntax
		//																
		$this->PHP_COMPLEX["php_open_script"] = sprintf("<script%slanguage%s=%s[\"']php[\"']%s>",
																							$this->PHP_BASE["space"],
																							$this->PHP_BASE["space_optional"],
																							$this->PHP_BASE["space_optional"],
																							$this->PHP_BASE["space_optional"]
																						);

		$this->PHP_COMPLEX["php_open_all"] = sprintf("(?:%s|%s|%s|%s|%s|%s)",
																					$this->PHP_BASE["php_open_long"],
																					$this->PHP_BASE["php_open_short"],
																					$this->PHP_BASE["php_open_asp"],
																					$this->PHP_BASE["php_open_short_print"],
																					$this->PHP_BASE["php_open_asp_print"],
																					$this->PHP_COMPLEX["php_open_script"]
																				);

		$this->C_COMPLEX["module_doc"] = sprintf("@^%s%s%s/\*\*@is", 
																					$this->PHP_BASE["space_optional"],
																					$this->PHP_COMPLEX["php_open_all"],
																					$this->PHP_BASE["space_optional"]
																				);

		$this->C_COMPLEX["module_tags"] = sprintf("/%s/is", $this->C_BASE["module_tags"] );

		//
		// RegExp used to grep variables types
		//
		$elements = array( 
											"boolean", "string", "string_enclosed", 
											"int_oct", "int_hex", "float", "float_exponent", 
											"number", "array", "empty_array" 
										);
		reset($elements);
		while (list($key, $name)=each($elements)) 
			$this->PHP_COMPLEX["type_".$name] = sprintf("@^%s@", $this->PHP_BASE[$name]);
																			
		// 
		// Regular expressions used to analyse phpdoc tags.
		// 
		$this->TAGS["return"] = sprintf("/%s%s(%s)%s(%s)?%s(.*)/is",
															$this->C_BASE["vartype"],
															$this->PHP_BASE["space_optional"],
															$this->C_COMPLEX["objectname_optional"],
															$this->PHP_BASE["space"],
															$this->PHP_COMPLEX["varname"],
															$this->PHP_BASE["space_optional"]
														);
														
		$this->TAGS["var"] = sprintf("/%s%s(%s)%s(%s)?%s(.*)/is",
															$this->C_BASE["vartype"],
															$this->PHP_BASE["space_optional"],
															$this->C_COMPLEX["objectname_optional"],
															$this->PHP_BASE["space"],
															$this->PHP_COMPLEX["varname"],
															$this->PHP_BASE["space_optional"]
														);	
														
		$this->TAGS["global"] = sprintf("/%s%s(%s)%s(%s)%s(.*)/is",
															$this->C_BASE["vartype"],
															$this->PHP_BASE["space_optional"],
															$this->C_COMPLEX["objectname_optional"],
															$this->PHP_BASE["space"],
															$this->PHP_COMPLEX["varname"],
															$this->PHP_BASE["space_optional"]
														);	
														
		$this->TAGS["brother"] = sprintf("/(%s\(\)|\$%s)/is", 
															$this->PHP_BASE["label"],
															$this->PHP_BASE["label"]
														);
		
		$this->TAGS["const"] = sprintf("/(%s)%s(.*)/is",
															$this->PHP_BASE["label"],
															$this->PHP_BASE["space"]
														);
														
		$this->TAGS["access"] = sprintf("/%s/is", $this->C_BASE["access"]);
		$this->TAGS["module"] = sprintf("/%s/is", $this->PHP_BASE["label"]);
		
		$this->TAGS["author"] = sprintf("/%s/is", $this->TAGS["author"]);
		
		$all_tags = "";											
		reset($this->PHPDOC_TAGS);														
		while (list($tag, $v)=each($this->PHPDOC_TAGS))
			$all_tags.= substr($tag, 1)."|";
		$all_tags = substr($all_tags, 0, -1);
		
		$this->TAGS["all"] = "/@($all_tags)/is";
		
		$elements = array ( "see_function", "see_var", "see_moduleclass" );
		reset($elements);
		while (list($k, $index)=each($elements))
			$this->TAGS[$index] = sprintf("/%s/is", $this->C_COMPLEX[$index]);

	} // end func buildComplexRegExps
	
} // end class PhpdocParserRegExp
?>