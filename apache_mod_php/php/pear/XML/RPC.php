<?php
  require_once "XML/Parser.php";

//
// This file contains PEAR-ifications of XML-RPC code written by Edd
// Dumbill.
//
// by Edd Dumbill (C) 1999-2000
// <edd@usefulinc.com>
// $Id: RPC.php,v 1.1.1.2 2001/01/25 05:00:33 wsanchez Exp $

// License is granted to use or modify this software ("XML-RPC for PHP")
// for commercial or non-commercial use provided the copyright of the author
// is preserved in any distributed or derivative work.

// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESSED OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

if (!function_exists('xml_parser_create')) {
// Win 32 fix. From: "Leo West" <lwest@imaginet.fr>
	if($WINDIR) {
		dl("php3_xml.dll");
	} else {
		dl("xml.so");
	}
}

$XML_RPC_I4="i4";
$XML_RPC_Int="int";
$XML_RPC_Boolean="boolean";
$XML_RPC_Double="double";
$XML_RPC_String="string";
$XML_RPC_DateTime="dateTime.iso8601";
$XML_RPC_Base64="base64";
$XML_RPC_Array="array";
$XML_RPC_Struct="struct";

$XML_RPC_Types=array($XML_RPC_I4 => 1,
				   $XML_RPC_Int => 1,
				   $XML_RPC_Boolean => 1,
				   $XML_RPC_String => 1,
				   $XML_RPC_Double => 1,
				   $XML_RPC_DateTime => 1,
				   $XML_RPC_Base64 => 1,
				   $XML_RPC_Array => 2,
				   $XML_RPC_Struct => 3);

$XML_RPC_Entities=array("quot" => '"',
				   "amp" => "&",
				   "lt" => "<",
				   "gt" => ">",
				   "apos" => "'");

$XML_RPC_err["unknown_method"]=1;
$XML_RPC_str["unknown_method"]="Unknown method";
$XML_RPC_err["invalid_return"]=2;
$XML_RPC_str["invalid_return"]="Invalid return payload: enabling debugging to examine incoming payload";
$XML_RPC_err["incorrect_params"]=3;
$XML_RPC_str["incorrect_params"]="Incorrect parameters passed to method";
$XML_RPC_err["introspect_unknown"]=4;
$XML_RPC_str["introspect_unknown"]="Can't introspect: method unknown";
$XML_RPC_err["http_error"]=5;
$XML_RPC_str["http_error"]="Didn't receive 200 OK from remote server.";

$XML_RPC_defencoding="UTF-8";

// let user errors start at 800
$XML_RPC_erruser=800; 
// let XML parse errors start at 100
$XML_RPC_errxml=100;

// formulate backslashes for escaping regexp
$XML_RPC_backslash=chr(92).chr(92);

$XML_RPC_twoslash=$XML_RPC_backslash . $XML_RPC_backslash;
$XML_RPC_twoslash="2SLS";
// used to store state during parsing
// quick explanation of components:
//   st - used to build up a string for evaluation
//   ac - used to accumulate values
//   qt - used to decide if quotes are needed for evaluation
//   cm - used to denote struct or array (comma needed)
//   isf - used to indicate a fault
//   lv - used to indicate "looking for a value": implements
//        the logic to allow values with no types to be strings
//   params - used to store parameters in method calls
//   method - used to store method name

$_xh=array();


class XML_RPC_Parser extends XML_Parser {
    var $st;  /* used to build up a string for evaluation */
    var $ac;  /* used to accumulate values */
    var $qt;  /* used to decide if quotes are needed for evaluation */
    var $cm;  /* used to denote struct or array (comma needed) */
    var $isf; /* used to indicate a fault */
    var $lv;  /* used to indicate "looking for a vlaue": implements
		 the logic to allow values with no types to be strings */
    var $params; /* used to store parameters in method calls */
    var $method; /* used to store method name */

    function XML_RPC_Parser() {
	$this->XML_Parser();
    }

