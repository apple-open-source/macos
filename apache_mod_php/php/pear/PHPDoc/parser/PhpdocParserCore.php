<?php
/**
* Provides basic parser functions.
*
* Provides basic parser functions to extract doc comments, analyse tags and variable
* declarations.
*
* @author		Ulf Wendel <ulf@redsys.de>
* @version  0.3
*/
class PhpdocParserCore extends PhpdocParserTags {
					
	/**
	* Scans code for documented and undocumented phpdoc keywords (classes, functions, class variables, uses, constants).
	*
	* This method is somewhat the heart of the phpdoc parser. It takes a string of 
	* phpcode and extracts all classes, functions, class variables, uses (include and friends), 
	* and constants (define) from it. Extract does not mean that the whole class or another element
	* gets extracted. It does not take the code from the class definition and it's opening 
	* curly brace to the closing one. PHPDoc just extracts the class definition itself and 
	* if available a trailing doc comment. This has some drawbacks: phpdoc can't handle 
	* files that contain more than one class it wouldn't know which method/class variable belongs to 
	* a certain class. It's possible to provide a workaround but phpdoc would slow down dramatically.
	* As PHPDoc does not have a real parser but does a simple grep using a bunch of regular expressions
	* there're indeed more limitations. Nevertheless I doubt that you'll have problems with "normal" code.
	*
	* The search algorithm looks pretty strange but belive me it's fast. I have tried several other ways
	* (really complex regexps >500 chars, preg_match_all + looking backwards for comments, ...) but none was
	* faster. This one takes 13s on my machine to scan the current (14/08/2000) code (7130 lines), the 
	* big RegExp way took more than 5 Minutes, the preg_match_all + looking backwards 52s.
	*
	* @param	string	PHP code to scan.
	* @param	mixed		String of one keyword or array of keywords not to scan for. Known keywords are:
	*									"classes", "functions", "variables", "uses", "consts".
	* @return	array   Hash of phpdoc elements found, indexed by "variables", "functions", "classes", "consts", "uses".
	* @see		$PHP_BASE, $PHP_COMPLEX, $C_BASE, $C_COMPLEX, extractPhpdoc(), getModuleDoc()
	*/					
	function getPhpdocParagraphs($phpcode, $keywords="none") {

		// what are we not looking for?		
		if ( !is_array($keywords) ) {
			if ("none" == $keywords) 
				$keywords = array ();
			else
				$keywords = array ( $keywords => true );
		}
		
		$start	 			= 0;
		$paragraphs	= array(
													"classes"		=> array(),
													"functions"	=> array(),
													"variables"	=> array(),
													"consts"		=> array(),
													"uses"			=> array(),
													"modules"		=> array()
											);


		// remember the documented elements to be able to compare with the list of all elements													
		$variables = array();
		$functions = array();
		$variables = array();
		$constants = array();
		$uses 		 = array();
		
		//
		// Module docs are somewhat more difficult to grep. Always
		// use this function.
		//
		if (!isset($keywords["modules"]))
			list($paragraphs["modules"], $phpcode) = $this->getModuleDoc($phpcode);
		else
			list( , $phpcode) = $this->getModuleDoc($phpcode);
			
		//
		// Find documented elements
		//

		while (true) {
			
			$start = strpos($phpcode, "/**", $start);
			if (0==(int)$start && "integer" != gettype($start) ) 
				break;

      $end 				= strpos($phpcode, "*/", $start);
			$remaining 	= trim(substr($phpcode, $end+2));
			
			if ( !isset($keywords["classes"]) && preg_match($this->PHP_COMPLEX["class"], $remaining, $regs) || preg_match($this->PHP_COMPLEX["class_extends"], $remaining, $regs)) {
				
				$paragraphs["classes"][] = array(
																					"name"		=> $regs[1],
																					"extends"	=> (isset($regs[2])) ? $regs[2] : "",
																					"doc"			=> $this->extractPhpdoc(substr($phpcode, $start+3, ($end-$start)-2))
																			);
				$classes[$regs[1]] = true;						
			
			} else if ( !isset($keywords["functions"]) &&	preg_match($this->PHP_COMPLEX["function"], $remaining, $regs)) {

				$head = substr($remaining, strpos($remaining, $regs[0])+strlen($regs[0]));
				$head = substr( trim($this->getValue($head, array( "{" => true) )), 0, -1);
				$paragraphs["functions"][] = array(
																						"name"	=> $regs[1],
																						"doc"		=> $this->extractPhpdoc( substr($phpcode, $start+3, ($end-$start)-2) ),
																						"head"	=> $head
																				);
				$functions[$regs[1]] = true;																				
										
			} else if ( !isset($keywords["variables"]) && preg_match($this->PHP_COMPLEX["var"], $remaining, $regs)) {

				if ("=" == $regs[2]) 
					$value = trim($this->getValue( substr($remaining, strpos($remaining, $regs[0])+strlen($regs[0]) ), array( ";" => true)));
				else
					$value = "";					
				
				$paragraphs["variables"][] = array(
																						"name"	=> $regs[1],
																						"value" => $value,
																						"doc"		=> $this->extractPhpdoc(substr($phpcode, $start+3, ($end-$start)-2))
																				);
				$variables[$regs[1]] = true;
				
			} else if ( !isset($keywords["consts"]) && preg_match($this->PHP_COMPLEX["const"], $remaining, $regs) ) {
			
				$name = (""!=$regs[2]) ? substr($regs[1], 1, -1) : $regs[1];
				
				if (isset($regs[5])) {
					if ($regs[5])
						$case = "case insensitive, userdefined: '$regs[5]'";
					else
						$case = "case sensitive, userdefined: '$regs[5]'";
				} else {
					$case = "default: case sensitive";
				}
				
				$paragraphs["consts"][] = array(
																					"name"	=> $name,
																					"value"	=> (""!=$regs[4]) ? substr($regs[3], 1, -1) : $regs[3],
																					"case"	=> $case,
																					"doc"		=> $this->extractPhpdoc(substr($phpcode, $start+3, ($end-$start)-2))
																		);
				$constants[$name] = true;
																						
			} else if ( !isset($keywords["uses"]) && preg_match($this->PHP_COMPLEX["use"], $remaining, $regs)) {

				$filename = isset($regs[5]) ? $regs[5] : $regs[4];
				$paragraphs["uses"][] = array(
																				"type"	=> $regs[1],
																				"file"	=> $filename,
																				"doc"		=> $this->extractPhpdoc(substr($phpcode, $start+3, ($end-$start)-2))
																		);
				$uses[$filename] = true;																		
				
			} 
			
			$start++;
		} 

		//
		// Find undocumented elements
		//
		if (!isset($keywords["classes"])) {
		
			preg_match_all($this->PHP_COMPLEX["undoc_class"], $phpcode, $regs, PREG_SET_ORDER);
			reset($regs);
			while (list($k, $data)=each($regs))
				if (!isset($classes[$data[1]]))
					$paragraphs["classes"][] = array(
																						"name"		=> $data[1],
																						"extends"	=> "",
																						"doc"			=> ""
																					);

			preg_match_all($this->PHP_COMPLEX["undoc_class_extends"], $phpcode, $regs, PREG_SET_ORDER);
			reset($regs);
			while (list($k, $data)=each($regs))
				if (!isset($classes[$data[1]]))
					$paragraphs["classes"][] = array(
																						"name"		=> $data[1],
																						"extends"	=> $data[2],
																						"doc"			=> ""
																				);																					
				
		}

		if (!isset($keywords["functions"])) {
			
			preg_match_all($this->PHP_COMPLEX["undoc_function"], $phpcode, $regs, PREG_SET_ORDER);
			reset($regs);
			while (list($k, $data)=each($regs)) 
				if (!isset($functions[$data[1]])) {
									
					$head = substr($phpcode, strpos($phpcode, $data[0])+strlen($data[0]));
					$head = substr( trim( $this->getValue($head, array( "{" => true) )), 0, -1);
					$paragraphs["functions"][] = array(
																						"name"	=> $data[1],
																						"doc"		=> "",
																						"head"	=> $head
																				);
				}
			
		}
		

		if (!isset($keywords["variables"])) {

			preg_match_all($this->PHP_COMPLEX["undoc_var"], $phpcode, $regs, PREG_SET_ORDER);
			reset($regs);
			while (list($k, $data)=each($regs)) 
				if (!isset($variables[$data[1]])) {
					
					if ("=" == $data[2])
						$value = trim($this->getValue( substr($phpcode, strpos($phpcode, $data[0])+strlen($data[0]) ), array( ";" => true)));
	   			else 
						$value = "";
				
					$paragraphs["variables"][] = array(
																						"name"	=> $data[1],
																						"value"	=> $value,
																						"doc"		=> ""
																				);
			}			
		}
		
		if (!isset($keywords["consts"])) {

			preg_match_all($this->PHP_COMPLEX["undoc_const"], $phpcode, $regs, PREG_SET_ORDER);
			reset($regs);
			while (list($k, $data)=each($regs)) {
			
				$name = (""!=$data[2]) ? substr($data[1], 1, -1) : $data[1];
				if (!isset($constants[$name])) {
					
					if (isset($data[5])) {
						if ($data[5])
							$case = "case insensitive, userdefined: '$data[5]'";
						else
							$case = "case sensitive, userdefined: '$data[5]'";
					} else {
						$case = "default: case sensitive";
					}
					
					$paragraphs["consts"][] = array(
																					"name"	=> $name,
																					"value"	=> (""!=$data[4]) ? substr($data[3], 1, -1) : $data[3],
																					"case"	=> $case,
																					"doc"		=> ""
																			);
				}
			}
		}
		
		if (!isset($keywords["uses"])) {

			preg_match_all($this->PHP_COMPLEX["undoc_use"], $phpcode, $regs, PREG_SET_ORDER);

			reset($regs);
			while (list($k, $data)=each($regs)) {
			
				$filename = isset($data[5]) ? $data[5] : $data[4];
				if (!isset($uses[$filename])) {
					
					$paragraphs["uses"][] = array(
																					"type"	=> $data[1],
																					"file"	=> $filename,
																					"doc"		=> ""
																				);
					
				}
			}
			
		}

		return $paragraphs;
	}	// end func getPhpdocParagraphs
	
