<?php
class PhpdocParserTags extends PhpdocParserRegExp {
	
		/**
	* Extract the value from the given tags and copy the data to the $data array if its an allowed tag
	* @param	array	$tags			array of tags returned by getTags
 	* @param	array	$data			array where the allowed tags and their values are copied to
	* @param	array	$allowed	array of allowed (recognized) tags
	* @return	array	$data
	* @throws	PhpdocError
	* @see	getTags(), analyseVariableParagraphs(), analyseFunctionParagraphs(), analyseClassParagraphs(), analyseSeeTags()
	*/
	function analyseTags($tags, $data, $allowed) {
	
		if (!is_array($tags) || !is_array($data) || !is_array($allowed)) {
			$this->err[] = new PhpdocError("Illegal function call", __FILE__, __LINE__);
			return $data;
		}

		reset($tags);
		while (list($k, $tag)=each($tags)) {	
		
			$tagname = substr( strtolower($tag["tag"]), 1 );
			if (!isset($allowed[$tagname])) {
				$data["notallowed"][$tagname] = true;
				continue;
			}
			
			switch ($tagname) {
				
				# @tagname description
				case "exclude":
				case "package":
				case "magic":
				case "todo":
				case "version":
				case "since":
				case "include":
				case "copyright":
				
					if (isset($data[$tagname])) {	
					
						$tag["msg"] = "This tag must be used only once within a doc comment. First declaration gets used.";
						$data["syntaxerror"][] = $tag;
						break;
						
					}
						
					if (""==$tag["value"]) {
					
						$tag["msg"] = "Description is missing, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
						$data["syntaxerror"][] = $tag;
						$data[$tagname] = $tag["value"];
						
					} else 
						$data[$tagname] = $tag["value"];
						
					break;
				
				# @tagname [void]	
				case "abstract":		
				case "static":			
				case "final":
				
					if (isset($data[$tagname])) {
					
						$tag["msg"] = "This tag must be used only once within a doc comment. First declaration gets used.";
						$data["syntaxerror"][] = $tag;
						break;
						
					}
					
					if (""!=$tag["value"]) {
						
						$tag["msg"] = "Description gets ignored, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
						$data["syntaxerror"][] = $tag;
						
					}
					$data[$tagname] = true;
					break;
				
				case "var":
				case "return":

					if (isset($data[$tagname])) {
					
						$tag["msg"] = "This tag must be used only once within a doc comment. First declaration gets used.";
						$data["syntaxerror"][] = $tag;
						break;
						
					}
					
					if (preg_match($this->TAGS[$tagname], $tag["value"], $regs)) {

						if ("object"==$regs[1]) {
						
							$type = "object ";
							if ( ""==$regs[2] ) {
								$type.= "[unknown]";
								$tag["msg"] = "Objectname is missing, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
								$data["syntaxerror"][] = $tag;		
							} else {
								$type.= $regs[2];
							}
							
						} else {
							$type = $regs[1];
						}
							
						$data[$tagname] = array (
																		"type"		=> $type,
																		"name"		=> $regs[3]
																);
						if ("return"==$tagname && ""!=$regs[4]) 
							$data[$tagname]["desc"] = $regs[4];
														
					} else {
					
						$tag["msg"] = "General syntax error, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
						$data["syntaxerror"][] = $tag;
						
					}
					break;
	
				case "global":

					if (preg_match($this->TAGS["global"], $tag["value"], $regs)) {
						
						if (""==$regs[3]) {
							$tag["msg"] = "Variablename ist missing, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
							$data["syntaxerror"][] = $tag;
						}
						
						if ("object"==$regs[1]) {
						
							$type = "object ";
							if ( ""==$regs[2] ) {
								$type.= "[unknown]";
								$tag["msg"] = "Objectname is missing, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
								$data["syntaxerror"][] = $tag;		
							} else {
								$type.= $regs[2];
							}
							
						} else {
							$type = $regs[1];
						}
						
						if ("" == $regs[4]) {
							$data["global"][] = array (
																				"type"	=> $type,
																				"name"	=> $regs[3]
																		);
						} else {
							$data["global"][] = array (
																				"type"	=> $type,
																				"name"	=> $regs[3],
																				"desc"	=> $regs[4]
																		);
						}
						
					} else {
					
						$tag["msg"] = "General syntax error, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
						$data["syntaxerror"][] = $tag;
						
					}
						
					break;
					
				case "param":
				case "parameter":

					if (preg_match($this->TAGS["var"], $tag["value"], $regs)) {

						if ("object"==$regs[1]) {
						
							$type = "object ";
							if ( ""==$regs[2] ) {
								$type.= "[unknown]";
								$tag["msg"] = "Objectname is missing, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
								$data["syntaxerror"][] = $tag;		
							} else {
								$type.= $regs[2];
							}
							
						} else {
							$type = $regs[1];
						}											
						
						if (""==$regs[4]) {
							$data["params"][] = array (
																					"type"		=> $type,
																					"name"		=> $regs[3]
																				);
						} else {
							$data["params"][] = array (
																					"type"		=> $type,
																					"name"		=> $regs[3],
																					"desc"		=> $regs[4]
																				);
						}
											
					}  else {
					
						$tag["msg"] = "General syntax error, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
						$data["syntaxerror"][] = 	$tag;
						
					}
					
					break;
				
				case "see":

					if (""!=$tag["value"]) {
	
						$error = "";
						$references = explode(",", $tag["value"] );
						reset($references);
						while (list($k, $reference)=each($references)) {
						
							if (preg_match($this->TAGS["see_var"], $reference, $regs)) {
							
								list($msg, $entry) = $this->analyseSeeTagRegs($regs);
								$error.=$msg;
								if (count($entry)>0)
									$data["see"]["var"][] = $entry;	
								
							} else if (preg_match($this->TAGS["see_function"], $reference, $regs)) {
								
								list($msg, $entry) = $this->analyseSeeTagRegs($regs);
								$error.=$msg;
								if (count($entry)>0)
									$data["see"]["function"][] = $entry;
								
							} else if (preg_match($this->TAGS["see_moduleclass"], $reference, $regs)) {
								
								$name = $regs[1];
								
								if (substr($name, 0, $this->C_COMPLEX["module_separator_len"]) == $this->C_BASE["module_separator"]) {
								
									$name = substr($name, $this->C_COMPLEX["module_separator_len"]);
									if (""==$name) {
									
										$error.= "Element name is missing: '$regs[0]'. Reference gets ignored";
										continue;
										
									} else {
									
										$error.= "Element name starts with moduleseparator, module name forgotten '$regs[0]'?";
									
									}
										
								}
								$data["see"]["moduleclass"][] = $name;
								
							} else {
							
								$error.= "Unknown syntax '$reference'";
								
							}
						
						}
						
						if (""!=$error) {
							
							$tag["msg"] = sprintf("Could not understand all references. %s. Syntax: '%s' (function), '%s' (variable), '%s' (module or class).",
																			$error,
																			$this->C_COMPLEX["see_function"],
																			$this->C_COMPLEX["see_var"],
																			$this->C_COMPLEX["see_moduleclass"]
																		);
							$data["syntaxerror"][] = $tag;

						}
						
					} else {
					
						$tag["msg"] = "General syntax error, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
						$data["syntaxerror"][] = $tag;
						
					}
					break;
					
				case "link":
				
					if (preg_match($this->TAGS["link"], $tag["value"], $regs)) {

						$desc = trim($regs[2]);
						if ("" == $desc) {
							$data["link"][] = array(
																		"url"		=> $regs[1]
																);
						} else {					
							$data["link"][] = array(
																		"url"		=> $regs[1],
																		"desc"	=> trim($regs[2])
																);
						}
						
					} else {
					
						$tag["msg"] = "General syntax error, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
						$data["syntaxerror"][] = $tag;
						
					}
					break;
					
				case "throws":
				
					if (""!=$tag["value"]) {
				
						$exceptions = explode(",", $tag["value"]);
						reset($exceptions);
						while (list($k, $exception)=each($exceptions)) 
							$data["throws"][] = trim($exception);
					
					} else {
					
						$tag["msg"] = "General syntax error, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
						$data["syntaxerror"][] = $tag;
					}
					break;
					
				case "access":
				
					if (preg_match($this->TAGS["access"], $tag["value"], $regs)) {
					
						$data["access"] = $regs[1];
						
					} else {

						$tag["msg"] = (""==$tag["value"]) ? "General syntax error," : "Access modifier unknown,";
						$tag["msg"].= " '".$this->PHPDOC_TAGS["@$tagname"]."'.";
						$data["syntaxerror"][] = $tag;											
						
					}
						
					break;
					
				case "deprec":
				case "deprecated":
				
					if (isset($data["deprec"])) {
					
						$tag["msg"] = "This tag must be used only once within a doc comment. First declaration gets used.";
						$data["syntaxerror"][] = $tag;
						break;
					
					}				
					
					if (""!=$tag["value"]) {
					
						$data["deprec"] = $tag["value"];
					
					} else {
						
						$tag["msg"] = "Description is missing, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
						$data["syntaxerror"][] = $tag;
						
					}
					break;
					
				case "brother":
				case "sister":
				
					if (isset($data["brother"])) {
						
						$tag["msg"] = "This tag must be used only once within a doc comment. First declaration gets used.";
						$data["syntaxerror"][] = $tag;
						break;
						
					}
					
					if (""!=$tag["value"]) {
					
						if (preg_match($this->TAGS["brother"], $tag["value"], $regs)) {
							
							$data["brother"] = $regs[1];
							
						} else {
						
							$tag["msg"] = "Can't find a function name nor a variable name, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
							$data["syntaxerror"][] = $tag;
							
						}
						
					
					} else {
						
						$data["msg"] = "General syntax error, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
						$data["syntaxerror"][] = $tag;
						
					}
					break;

				case "module":
				case "modulegroup":
				
					if (isset($data[$tagname])) {
					
						$tag["msg"] = "This tag must be used only once within a doc comment. First declaration gets used.";
						$data["syntaxerror"][] = $tag;
						break;
					
					}
					
					if (""!=$tag["value"]) {
						
						if (preg_match($this->TAGS["module"], $tag["value"], $regs)) {
							
							$data[$tagname] = $regs[0];
							
						} else {

							$tag["msg"] = "Illegal label used, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";						
							$data["syntaxerror"][] = $tag;
							
						}
						
					} else {
					
						$tag["msg"] = "General syntax error, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
						$data["syntaxerror"][] = $tag;
						
					}
					break;		
					
				case "const":
				case "constant":	
				
					if (isset($data["const"])) {
					
						$tag["msg"] = "This tag must be used only once within a doc comment. First declaration gets used.";
						$data["syntaxerror"][] = $tag;
						break;
						
					}	
					
					if (""!=$tag["value"]) {
					
						if (preg_match($this->TAGS["const"], $tag["value"], $regs)) {
							
							$data["const"] = array(
																			"name"	=> $regs[1],
																			"desc"	=> trim($regs[2])
																		);
																		
						} else {
						
							$tag["msg"] = "General syntax error, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
							$data["syntaxerror"][] = $tag;
							
						}
						
					} else {
						
						$tag["msg"] = "General syntax error, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
						$data["syntaxerror"][] = $tag;

					}
					break;
				
				case "author":
				
					if (""!=$tag["value"]) {
					
						$authors = explode(",", $tag["value"]);
						reset($authors);
						while (list($k, $author)=each($authors)) {

							if (preg_match($this->TAGS["author"], $author, $regs)) {
												
								$data["author"][] = array(
																					"name"	=> trim(substr($author, 0, strpos($author, $regs[0]))),
																					"mail"	=> trim($regs[1])
																				);
							} else if (""!=trim($author)) {
								
								$data["author"][] = array(
																					"name"	=> trim($author)
																				);
																				
							} else {
							
								$tag["msg"] = "Name is missing in enumeration, syntax: '".$this->PHPDOC_TAGS["@$tagname"]."'.";
								$data["syntaxerror"][] = $tag;
								
							}
							
						}
					
					} else {
					
						$tag["msg"] = "General syntax error, syntax: ".$this->PHPDOC_TAGS["@$tagname"]."'.";
						$data["syntaxerror"][] = $tag;
					
					}

					break;
										
				default:
					// I'm quite sure this default is obsolete, but I don't feel like checking it.
					// Anyway this array index should get used one fine day.
					$data["unknown"][] = $tag;
					break;
			}	
			
		}	 	

		return $data;		
	} // end func analyseTags
	