    function xmlrpc_se($parser, $name, $attrs) {
	global $_xh, $XML_RPC_DateTime;
	
	switch($name) {
	    case "STRUCT":
	    case "ARRAY":
		$_xh[$parser]['st'].="array(";
		$_xh[$parser]['cm']++;
		// this last line turns quoting off
		// this means if we get an empty array we'll 
		// simply get a bit of whitespace in the eval
		$_xh[$parser]['qt']=0;
		break;
	    case "NAME":
		$_xh[$parser]['st'].="'"; $_xh[$parser]['ac']="";
		break;
	    case "FAULT":
		$_xh[$parser]['isf']=1;
		break;
	    case "PARAM":
		$_xh[$parser]['st']="";
		break;
	    case "VALUE":
		$_xh[$parser]['st'].="new xmlrpcval("; 
		$_xh[$parser]['lv']=1; $_xh[$parser]['vt']="string";
		// look for a value: if this is still 1 by the
		// time we reach the first data segment then the type is string
		// by implication and we need to add in a quote
		$_xh[$parser]['ac']=""; // reset the accumulator
		break;
	    case "MEMBER":
		$_xh[$parser]['ac']="";
		break;
	    default:
		if ($name=="DATETIME.ISO8601" || $name=="STRING") {
		    $_xh[$parser]['qt']=1; 
		    if ($name=="DATETIME.ISO8601")
			$_xh[$parser]['vt']=$XML_RPC_DateTime;
		} else if ($name=="BASE64") {
		    $_xh[$parser]['qt']=2;
		} else {
		    $_xh[$parser]['qt']=0;
		}
	}
	if ($name!="VALUE") $_xh[$parser]['lv']=0;
    }

    function xmlrpc_ee($parser, $name) {
	global $_xh,$XML_RPC_Types;
	
	switch($name) {
	    case "STRUCT":
	    case "ARRAY":
		if ($_xh[$parser]['cm'] && substr($_xh[$parser]['st'], -1) ==',') {
		    $_xh[$parser]['st']=substr($_xh[$parser]['st'],0,-1);
		}
		$_xh[$parser]['st'].=")";	
		$_xh[$parser]['vt']=strtolower($name);
		$_xh[$parser]['cm']--;
		break;
	    case "NAME":
		$_xh[$parser]['st'].= $_xh[$parser]['ac'] . "' => ";
		break;
	    case "VALUE":
		if ($_xh[$parser]['qt']==1) {
		    // we use double quotes rather than single so backslashification works OK
		    $_xh[$parser]['st'].="\"". $_xh[$parser]['ac'] . "\""; 
		} else if ($_xh[$parser]['qt']==2) {
		    $_xh[$parser]['st'].="base64_decode('". $_xh[$parser]['ac'] . "')"; 
		} else 
		    $_xh[$parser]['st'].=$_xh[$parser]['ac'];
		$_xh[$parser]['st'].=", '" . $_xh[$parser]['vt'] . "')";
		if ($_xh[$parser]['cm']) $_xh[$parser]['st'].=",";
		break;
	    case "MEMBER":
		$_xh[$parser]['ac']=""; $_xh[$parser]['qt']=0;
		break;
	    case "DATA":
		$_xh[$parser]['ac']=""; $_xh[$parser]['qt']=0;
		break;
	    case "PARAM":
		$_xh[$parser]['params'][]=$_xh[$parser]['st'];
		break;
	    case "METHODNAME":
		$_xh[$parser]['method']=ereg_replace("^[\n\r\t ]+", "", $_xh[$parser]['ac']);
		break;
	    case "BOOLEAN":
		// special case here: we translate boolean 1 or 0 into PHP
		// constants true or false
		if ($_xh[$parser]['ac']=='1') 
		    $_xh[$parser]['ac']="true";
		else 
		    $_xh[$parser]['ac']="false";
		$_xh[$parser]['vt']=strtolower($name);
		break;
	    default:
		// if it's a valid type name, set the type
		if ($XML_RPC_Types[strtolower($name)]) {
		    $_xh[$parser]['vt']=strtolower($name);
		}
		break;
	}
    }
    
    function xmlrpc_cd($parser, $data) {	
	global $_xh, $XML_RPC_backslash, $XML_RPC_twoslash;
	
	//if (ereg("^[\n\r \t]+$", $data)) return;
	// print "adding [${data}]\n";
	if ($_xh[$parser]['lv']==1) {  
	    $_xh[$parser]['qt']=1; 
	    $_xh[$parser]['lv']=2; 
	}
	if ($_xh[$parser]['qt']) { // quoted string
	    $_xh[$parser]['ac'].=ereg_replace('\$', '\\$',
					      ereg_replace('"', '\"', 
							   ereg_replace($XML_RPC_backslash, 
									$XML_RPC_backslash, $data)));
	}
	else 
	    $_xh[$parser]['ac'].=$data;
    }