	/**
	* Does a quick prescan to find modules an classes.
	* @param	string	Code to scan
	* @return	array		Hash of modules and classes found in the given code
	* @access	public
	* @see	getPhpdocParagraphs()
	*/
	function getModulesAndClasses($phpcode) {
		
		$para = array();
		list( $para["modules"], $phpdcode) = $this->getModuleDoc($phpcode);
		$para["classes"] = $this->getClasses($phpcode);
		
		return $para;
	} // end func getModulesAndClasses

	/**
	* Tries to extract a module doc.
	* 
	* The syntax for modules is not final yet. The implementation and meaning of "module" 
	* might change at every time! Please do not ask for implementation details.
	*
	* @param	string	PHP Code to scan
	* @return	array 	$module		$module[0] = array with module data, 
	*														$module[1] = php code without the leading module doc
	*/	
	function getModuleDoc($phpcode) {
		
		$module = array();
		
		if (preg_match($this->C_COMPLEX["module_doc"], $phpcode, $regs) ) {
		
			$start 			= strlen($regs[0]);
      $end 				= strpos($phpcode, "*/", $start);
			$remaining 	= substr($phpcode, $end+2);
			$doc_comment= substr($phpcode, $start, $end-$start);
			
			// Do we have OO Code? If not, continue.
			if ( !preg_match($this->PHP_COMPLEX["class"], $remaining) && !preg_match($this->PHP_COMPLEX["class_extends"], $remaining) ) {

				// Is there a module tag?
				if ( preg_match($this->C_COMPLEX["module_tags"], $doc_comment) ) {
				
					$doc_comment = $this->extractPhpDoc($doc_comment);
					$tags = $this->getTags( $doc_comment);
					$allowed = array (
															"module"	=> true,
															"modulegroup"	=> true
															
													);
					$tags = $this->analyseTags( $tags, array(), array( "module" => true, "modulegroup" => true) );
					
					$module = array (
														"doc"			=> $doc_comment,
														"status"	=> "ok",
														"name"		=> (isset($tags["module"])) ? $tags["module"] : "",
														"group"		=> (isset($tags["modulegroup"])) ? $tags["modulegroup"] : ""
													);
				
				} else {
			
					// No module tag. 
					// Try the remaining keywords. If one matches it's not a module doc 
					// assume that the module doc is missing. If none matches assume that
					// it's a module doc which lacks the module tags.
					if (	preg_match($this->PHP_COMPLEX["function"], $remaining) ||
								preg_match($this->PHP_COMPLEX["use"], $remaining) ||
								preg_match($this->PHP_COMPLEX["const"], $remaining) ||
								preg_match($this->PHP_COMPLEX["var"], $remaining) 
							) {

							$module = array(
															"doc"			=> "",
															"status"	=> "missing",
															"name"		=> "",
															"group"		=> ""
													);	
							$remaining = $phpcode;
							
					} else {

						$module = array (
																"doc"			=> $doc_comment,
																"status"	=> "tags missing",
																"name"		=> "",
																"group"		=> ""
															);	
						
					}
					
				} // end if module_tags
				
			} else {
				
				$remaining = $phpcode;
				
			} // end if class
							
		} else {
			
			$remaining = $phpcode;
					
		}
		
		return array($module, $remaining);
	} // end func getModuleDoc
	
