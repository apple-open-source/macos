<?php

/* vim: set expandtab tabstop=4 shiftwidth=4: */
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
// | Authors: Mika Tuupola <tuupola@appelsiini.net>                       |
// +----------------------------------------------------------------------+
//
// $Id: Remote.php,v 1.1.1.2 2001/12/14 22:14:56 zarzycki Exp $

define("GIF_SIGNATURE_LENGTH",      3);
define("GIF_VERSION_LENGTH",        3);
define("GIF_LSW_LENGTH",            2);
define("GIF_LSH_LENGTH",            2);

define("PNG_SIGNATURE_LENGTH",      8);
define("PNG_IHDR_LENGTH_LENGTH",    4);
define("PNG_IHDR_CHUNKTYPE_LENGTH", 4);
define("PNG_IHDR_IMW_LENGTH",       4);
define("PNG_IHDR_IMH_LENGTH",       4);

define("JPG_PARAM_LENGTH_LENGTH",   2);
define("JPG_DATA_PRECISION_LENGTH", 1);
define("JPG_IMW_LENGTH_LENGTH",     2);
define("JPG_IMH_LENGTH_LENGTH",     2);
define("JPG_NUM_COMPONENTS_LENGTH", 1);

// The following jpeg defines were taken from rdjpgcom.c, which is part
// of the Independent JPEG Group's software.
// Copyright (C) 1994-1997, Thomas G. Lane.
//
// ftp://ftp.uu.net/graphics/jpeg/jpegsrc.v6b.tar.gz

define("M_SOF0",  0xC0);   /* Start Of Frame N */
define("M_SOF1",  0xC1);   /* N indicates which compression process */
define("M_SOF2",  0xC2);   /* Only SOF0-SOF2 are now in common use */
define("M_SOF3",  0xC3);
define("M_SOF5",  0xC5);   /* NB: codes C4 and CC are NOT SOF markers */
define("M_SOF6",  0xC6);
define("M_SOF7",  0xC7);
define("M_SOF9",  0xC9);
define("M_SOF10", 0xCA);
define("M_SOF11", 0xCB);
define("M_SOF13", 0xCD);
define("M_SOF14", 0xCE);
define("M_SOF15", 0xCF);
define("M_SOI",   0xD8);   /* Start Of Image (beginning of datastream) */
define("M_EOI",   0xD9);   /* End Of Image (end of datastream) */
define("M_SOS",   0xDA);   /* Start Of Scan (begins compressed data) */
define("M_APP0",  0xE0);   /* Application-specific marker, type N */
define("M_APP12", 0xEC);   /* (we don't bother to list all 16 APPn's) */
define("M_COM",   0xFE);   /* COMment */

require_once('PEAR.php');

/**
  * Image_Remote - retrieve size from remote image files.
  *
  * Class can be used to get size information from remote 
  * image files without downloading the whole image file. 
  * It mimics the GetImageSize() PHP function but works on  
  * remote images. Gif, jpeg and png currently supported. 
  * 
  * NOTE: Since PHP 4.0.5 URL support was added also to PHP's native
  * GetImageSize().
  *
  * Usage:
  *   
  *   $i = new Image_Remote("http://www.example.com/test.png");
  *   if (PEAR::isError($i)) {
  *       print "ERROR: " . $i->getMessage();
  *   } else { 
  *       $data = $i->getImageSize();
  *   }
  *   
  *
  * @access public
  * @author Mika Tuupola <tuupola@appelsiini.net>
  * @version $Revision: 1.1.1.2 $
  */


class Image_Remote
{

  /**
    * The URL to fetch size from
    *
    * @var  string
    */
    var $url;         

  /**
    * Parsed URL
    *
    * @var  array
    */
    var $purl;  

  /**
    * Filesize of the remote image
    *
    * @var  integer
    */      

    var $filesize;    // filesize of the remote file

  /**
    * Mimetype of the remote image
    *
    * @var  string
    */
    var $mimetype;   

  /**
    * Imagetype of the remote image
    *
    * @var  integer
    */
    var $imagetype;  

  /**
    * Width of the remote image
    *
    * @var  integer
    */
    var $width;       // image width

  /**
    * Height of the remote image
    *
    * @var  integer
    */
    var $height;      // image height

  /**
    * Some useless extrainfo
    *
    * @var  string
    */
    var $extra;       // some useless extrainfo


  /**
    * Error of the last method call
    *
    * @var  string
    */
    var $errstr;      // error of the last method call

  /**
    * Constructor
    * 
    * @param 	string 	foo URL of the remote image. Currently supports only http.
    * @return 	object
    *
    */