    function xmlrpc_dh($parser, $data) {
	global $_xh;
	if (substr($data, 0, 1) == "&" && substr($data, -1, 1) == ";") {
	    if ($_xh[$parser]['lv']==1) {  
		$_xh[$parser]['qt']=1; 
		$_xh[$parser]['lv']=2; 
	    }
	    $_xh[$parser]['ac'].=$data;
	}
    }
}

function xmlrpc_entity_decode($string) {
  $top=split("&", $string);
  $op="";
  $i=0; 
  while($i<sizeof($top)) {
	if (ereg("^([#a-zA-Z0-9]+);", $top[$i], $regs)) {
	  $op.=ereg_replace("^[#a-zA-Z0-9]+;",
						xmlrpc_lookup_entity($regs[1]),
											$top[$i]);
	} else {
	  if ($i==0) 
		$op=$top[$i]; 
	  else
		$op.="&" . $top[$i];
	}
	$i++;
  }
  return $op;
}

function xmlrpc_lookup_entity($ent) {
  global $XML_RPC_Entities;
  
  if ($XML_RPC_Entities[strtolower($ent)]) 
	return $XML_RPC_Entities[strtolower($ent)];
  if (ereg("^#([0-9]+)$", $ent, $regs))
	return chr($regs[1]);
  return "?";
}

class xmlrpc_client {
  var $path;
  var $server;
  var $port;
  var $errno;
  var $errstring;
  var $debug=0;
	var $username="";
	var $password="";

  function xmlrpc_client($path, $server, $port=80) {
		$this->port=$port; $this->server=$server; $this->path=$path;
  }

  function setDebug($in) {
		if ($in) { 
			$this->debug=1;
		} else {
			$this->debug=0;
		}
  }

	function setCredentials($u, $p) {
		$this->username=$u;
		$this->password=$p;
	}

  function send($msg, $timeout=0) {
		// where msg is an xmlrpcmsg
		$msg->debug=$this->debug;
		return $this->sendPayloadHTTP10($msg, $this->server, $this->port,
																		$timeout, $this->username, 
																		$this->password);
  }

	function sendPayloadHTTP10($msg, $server, $port, $timeout=0,
														 $username="", $password="") {
		if($timeout>0)
			$fp=fsockopen($server, $port,
										$this->errno, $this->errstr, $timeout);
		else
			$fp=fsockopen($server, $port,
										$this->errno, $this->errstr);
		if (!$fp) {   
			return 0;
		}
		// Only create the payload if it was not created previously
		if(empty($msg->payload)) $msg->createPayload();
		
		// thanks to Grant Rauscher <grant7@firstworld.net>
		// for this
		$credentials="";
		if ($username!="") {
			$credentials="Authorization: Basic " .
				base64_encode($username . ":" . $password) . "\r\n";
		}

		$op= "POST " . $this->path. " HTTP/1.0\r\nUser-Agent: PHP XMLRPC 1.0\r\n" .
			"Host: ". $this->server  . "\r\n" .
			$credentials . 
			"Content-Type: text/xml\r\nContent-Length: " .
			strlen($msg->payload) . "\r\n\r\n" .
			$msg->payload;
		
		if (!fputs($fp, $op, strlen($op))) {
			$this->errstr="Write error";
			return 0;
		}
		$resp=$msg->parseResponseFile($fp);
		fclose($fp);
		return $resp;
	}

} // end class xmlrpc_client

class xmlrpcresp {
  var $xv;
  var $fn;
  var $fs;
  var $hdrs;

  function xmlrpcresp($val, $fcode=0, $fstr="") {
	if ($fcode!=0) {
	  $this->fn=$fcode;
	  $this->fs=htmlspecialchars($fstr);
	} else {
	  $this->xv=$val;
	}
  }

  function faultCode() { return $this->fn; }
  function faultString() { return $this->fs; }
  function value() { return $this->xv; }

  function serialize() { 
	$rs="<methodResponse>\n";
	if ($this->fn) {
	  $rs.="<fault>
  <value>
    <struct>
      <member>
        <name>faultCode</name>
        <value><int>" . $this->fn . "</int></value>
      </member>
      <member>
        <name>faultString</name>
        <value><string>" . $this->fs . "</string></value>
      </member>
    </struct>
  </value>
</fault>";
	} else {
	  $rs.="<params>\n<param>\n" . $this->xv->serialize() . 
		"</param>\n</params>";
	}
	$rs.="\n</methodResponse>";
	return $rs;
  }
}