	/**
	* Returns a list of classes found in the given code.
	*
	* In early versions PHPdoc parsed all the code at once which restulted in huge
	* memory intensive hashes. Now it scans for classes, builds a classtree and 
	* does the parsing step by step, writing information to the destination 
	* (renderer, exporter) as soon as possible. This reduces the memory consumption 
	* dramatically. getPhpdocParagraphs() could be used to extract the class definitions
	* as well but this specialized function is somewhat faster.
	*
	* @param	string	PHP code to scan.
	* @return	array		$classes	Array of classes found in the code. $classes[classname] = extends
	*/
	function getClasses($phpcode) {
		
		$classes = array();
		
		preg_match_all($this->PHP_COMPLEX["undoc_class"], $phpcode, $regs, PREG_SET_ORDER);
		reset($regs);
		while (list($k, $data)=each($regs))
			$classes[] = array(
													"name"		=> $data[1],
													"extends" => ""
												);
		
		preg_match_all($this->PHP_COMPLEX["undoc_class_extends"], $phpcode, $regs, PREG_SET_ORDER);
		reset($regs);
		while (list($k, $data)=each($regs)) 
			$classes[] = array(
													"name"		=> $data[1],
													"extends"	=> $data[2]
												);
		
		return $classes;
	} // end func getClasses
	