	/**
	* Helperfunction to analyse @see tags
	* @param	array	Array return by preg_match()
	* @return	array $see[0] = error, $see[1] = data
	* @see	analyseTags()
	*/
	function analyseSeeTagRegs($regs) {
	
		$error 	= "";
		$group 	= $regs[1];
		$name 	= $regs[2];
		
		if (substr($name, 0, $this->C_COMPLEX["module_separator_len"]) == $this->C_BASE["module_separator"]) {
		
			$name = substr($name, $this->C_COMPLEX["module_separator_len"]);
			if (""==$name) {
			
				$error = "Element name is missing '$regs[0]'. Reference gets ignored";
				return array($error, array());
				
			} else {
			
				$error = "Element name starts with moduleseparator, module name forgotten '$regs[0]'?";
			
			}
			
		}
		
		if (""!=$group && $this->C_BASE["module_separator"]!=$group) {
		
			$group = substr($group, 0, $this->C_COMPLEX["module_separator_len_neg"]);
			if (""==$group) {

				$error = "Groupname missing '$regs[0]'.";
				$data = array( "name" => $name );
				
			} else {
			
				$data = array ( 
												"group"	=> $group,
												"name" 	=> $name
											);
											
			}
	
		} else {
		
			$data = array ( "name" => $name );
			
		}
		
		return array($error, $data);
	} // end func analyseSeeTagRegs
	