class xmlrpcmsg {
  var $payload;
  var $methodname;
  var $params=array();
  var $debug=0;

  function xmlrpcmsg($meth, $pars=0) {
		$this->methodname=$meth;
		if (is_array($pars) && sizeof($pars)>0) {
			for($i=0; $i<sizeof($pars); $i++) 
				$this->addParam($pars[$i]);
		}
  }

  function xml_header() {
	return "<?xml version=\"1.0\"?>\n<methodCall>\n";
  }

  function xml_footer() {
	return "</methodCall>\n";
  }

  function createPayload() {
	$this->payload=$this->xml_header();
	$this->payload.="<methodName>" . $this->methodname . "</methodName>\n";
	//	if (sizeof($this->params)) {
	  $this->payload.="<params>\n";
	  for($i=0; $i<sizeof($this->params); $i++) {
		$p=$this->params[$i];
		$this->payload.="<param>\n" . $p->serialize() .
		  "</param>\n";
	  }
	  $this->payload.="</params>\n";
	// }
	$this->payload.=$this->xml_footer();
	$this->payload=ereg_replace("\n", "\r\n", $this->payload);
  }

  function method($meth="") {
	if ($meth!="") {
	  $this->methodname=$meth;
	}
	return $this->methodname;
  }

  function serialize() {
		$this->createPayload();
		return $this->payload;
  }

  function addParam($par) { $this->params[]=$par; }
  function getParam($i) { return $this->params[$i]; }
  function getNumParams() { return sizeof($this->params); }

  function parseResponseFile($fp) {
	$ipd="";

	while($data=fread($fp, 32768)) {
	  $ipd.=$data;
	}
	return $this->parseResponse($ipd);
  }

  function parseResponse($data="") {
	global $_xh,$XML_RPC_err,$XML_RPC_str;
	global $XML_RPC_defencoding;

	
	$parser = xml_parser_create($XML_RPC_defencoding);

	$_xh[$parser]=array();

	$_xh[$parser]['st']=""; 
	$_xh[$parser]['cm']=0; 
	$_xh[$parser]['isf']=0; 
	$_xh[$parser]['ac']="";

	xml_parser_set_option($parser, XML_OPTION_CASE_FOLDING, true);
	xml_set_element_handler($parser, "xmlrpc_se", "xmlrpc_ee");
	xml_set_character_data_handler($parser, "xmlrpc_cd");
	xml_set_default_handler($parser, "xmlrpc_dh");
	$XML_RPC_value=new xmlrpcval;

	$hdrfnd=0;
	if ($this->debug)
	  print "<PRE>---GOT---\n" . htmlspecialchars($data) . 
		"\n---END---\n</PRE>";
	// see if we got an HTTP 200 OK, else bomb
	// but only do this if we're using the HTTP protocol.
	if (ereg("^HTTP",$data) && 
			!ereg("^HTTP/[0-9\.]+ 200 ", $data)) {
		$errstr= substr($data, 0, strpos($data, "\n")-1);
		error_log("HTTP error, got response: " .$errstr);
		$r=new xmlrpcresp(0, $XML_RPC_err["http_error"],
											$XML_RPC_str["http_error"]. " (" . $errstr . ")");
		xml_parser_free($parser);
		return $r;
	}
	// gotta get rid of headers here
	if ((!$hdrfnd) && ereg("^(.*)\r\n\r\n",$data,$_xh[$parser]['ha'])) {
	  $data=ereg_replace("^.*\r\n\r\n", "", $data);
	  $hdrfnd=1;
	}
	
	if (!xml_parse($parser, $data, sizeof($data))) {
		// thanks to Peter Kocks <peter.kocks@baygate.com>
		if((xml_get_current_line_number($parser)) == 1)   
			$errstr = "XML error at line 1, check URL";
		else
			$errstr = sprintf("XML error: %s at line %d",
												xml_error_string(xml_get_error_code($parser)),
												xml_get_current_line_number($parser));
		error_log($errstr);
		$r=new xmlrpcresp(0, $XML_RPC_err["invalid_return"],
											$XML_RPC_str["invalid_return"]);
		xml_parser_free($parser);
		return $r;
	}
	xml_parser_free($parser);
	if ($this->debug) {
	  print "<PRE>---EVALING---[" . 
		strlen($_xh[$parser]['st']) . " chars]---\n" . 
		htmlspecialchars($_xh[$parser]['st']) . ";\n---END---</PRE>";
	}
	if (strlen($_xh[$parser]['st'])==0) {
	  // then something odd has happened
	  // and it's time to generate a client side error
	  // indicating something odd went on
	  $r=new xmlrpcresp(0, $XML_RPC_err["invalid_return"],
						$XML_RPC_str["invalid_return"]);
	} else {
	  eval('$v=' . $_xh[$parser]['st'] . '; $allOK=1;');
	  if ($_xh[$parser]['isf']) {
		$f=$v->structmem("faultCode");
		$fs=$v->structmem("faultString");
		$r=new xmlrpcresp($v, $f->scalarval(), 
						  $fs->scalarval());
	  } else {
		$r=new xmlrpcresp($v);
	  }
	}
	$r->hdrs=split('\r?\n', $_xh[$parser]['ha'][1]);
	return $r;
  }

}