	/**
	* Strips "/xx", "x/" and x from doc comments (x means asterix).
	* @param	string	Doc comment to clean up.
	* @return	string	$phpdoc
	*/
	function extractPhpdoc($paragraph) {

		$lines = split( $this->PHP_BASE["break"], $paragraph);
		$phpdoc = "";

		reset($lines);
		while (list($k, $line)=each($lines)) {
		
			$line = trim($line);
			if (""==$line)
				continue;
				
			if ("*" == $line[0])
				$phpdoc.= trim(substr($line, 1))."\n";
			else 
				$phpdoc.= $line."\n";
				
		}
		
		return substr($phpdoc, 0, -1);
	} // end func extractPhpdoc
	
	/**
	* Extract the description from a PHPDoc doc comment.
	*
	* Every PHPDoc doc comment has the same syntax: /xx[break][x]short description
	* [break][[x]multiple line long description[break]][[x]@list of tags[. This function
	* returns an array of the short description and long description.
	*
	* @param	string	Doc comment to examine.
	* @return	array		$description	$description[0] = short description (first line),
	* 															$description[1] = long description (second line upto the first tag)
	*/
	function getDescription($phpdoc) {
	
		$positions = $this->getTagPos($phpdoc);
		
		if (0==count($positions)) {
			$desc = trim($phpdoc);
			$description = array ($desc, $desc);
		} else {
		
			$desc = trim(substr($phpdoc, 0, $positions[0]["pos"]));
			$lines = split($this->PHP_BASE["break"], $phpdoc);
			
			if (1==count($lines) || ""==$desc) 
				$description = array ($desc, $desc);
			else
				$description = array ( trim($lines[0]), $desc	);
		}

		return $description;
	} // end func getDescription
	
	/**
	* Scans a code passage for a value.
	*
	* There some cases where you can hardly use a regex to grep a value
	* because the value might contain unescaped charaters that end the value.
	* Value means something like "array ( ";", '\;' );" or "'phpdoc; ';" where
	* the delimiter would be ";".
	*
	* @param	string	The php code to examine.
	* @param	mixed		String of one delimiter or array of delimiters.
	* @return	string	Value found in the code
	* @todo		Racecondition: comments
	*/
	function getValue($code, $delimiter) {
		if (""==$code)
			return "";
	
		if (!is_array($delimiter)) 
			$delimiter = array( $delimiter => true );
			
		$code 				= trim($code);
		$len 					= strlen($code);
		$enclosed 		= false;
		$enclosed_by 	= "";
		
		if ( isset($delimiter[$code[0]]) ) {
		
			$i = 1;
			
		} else {
		
			for ($i=0; $i<$len; $i++) {
			
				$char = $code[$i];

				if (('"'==$char || "'"==$char) && ($char == $enclosed_by || ""==$enclosed_by) && (0==$i || ($i>0 && "\\"!=$code[$i-1]))) {
				
					if (!$enclosed)
						$enclosed_by = $char;
					else 
						$enclosed_by = "";
						
					$enclosed = !$enclosed;
					
				}
				if (!$enclosed && isset($delimiter[$char]))
					break;					
					
			}
		
		}
  
		return substr($code, 0, $i);
	} // end func getValue
	