    function Image_Remote($input) 
    {
        $this->purl = parse_url($input);  

        if ($this->purl[scheme] == 'http') {
            $this->url = $input;

            // assign default port if not given
            if (!($this->purl[port])) {
                $this->purl[port] = 80;
            }
            $this->errstr = "";
            // actually fetch the info
            Image_Remote::_fetchImageInfo();

        } else {
            $this->errstr = "Only http supported.";
        }
    
        if ($this->errstr) {
            $this = new PEAR_Error($this->errstr);
        }

    } // constructor

  
  /**
    * Return information on the remote image. 
    *
    * Imitates PHP's native GetImageSize. 
    * 
    * http://www.php.net/manual/function.getimagesize.php  
    *
    * @access 	public
    * @return 	array 
    */
    function getImageSize() 
    {
        $retval = array(0,0,0,"");

        $retval[0] = $this->width;  
        $retval[1] = $this->height;  
        $retval[2] = $this->imagetype;
        $retval[3] = "WIDTH=\"$retval[0]\" HEIGHT=\"$retval[1]\"";

        return($retval);   
    }

  /**
    * Return the URL of the remote image. 
    *
    * This is actually the same URL which was given to the constructor.  
    * However it sometimes comes handy to be able to retrieve the URL  
    * again from the object. 
    * 
    * @access	public
    * @return 	string
    */

    function getImageURL() 
    {
        $retval = $this->url;
        return($retval);
    }

  /**
    * Fetch information of the remote image. 
    *
    * @access	private
    * @return 	boolean
    */
    function _fetchImageInfo() 
    {

        // default to success.
        $retval = 1;
        // Open the connection and get the headers
        $fp = fsockopen ($this->purl[host], $this->purl[port], &$errno, &$errstr, 30);
        if (!$fp) {
            $this->errstr = "Could not open socket $errstr ($errno)\n";
            $retval = 0;
        } else {

            // get the headers.
            fputs ($fp, "GET " . $this->purl[path] . $this->purl[query] . " HTTP/1.1\r\n");
            fputs ($fp, "Host: " . $this->purl[host] . "\r\n");
            fputs ($fp, "User-Agent: Image_Remote PHP Class\r\n");
            fputs ($fp, "Connection: close\r\n\r\n");
            $line = trim(fgets($fp,128));
            while ($line) {
                // Extract redirection
                if (preg_match("/^HTTP\/1\.[0-1] (3[0-9][0-9] .*)/", $line, $matches)) {
                    $this->errstr = $matches[1] . " not implemented yet!";
                    $retval = 0;
                }
                // Extract client error
                    if (preg_match("/^HTTP\/1\.[0-1] (4[0-9][0-9] .*)/", $line, $matches)) {
                    $this->errstr = $matches[1];
                    $retval = 0;
                }
                // Extract server error
                if (preg_match("/^HTTP\/1\.[0-1] (5[0-9][0-9] .*)/", $line, $matches)) {
                    $this->errstr = $matches[1];
                    $retval = 0;
                }
                // Extract mime type
                if (preg_match("/Content-Type: (.*)/", $line, $matches)) {
                    $tempmime = chop($matches[1]);
                }
                // Extract filesize
                 if (preg_match("/Content-Length: ([0-9]*)/", $line, $matches)) {
                   $tempfilesize = $matches[1];
                }
                $line = trim(fgets($fp,128));
            }

            // If we got correct mimetype etc (we trust what the webserver says), 
            // continue to fetch the actual data. 
      
            // if no error yet
            if ($retval) {
            // if correct mimetype
                if (preg_match("/image\/(gif|jpeg|x-jpeg|x-png|png)/", $tempmime)) {
                    $this->mimetype = $tempmime;
                    $this->filesize = $tempfilesize;

                    SWITCH ($tempmime) {
                        case 'image/gif':
                            Image_Remote::_fetchAndParseGIFHeader($fp);
                            $this->imagetype = 1;
                            break;
                        case 'image/png':
                        case 'image/x-png':
                            Image_Remote::_fetchAndParsePNGHeader($fp);
                            $this->imagetype = 3;
                            break;
                        case 'image/jpeg':
                        case 'image/x-jpeg':
                            Image_Remote::_fetchAndParseJPGHeader($fp);
                            $this->imagetype = 2;
                            break;
                    }

                } else {
                    $this->errstr = "Unsupported mimetype $tempmime.";
                    $retval = 0;
                }
            } 
            fclose($fp);
        }
        return($retval);
    } // _fetchImageInfo


  /**
    * Parse GIF header
    *
    * @access	private
    * @return 	void
    */
    function _fetchAndParseGIFHeader($fd) 
    {
        if (!$fd) {
            $this->errstr = "No socket.";
        } else {
            $signature = fread($fd, GIF_SIGNATURE_LENGTH);
            $version   = fread($fd, GIF_VERSION_LENGTH);
            $wbytes    = fread($fd, GIF_LSW_LENGTH);
            $hbytes    = fread($fd, GIF_LSH_LENGTH);
 
            // Host Byte Order
            $this->width  = Image_Remote::_htodecs($wbytes);
            $this->height = Image_Remote::_htodecs($hbytes);

            $this->extra  = $signature . $version;	
        }
    }