class xmlrpcval {
  var $me=array();
  var $mytype=0;

  function xmlrpcval($val=-1, $type="") {
		global $XML_RPC_Types;
		$this->me=array();
		$this->mytype=0;
		if ($val!=-1 || $type!="") {
			if ($type=="") $type="string";
			if ($XML_RPC_Types[$type]==1) {
				$this->addScalar($val,$type);
			}
	  else if ($XML_RPC_Types[$type]==2)
			$this->addArray($val);
			else if ($XML_RPC_Types[$type]==3)
				$this->addStruct($val);
		}
  }

  function addScalar($val, $type="string") {
		global $XML_RPC_Types, $XML_RPC_Boolean;

		if ($this->mytype==1) {
			echo "<B>xmlrpcval</B>: scalar can have only one value<BR>";
			return 0;
		}
		$typeof=$XML_RPC_Types[$type];
		if ($typeof!=1) {
			echo "<B>xmlrpcval</B>: not a scalar type (${typeof})<BR>";
			return 0;
		}
		
		if ($type==$XML_RPC_Boolean) {
			if (strcasecmp($val,"true")==0 || $val==1 || $val==true) {
				$val=1;
			} else {
				$val=0;
			}
		}
		
		if ($this->mytype==2) {
			// we're adding to an array here
			$ar=$this->me["array"];
			$ar[]=new xmlrpcval($val, $type);
			$this->me["array"]=$ar;
		} else {
			// a scalar, so set the value and remember we're scalar
			$this->me[$type]=$val;
			$this->mytype=$typeof;
		}
		return 1;
  }

  function addArray($vals) {
		global $XML_RPC_Types;
		if ($this->mytype!=0) {
			echo "<B>xmlrpcval</B>: already initialized as a [" . 
				$this->kindOf() . "]<BR>";
			return 0;
		}
		$this->mytype=$XML_RPC_Types["array"];
		$this->me["array"]=$vals;
		return 1;
  }

  function addStruct($vals) {
	global $XML_RPC_Types;
	if ($this->mytype!=0) {
	  echo "<B>xmlrpcval</B>: already initialized as a [" . 
		$this->kindOf() . "]<BR>";
	  return 0;
	}
	$this->mytype=$XML_RPC_Types["struct"];
	$this->me["struct"]=$vals;
	return 1;
  }

  function dump($ar) {
	reset($ar);
	while ( list( $key, $val ) = each( $ar ) ) {
	  echo "$key => $val<br>";
	  if ($key == 'array')
		while ( list( $key2, $val2 ) = each( $val ) ) {
		  echo "-- $key2 => $val2<br>";
		}
	}
  }

  function kindOf() {
	switch($this->mytype) {
	case 3:
	  return "struct";
	  break;
	case 2:
	  return "array";
	  break;
	case 1:
	  return "scalar";
	  break;
	default:
	  return "undef";
	}
  }

  function serializedata($typ, $val) {
		$rs="";
		global $XML_RPC_Types, $XML_RPC_Base64, $XML_RPC_String;
		switch($XML_RPC_Types[$typ]) {
		case 3:
			// struct
			$rs.="<struct>\n";
			reset($val);
			while(list($key2, $val2)=each($val)) {
				$rs.="<member><name>${key2}</name>\n";
				$rs.=$this->serializeval($val2);
				$rs.="</member>\n";
			}
			$rs.="</struct>";
			break;
		case 2:
			// array
			$rs.="<array>\n<data>\n";
			for($i=0; $i<sizeof($val); $i++) {
				$rs.=$this->serializeval($val[$i]);
			}
			$rs.="</data>\n</array>";
			break;
		case 1:
			switch ($typ) {
			case $XML_RPC_Base64:
				$rs.="<${typ}>" . base64_encode($val) . "</${typ}>";
				break;
			case $XML_RPC_Boolean:
				$rs.="<${typ}>" . ($val ? "1" : "0") . "</${typ}>";
					break;
			case $XML_RPC_String:
				$rs.="<${typ}>" . htmlspecialchars($val). "</${typ}>";
				break;
			default:
				$rs.="<${typ}>${val}</${typ}>";
			}
			break;
		default:
			break;
		}
		return $rs;
  }

