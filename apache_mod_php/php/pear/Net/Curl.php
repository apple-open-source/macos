<?php
//
// +----------------------------------------------------------------------+
// | PHP version 4.0                                                      |
// +----------------------------------------------------------------------+
// | Copyright (c) 1997-2001 The PHP Group                                |
// +----------------------------------------------------------------------+
// | This source file is subject to version 2.02 of the PHP license,      |
// | that is bundled with this package in the file LICENSE, and is        |
// | available at through the world-wide-web at                           |
// | http://www.php.net/license/2_02.txt.                                 |
// | If you did not receive a copy of the PHP license and are unable to   |
// | obtain it through the world-wide-web, please send a note to          |
// | license@php.net so we can mail you a copy immediately.               |
// +----------------------------------------------------------------------+
// | Authors: Sterling Hughes <sterling@php.net>                          |
// +----------------------------------------------------------------------+
//
// $Id: Curl.php,v 1.1.1.1 2001/07/19 00:20:50 zarzycki Exp $
//
// A nice friendly OO interface for CURL
//
require_once('PEAR.php');

class Net_Curl extends PEAR
{
	// {{{ Public Properties
	
	/**
	 * The URL for cURL to work with
	 *
	 * @var string $url
	 * @access public
	 */
	var $url;
	
	/**
	 * The SSL version for the transfer
	 *
	 * @var integer $sslVersion
	 * @access public
	 */
	var $sslVersion;
	
	/**
	 * The filename of the SSL certificate
	 *
	 * @var string $sslCert
	 * @access public
	 */
	var $sslCert;
	
	/**
	 * The password corresponding to the certificate
	 * in the $sslCert property
	 *
	 * @var string $sslCertPasswd
	 * @access public
	 */
	var $sslCertPasswd;
	
	/**
	 * Whether or not to include the header in the results
	 * of the CURL transfer
	 *
	 * @var boolean $header
	 */
	var $header = 0;
	
	/**
	 * Whether or not to output debug information while executing a
	 * curl transfer
	 *
	 * @var boolean $verbose
	 * @access public
	 */
	var $verbose = 0;
	
	/**
	 * Whether or not to display a progress meter for the current transfer
	 *
	 * @var boolean $progress
	 * @access public
	 */
	var $progress = 0;
	
	/**
	 * Whether or not to suppress error messages
	 *
	 * @var boolean $mute
	 * @access public
	 */
	var $mute = 1;
	
	/**
	 * Whether or not to return the results of the
	 * current transfer
	 *
	 * @var boolean $return_transfer
	 * @access public
	 */
	var $return_transfer = 1;
	
	/**
	 * The type of transfer to perform
	 *
	 * @var string $type
	 * @access public
	 */
	var $type;
	
	/**
	 * The file to upload
	 *
	 * @var string $file
	 * @access public
	 */
	var $file;
	
	/**
	 * The file size of the file pointed to by the $file
	 * property
	 *
	 * @var integer $file_size
	 * @access public
	 */
	var $file_size;
	
	/**
	 * The cookies to send to the remote site
	 *
	 * @var array $cookies
	 * @access public
	 */
	var $cookies;
	
	/**
	 * The fields to send in a 'POST' request
	 *
	 * @var array $fields
	 * @access public
	 */
	var $fields;
	
	/**
	 * The proxy server to go through
	 *
	 * @var string $proxy
	 * @access public
	 */
	var $proxy;
	
	/**
	 * The username for the Proxy server
	 *
	 * @var string $proxyUser
	 * @access public
	 */
	var $proxyUser;
	
	/**
	 * The password for the Proxy server
	 *
	 * @var string $proxyPassword
	 * @access public
	 */
	var $proxyPassword;
	
	// }}}
	// {{{ Private Properties
	
	/**
	 * The current curl handle
	 *
	 * @var resource $_ch
	 * @access public
	 */
	var $_ch;
	
	// }}}
	// {{{ Net_Curl()
	