  /**
    * Parse PNG header
    *
    * @access	private
    * @return 	void
    */
    function _fetchAndParsePNGHeader($fd) 
    {

        if (!$fd) {
            $this->errstr = "No socket.";
        } else {
            $signature = fread($fd, PNG_SIGNATURE_LENGTH);
            $ihdrl     = fread($fd, PNG_IHDR_LENGTH_LENGTH);
            $ihdrct    = fread($fd, PNG_IHDR_CHUNKTYPE_LENGTH);
            $wbytes    = fread($fd, PNG_IHDR_IMW_LENGTH);
            $hbytes    = fread($fd, PNG_IHDR_IMH_LENGTH);
 
            // Network Byte Order
            $this->width  = Image_Remote::_ntodecl($wbytes);
            $this->height = Image_Remote::_ntodecl($hbytes);

            $this->extra  = $signature;       
        }
    }

    // The jpeg parser is basically port of code found from rdjpgcom.c, 
    // which is  part of the Independent JPEG Group's software.
    // Copyright (C) 1994-1997, Thomas G. Lane.
    //
    // ftp://ftp.uu.net/graphics/jpeg/jpegsrc.v6b.tar.gz
    //
    // Similar port can be found in elqGetImageFormat() by Oyvind Hallsteinsen 
    //
    // ftp://ftp.elq.org/pub/php3/elqGetImageFormat/elqGetImageFormat.php3

  /**
    * Parse JPEG header
    *
    * @access	private
    * @return 	void
    */
    function _fetchAndParseJPGHeader($fd) 
    {

        if (!$fd) {
            $this->errstr = "No socket.";
        } else {
            $done = 0;

            // first marker
            $c1 = ord(fread($fd, 1));
            $c2 = ord(fread($fd, 1));
            if ($c1 != 0xFF || $c2 != M_SOI) {
                $this->errstr="Not a jpeg file?";
            } else { 

                while (!($done)) {
        
                    // find next marker
                    $marker = ord(fread($fd, 1));
                    while ($marker != 0xFF) {
                        $marker = ord(fread($fd, 1));
                    }   
                    do {
                        $marker = ord(fread($fd, 1));
                    } while ($marker == 0xFF);

                    SWITCH ($marker) {
                        case M_SOF0:
                        case M_SOF1:
                        case M_SOF2:
                        case M_SOF3:
                        case M_SOF5:
                        case M_SOF6:
                        case M_SOF7:
                        case M_SOF9: 
                        case M_SOF10:
                        case M_SOF11:
                        case M_SOF13:
                        case M_SOF14:
                        case M_SOF15:
                            $length    = fread($fd, JPG_PARAM_LENGTH_LENGTH);
                            $precision = fread($fd, JPG_DATA_PRECISION_LENGTH);
                            $hbytes    = fread($fd, JPG_IMH_LENGTH_LENGTH);
                            $wbytes    = fread($fd, JPG_IMW_LENGTH_LENGTH);
                            $components= fread($fd, JPG_NUM_COMPONENTS_LENGTH);
               
                            // Network Byte Order
                            $this->width  = Image_Remote::_ntodecs($wbytes);
                            $this->height = Image_Remote::_ntodecs($hbytes);
                         
                            $this->extra  = "";
                            $done = 1;
                            break; 
                        case M_SOS:  
                            $done = 1; 
                            break;

                        // By default we skip over unwanted markers and avoid
                        // being fooled by 0xFF bytes in parameter segment.
                        default:
                            $lbytes = fread($fd, JPG_PARAM_LENGTH_LENGTH);
                            $length = Image_Remote::_ntodecs($lbytes); 
                            if ($length < JPG_PARAM_LENGTH_LENGTH) {
                                $this->errstr="Erronous parameter length.";
                                $done = 1;
                            } 
                            // the length is included in param length and
                            // allready read
                            $length -= JPG_PARAM_LENGTH_LENGTH;
                            fread($fd, $length);
                            break;
                    }    
                }
            }
        }
    }

  /**
    * Host byte order to decimal long
    *
    * @access	private
    * @return 	integer
    */
    function _htodecl($bytes) 
    {
        $b1 = ord($bytes[0]);
        $b2 = (ord($bytes[1])<<8);
        $b3 = (ord($bytes[2])<<16);
        $b4 = (ord($bytes[3])<<24);
        return($b1 + $b2 + $b3 + $b4);
    }

  /**
    * Host byte order to decimal short
    *
    * @access	private
    * @return 	integer
    */
    function _htodecs($bytes) 
    {
        $b1 = ord($bytes[0]);
        $b2 = (ord($bytes[1]) << 8);
        return($b1 + $b2);
    }

  /**
    * Network byte order to decimal long
    *
    * @access	private
    * @return 	integer
    */
    function _ntodecl($bytes) 
    {
        $b1 = ord($bytes[3]);
        $b2 = (ord($bytes[2]) << 8);
        $b3 = (ord($bytes[1]) << 16);
        $b4 = (ord($bytes[0]) << 24);
        return($b1 + $b2 + $b3 + $b4); 
    }

  /**
    * Network byte order to decimal short
    *
    * @access	private
    * @return 	integer
    */
    function _ntodecs($bytes) 
    {
        $b1 = ord($bytes[1]);
        $b2 = (ord($bytes[0]) << 8 );
        return($b1 + $b2);
    }

} // class 

?>