	/**
	* Analyses a code snipped and returns the type and value of the first variable found.
	*
	* With version 0.3 PHPDoc tries to analyse variable declarations to find 
	* type and value. This is used to analyse class variable declarations and 
	* optional function arguments.
	* 
	* Note that all regular expressions in this function start with "^". That means
	* you have to do some preparations to the code snippet you're passing to this
	* function.
	*
	* @param	string 	PHP code to analyse
	* @param	boolean	Flag indicating the "type" of code to analyse. Optional 
	* 								function parameters and class variables have a slightly 
	* 								different syntax for arrays. By default function parameters
										are expected.
	* @return array		$vartype $vartype[0] = type, $vartype[1] = value, $vartype[2] = raw value
	* @see
	*/
	function getVariableTypeAndValue($code, $flag_args = true) {
	
		$type 			= "unknown";
		$value 			= "unknown";
		$raw_value 	= $code;

		//
		// Do not change the order the function tries to find out the type.
		//
		
		if (preg_match( $this->PHP_COMPLEX["type_boolean"], $code, $regs)) {

			$type					=	"boolean";
			$raw_value		= $regs[0];
			$value 				= $regs[0];
	
		} else if (preg_match( $this->PHP_COMPLEX["type_string_enclosed"], $code, $regs)) {

			$type 				= "string";
			$raw_value		= $regs[0];
			$value				= $regs[0];
			
		}	else if (preg_match( $this->PHP_COMPLEX["type_int_oct"], $code, $regs)) {
	
			$type 				= "integer (octal)";
			$raw_value		= $regs[0];
			$value				= preg_replace("@\s@", "", $regs[0]);
			if ( (int)$value != $value )
				$type.= " [warning: out of integer range, possible overflow trouble]";
			$value				= octdec($value)." ($value)";
			
	
		} else if (preg_match( $this->PHP_COMPLEX["type_int_hex"], $code, $regs)) {

			$type					= "integer (hexadecimal)";
			$raw_value		= $regs[0];
			$value				= preg_replace("@\s@", "", $regs[0]);
			if ( (int)$value != $value ) 
				$type.= " [warning: out of integer range, possible overflow trouble]";
			$value				= hexdec($value)." ($value)";

		} else if (preg_match( $this->PHP_COMPLEX["type_float_exponent"], $code, $regs)) {
		
			$type 				= "float";
			$raw_value		= $regs[0];
			$value				= (string)preg_replace("@\s@", "", $regs[0]);
			if ( (float)$value != $value ) 
				$type.= " [warning: out of float range]";
			$value				= (float)$value;
	
		} else if (preg_match( $this->PHP_COMPLEX["type_float"], $code, $regs)) {

			$type					= "float";
			$raw_value		= $regs[0];
			$value				= preg_replace("@\s@", "", $regs[0]);
			if ( (float)$value != $value ) 
				$type.= " [warning: out of float range]";
			$value = (float)$value;
	
		} else if (preg_match( $this->PHP_COMPLEX["type_number"], $code, $regs)) {
	
			$value				= preg_replace("@\s@", "", $regs[0]);
			$raw_value		= $regs[0];
			
			if ( (int)$value == $value ) {

				$type	= "integer";
				$value = (int)$value;

			} else {

				$type = "float";
				if ( (float)$value != $value )
					$type.=" [warning: out of float range]";
				$value = (float)$value;

			}
	
		} else if ($flag_args && preg_match( $this->PHP_COMPLEX["type_empty_array"], $code, $regs)) {
			
			$value 			= "array()";
			$raw_value	= $regs[0];
			$type 			= "array";
			
		} else if (!$flag_args && preg_match( $this->PHP_COMPLEX["type_array"], $code, $regs)) {
		
			$value = $this->getValue( $code, array(";" => true));
			// strpos() is twice as fast as substr()
			if ( 0 == strpos($value, "array")) 
				$type = "array";
			$raw_value == $value;
		
		} else if (preg_match( $this->PHP_COMPLEX["type_string"], $code, $regs)) {

			$type					= "string";
			$raw_value		= $regs[0];
			$value				= $regs[0];
		} 

		return array($type, $value, $raw_value);
	} // end func getVariableTypeAndValue
	
} // end class PhpdocParserObject
?>