	/**
	* Extracts PHPDoc tags from a PHPDoc doc comment.
	* @param	string	Doc comment.
	* @return array		List of tags ordered by their appearance containing the 
	* 								tag name and it's (unparsed) value.
	* @see		getTagPos()
	*/
	function getTags($phpdoc) {

		$positions = $this->getTagPos($phpdoc);
		
		if (0==count($positions))
			return array();
		
		reset($positions);
		list($k, $data)=each($positions);
		$lastpos = $data["pos"];
		$lasttag = $data["tag"];
		
		while (list($k, $data)=each($positions)) {
			$line 		= substr($phpdoc, $lastpos, ($data["pos"]-$lastpos));
			$value 		= trim(substr($line, strlen($lasttag)));
			$tags[] 	= array ("tag"	=> $lasttag, "value"	=> $value );
			
			$lastpos	= $data["pos"];
			$lasttag	= $data["tag"];
		}
		
		$line 	= substr($phpdoc, $lastpos);
		$value 	= trim(substr($line, strlen($lasttag)));
		$tags[] = array ("tag"	=> $lasttag, "value"	=> $value );

		return $tags;
	} // end func getTags
	
	/**
	* Find the position of the next phpdoc tag.
	* @param	string	$phpdoc	
	* @param	int			$offset
	* @return array		$t					0 => tag, 1 => offset
	* @access	private
	* @see		findTags()
	*/
	function getTagPos($phpdoc, $offset=0) {
		
		$positions	= array();
		
		preg_match_all($this->TAGS["all"], $phpdoc, $regs, PREG_SET_ORDER);
		reset($regs);
		while (list($k, $data)=each($regs)) {
			$pos = strpos($phpdoc, $data[0], $offset);
			if ($pos>0 || $data[0]==substr($phpdoc, 0, strlen($data[0])) ) {
				$positions[] = array ("pos" => $pos, "tag" => $data[0]);
				$offset = $pos+1;
			}
		}

		return $positions;
	} // end func getTagPos	
	