	/**
	 * The Net_Curl constructor, called when a new Net_Curl object
	 * is initialized
	 *
	 * @param string           [$url] The URL to fetch (can be set 
	 *                                using the $url property as well)
	 *
	 * @return object Net_Curl $obj   A new Net_Curl object
	 *
	 * @access public
	 * @author Sterling Hughes <sterling@php.net>
	 * @since  PHP 4.0.5
	 */
	function Net_Curl($url = "")
	{
		if ($url) {
			$this->url = $url;
		}
		
		$ch = curl_init();
		if (!$ch) {
			$this = new PEAR_Error("Couldn't initialize a new curl handle");
		}
		
		$this->_ch = $ch;
	}

	// }}}
	// {{{ execute()
	
	/**
	 * Executes a prepared CURL transfer
	 *
	 * @access public
	 * @author Sterling Hughes <sterling@php.net>
	 * @since  PHP 4.0.5
	 */
	function execute()
	{
		$ch  = &$this->_ch;
		$ret = true;
		
		// Basic stuff
		
		$ret = curl_setopt($ch, CURLOPT_URL,    $this->url);
		$ret = curl_setopt($ch, CURLOPT_HEADER, $this->header);
		
		// Whether or not to return the transfer contents
		if ($this->return_transfer && !$this->file) {
			$ret = curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
		}

		// SSL Checks
		
		if (isset($this->sslVersion)) {
			$ret = curl_setopt($ch, CURLOPT_SSLVERSION, $this->sslVersion);
		}
		
		if (isset($this->sslCert)) {
			$ret = curl_setopt($ch, CURLOPT_SSLCERT, $this->sslCert);
		}
		
		if (isset($this->sslCertPasswd)) {
			$ret = curl_setopt($ch, CURLOPT_SSLCERTPASSWD, $this->sslCertPasswd);
		}
		
		// Proxy Related checks

		if (isset($this->proxy)) {
			$ret = curl_setopt($ch, CURLOPT_PROXY, $this->proxy);
		}

		if (isset($this->proxyUser) || isset($this->proxyPassword)) {
			$proxyString = $this->proxyUser . ":" . $this->proxyPassword;
			
			$ret = curl_setopt($ch, CURLOPT_PROXYUSERPWD, $proxyString);
		}
		
		
		// Transfer type
		
		if (isset($this->type)) {
			switch (strtolower($this->type)) {
			case 'post':
				$ret = curl_setopt($ch, CURLOPT_POST, 1);
				break;
			case 'put':
				$ret = curl_setopt($ch, CURLOPT_PUT, 1);
				break;
			}
		}
		
		
		// Transfer upload, etc. related
		
		if (isset($this->file)) {
			if (!isset($this->file_size)) {
				$this->file_size = filesize($this->file);
			}
			
			$ret = curl_setopt($ch, CURLOPT_INFILE, $this->file);
			$ret = curl_setopt($ch, CURLOPT_INFILESIZE, $this->file_size);
		}
		
		if (isset($this->fields)) {
			if (!isset($this->type)) {
				$this->type = 'post';
				$ret = curl_setopt($ch, CURLOPT_POST, 1);
			}
			
			$ret = curl_setopt($ch, CURLOPT_POSTFIELDS, $this->fields);
		}
		
		
		// Error related
		
		if ($this->progress) {
			$ret = curl_setopt($ch, CURLOPT_PROGRESS, 1);
		}
		
		if ($this->verbose) {
			$ret = curl_setopt($ch, CURLOPT_VERBOSE, 1);
		}
		
		if (!$this->mute) {
			$ret = curl_setopt($ch, CURLOPT_MUTE, 0);
		}
		
		
		// Cookies and the such
		
		if (isset($this->cookies)) {
			foreach ($this->cookies as $name => $value) {
				$cookie_data .= urlencode($name) . ": " . urlencode($value);
				$cookie_data .= "\n";
			}
		
			$ret = curl_setopt($ch, CURLOPT_COOKIE, $cookie_data);
		}
		
		$ret = curl_exec($ch);
		if (!$ret) {
			$errObj = new PEAR_Error(curl_error($ch), curl_errno($ch));
			return($errObj);
		}
		
		return($ret);
	}
	
	// }}}
	// {{{ close()
	
	/**
	 * Closes the curl transfer and finishes the object (kinda ;)
	 *
	 * @access public
	 * @author Sterling Hughes <sterling@php.net>
	 * @since  PHP 4.0.5
	 */
	function close()
	{
		if ($this->_ch) {
			curl_close($this->_ch);
		}
	}
	
	// }}}
}

// }}}

?>