  function serialize() {
	return $this->serializeval($this);
  }

  function serializeval($o) {
		global $XML_RPC_Types;
		$rs="";
		$ar=$o->me;
		reset($ar);
		list($typ, $val) = each($ar);
		$rs.="<value>";
		$rs.=$this->serializedata($typ, $val);
		$rs.="</value>\n";
		return $rs;
  }

  function structmem($m) {
		$nv=$this->me["struct"][$m];
		return $nv;
  }

	function structreset() {
		reset($this->me["struct"]);
	}
	
	function structeach() {
		return each($this->me["struct"]);
	}

  function scalarval() {
		global $XML_RPC_Boolean, $XML_RPC_Base64;
		reset($this->me);
		list($a,$b)=each($this->me);
		return $b;
  }

  function scalartyp() {
		global $XML_RPC_I4, $XML_RPC_Int;
		reset($this->me);
		list($a,$b)=each($this->me);
		if ($a==$XML_RPC_I4) 
			$a=$XML_RPC_Int;
		return $a;
  }

  function arraymem($m) {
		$nv=$this->me["array"][$m];
		return $nv;
  }

  function arraysize() {
		reset($this->me);
		list($a,$b)=each($this->me);
		return sizeof($b);
  }
}

// date helpers
function iso8601_encode($timet, $utc=0) {
	// return an ISO8601 encoded string
	// really, timezones ought to be supported
	// but the XML-RPC spec says:
	//
	// "Don't assume a timezone. It should be specified by the server in its
  // documentation what assumptions it makes about timezones."
	// 
	// these routines always assume localtime unless 
	// $utc is set to 1, in which case UTC is assumed
	// and an adjustment for locale is made when encoding
	if (!$utc) {
		$t=strftime("%Y%m%dT%H:%M:%S", $timet);
	} else {
		if (function_exists("gmstrftime")) 
			// gmstrftime doesn't exist in some versions
			// of PHP
			$t=gmstrftime("%Y%m%dT%H:%M:%S", $timet);
		else {
			$t=strftime("%Y%m%dT%H:%M:%S", $timet-date("Z"));
		}
	}
	return $t;
}

function iso8601_decode($idate, $utc=0) {
	// return a timet in the localtime, or UTC
	$t=0;
	if (ereg("([0-9]{4})([0-9]{2})([0-9]{2})T([0-9]{2}):([0-9]{2}):([0-9]{2})",
					 $idate, $regs)) {
		if ($utc) {
			$t=gmmktime($regs[4], $regs[5], $regs[6], $regs[2], $regs[3], $regs[1]);
		} else {
			$t=mktime($regs[4], $regs[5], $regs[6], $regs[2], $regs[3], $regs[1]);
		}
	} 
	return $t;
}

// general helper functions --- they DON'T WORK YET

function hash_to_rpcv($hash)
	// contributed by Ben Margolin <ben@wendy.auctionwatch.com>  
{
  reset($hash);
  while (list($k,$v) = each($hash)) {
    if (is_integer($v)) {
      $oa[$k] = new xmlrpcval($v, 'int');
    } else if (is_array($v)) {
			$oa[$k] = new xmlrpcval( hash_to_rpcv($v), 'struct' );
    } else { // if binary, do base64...?
      $oa[$k] = new xmlrpcval($v);
    }
	}
	return new xmlrpcval( $oa, 'struct' );
} // hash_to_rpc


function rpcv_to_hash($rv)
{
  $rv->structreset();
  while (list($k,$v) = $rv->structeach()) {
    $ko = $v->kindOf();
    if ('scalar' == $ko) {
      $oa[$k] = $v->scalarval();
    } else {
			$oa[$k] = rpcv_to_hash( $v );
		}
  }
	return $oa;
} // rpcv_to_hash 


?>