	/**
	* Takes an array filled by analyseTags() and converts the parse errors into a single error message.
	* 
	* Checks for [syntaxerror], [notallowed] and [unknown] entries in the data hash,
	* converts them into an error message and unsets the indizes. The function
	* returns a hash containing the aggregates error message and the modified 
	* data hash.
	*
	* @param	array		$data
	* @param	string	$mode	Keyword where the data hash comes from eg. function/class...
	* @return	array	array( $error_msg, $data )
	*/
	function checkParserErrors($data, $mode) {
		
		$msg = "";
		// tags with an incorrect syntax
		if (isset($data["syntaxerror"])) {

			$msg.= "PHPDoc found ".count($data["syntaxerror"])." syntax error(s) in the tag list. ";
			
			reset($data["syntaxerror"]);
			while (list($k, $error)=each($data["syntaxerror"])) 
				$msg.= sprintf("Tag: '%s %s' - %s.", $error["tag"], $error["value"], $error["msg"]);
				
			unset($data["syntaxerror"]);			
			
		}
		
		// tags that are not allowed in this context
		if (isset($data["notallowed"])) {
			
			$msg.= count($data["notallowed"])." tag[s] were used that are not allowed in $mode doc comments: ";
			
			reset($data["notallowed"]);
			while (list($tagname, $v)=each($data["notallowed"]))
				$msg.= "$tagname, ";
				
			$msg = substr($msg, 0, -2).".";
			unset($data["notallowed"]);
		}
		
		// unknown tags
		if (isset($data["unknown"])) {
		
			$msg.= "PHPDoc found ".count($data["unknown"])." tag[s] that are unknown: ";
			
			reset($data["unknown"]);
			while (list($k, $error)=each($data["unknown"]))
				$msg.= sprintf("%s, ", $error["tag"]);
			
			$msg = substr($msg, 0, -2).".";
			unset($data["unknown"]);
		}
		
		return array($msg, $data);
	} // end func checkParserErrors
		
} // end class PhpdocParserTags
?>