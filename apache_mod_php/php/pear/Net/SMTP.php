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
// | Authors: Chuck Hagenbuch <chuck@horde.org>                           |
// +----------------------------------------------------------------------+

require_once 'PEAR.php';

/**
 * Provides an implementation of the SMTP protocol using PEAR's
 * Net_Socket:: class.
 */
class Net_SMTP extends PEAR {
    
	/**
     * The server to connect to.
     * @var string
     */
	var $host = 'localhost';
    
	/**
     * The port to connect to.
     * @var int
     */
	var $port = 25;
    
	/**
     * The value to give when sending EHLO or HELO.
     * @var string
     */
	var $localhost = 'localhost';
    
	/**
     * The socket resource being used to connect to the SMTP server.
     * @var resource
     */
	var $socket;
    
	/**
     * The most recent reply code
     * @var int
     */
	var $code;
    
	/**
     * Stores detected features of the SMTP server.
     * @var array
     */
	var $esmtp;
    
    /**
     * Constructor
     *
     * Instantiates a new Net_SMTP object, overriding any defaults
     * with parameters that are passed in.
     *
     * @param string The server to connect to.
     * @param int The port to connect to.
     * @param string The value to give when sending EHLO or HELO.
     */
	function Net_SMTP($host = null, $port = null, $localhost = null) {
        if (isset($host)) $this->host = $host;
        if (isset($port)) $this->port = $port;
        if (isset($localhost)) $this->localhost = $localhost;
	}
    
    /**
     * Attempt to connect to the SMTP server.
     *
     * @return mixed Returns a PEAR_Error with an error message on any
     *               kind of failure, or true on success.
     * @access public
     */
    function connect() {
        include_once 'Net/Socket.php';
        
		if (PEAR::isError($this->socket = new Net_Socket())) { return new PEAR_Error('unable to create a socket object'); }
		if (PEAR::isError($this->socket->connect($this->host, $this->port))) { return new PEAR_Error('unable to open socket'); }
        
		if (PEAR::isError($this->validateResponse('220'))) { return new PEAR_Error('smtp server not 220 ready'); }
		if (!$this->identifySender()) { return new PEAR_Error('unable to identify smtp server'); }
        
		return true;
	}
    
    /**
     * Attempt to disconnect from the SMTP server.
     *
     * @return mixed Returns a PEAR_Error with an error message on any
     *               kind of failure, or true on success.
     * @access public
     */
	function disconnect() {
		if (PEAR::isError($this->socket->write("QUIT\r\n"))) { return new PEAR_Error('write to socket failed'); }
		if (!$this->validateResponse('221')) { return new PEAR_Error('221 Bye not received'); }
		if (PEAR::isError($this->socket->disconnect())) { return new PEAR_Error('socket disconnect failed'); }
        
		return true;
	}
    
    /**
     * Attempt to do SMTP authentication.
     *
     * @param string The userid to authenticate as.
     * @param string The password to authenticate with.
     *
     * @return mixed Returns a PEAR_Error with an error message on any
     *               kind of failure, or true on success.
     * @access public
     */
	function auth($uid, $pwd) {
		/* Note: not currently checking if AUTH LOGIN is allowed */	
		/* Note: only allows one authentication mechanism */ 
        
		if (!isset($this->esmtp['AUTH'])) { return new PEAR_Error('auth not supported'); }
        
		if (PEAR::isError($this->socket->write("AUTH LOGIN\r\n"))) { return new PEAR_Error('write to socket failed'); }
		if (!$this->validateResponse('334')) { return new PEAR_Error('AUTH LOGIN not recognized'); }
        
		if (PEAR::isError($this->socket->write(base64_encode($uid) . "\n"))) { return new PEAR_Error('write to socket failed'); }
		if (!$this->validateResponse('334')) { return new PEAR_Error('354 not received'); }
        
		if (PEAR::isError($this->socket->write(base64_encode($pwd) . "\n"))) { return new PEAR_Error('write to socket failed'); }
		if (!$this->validateResponse('235')) { return new PEAR_Error('235 not received'); }
        
		return true;
	}
    
    /**
     * Send the HELO command.
     * 
     * @param string The domain name to say we are.
     *
     * @return mixed Returns a PEAR_Error with an error message on any
     *               kind of failure, or true on success.
     * @access public
     */
	function helo($domain) {
		if (PEAR::isError($this->socket->write("HELO $domain\r\n"))) { return new PEAR_Error('write to socket failed'); }
		if (!($this->validateResponse('250'))) { return new PEAR_Error('250 OK not received'); }
        
		return true;
	}
    
    /**
     * Send the MAIL FROM: command.
     * 
     * @param string The sender (reverse path) to set.
     *
     * @return mixed Returns a PEAR_Error with an error message on any
     *               kind of failure, or true on success.
     * @access public
     */
	function mailFrom($reverse_path) {
		if (PEAR::isError($this->socket->write("MAIL FROM:<$reverse_path>\r\n"))) { return new PEAR_Error('write to socket failed'); }
		if (!($this->validateResponse('250'))) { return new PEAR_Error('250 OK not received'); }
        
		return true;
	}
    
    /**
     * Send the RCPT TO: command.
     * 
     * @param string The recipient (forward path) to add.
     *
     * @return mixed Returns a PEAR_Error with an error message on any
     *               kind of failure, or true on success.
     * @access public
     */
	function rcptTo($forward_path) {
		/* Note: 251 is also a valid response code */
        
		if (PEAR::isError($this->socket->write("RCPT TO:<$forward_path>\r\n"))) { return new PEAR_Error('write to socket failed'); }
		if (!($this->validateResponse('250'))) { return new PEAR_Error('250 OK not received'); }
        
		return true;
	}
    
    /**
     * Send the DATA command.
     * 
     * @param string The message body to send.
     *
     * @return mixed Returns a PEAR_Error with an error message on any
     *               kind of failure, or true on success.
     * @access public
     */
	function data($data) {
		$data = preg_replace("/([^\r]{1})\n/", "\\1\r\n", $data);
		$data = preg_replace("/\n\n/", "\n\r\n", $data);
		$data = preg_replace("/^(\..*)/", ".\\1", $data);
        
		if (PEAR::isError($this->socket->write("DATA\r\n"))) { return new PEAR_Error('write to socket failed'); }
		if (!($this->validateResponse('354'))) { return new PEAR_Error('354 not received'); }
		if (PEAR::isError($this->socket->write($data . "\r\n.\r\n"))) { return new PEAR_Error('write to socket failed'); }
		if (!($this->validateResponse('250'))) { return new PEAR_Error('250 OK not received'); }
        
		return true;
	}
    
    /**
     * Send the SEND FROM: command.
     * 
     * @param string The reverse path to send.
     *
     * @return mixed Returns a PEAR_Error with an error message on any
     *               kind of failure, or true on success.
     * @access public
     */
	function send_from($reverse_path) {
		if (PEAR::isError($this->socket->write("SEND FROM:<$reverse_path>\r\n"))) { return new PEAR_Error('write to socket failed'); }
		if (!($this->validateResponse('250'))) { return new PEAR_Error('250 OK not received'); }
        
		return true;
	}
    
    /**
     * Send the SOML FROM: command.
     * 
     * @param string The reverse path to send.
     *
     * @return mixed Returns a PEAR_Error with an error message on any
     *               kind of failure, or true on success.
     * @access public
     */
	function soml_from($reverse_path) {
		if (PEAR::isError($this->socket->write("SOML FROM:<$reverse_path>\r\n"))) { return new PEAR_Error('write to socket failed'); }
		if (!($this->validateResponse('250'))) { return new PEAR_Error('250 OK not received'); }
        
		return true;
	}
    
    /**
     * Send the SAML FROM: command.
     * 
     * @param string The reverse path to send.
     *
     * @return mixed Returns a PEAR_Error with an error message on any
     *               kind of failure, or true on success.
     * @access public
     */
	function saml_from($reverse_path) {
		if (PEAR::isError($this->socket->write("SAML FROM:<$reverse_path>\r\n"))) { return new PEAR_Error('write to socket failed'); }
		if (!($this->validateResponse('250'))) { return new PEAR_Error('250 OK not received'); }
        
		return true;
	}
    
    /**
     * Send the RSET command.
     * 
     * @return mixed Returns a PEAR_Error with an error message on any
     *               kind of failure, or true on success.
     * @access public
     */
	function rset() {
		if (PEAR::isError($this->socket->write("RSET\r\n"))) { return new PEAR_Error('write to socket failed'); }
		if (!($this->validateResponse('250'))) { return new PEAR_Error('250 OK not received'); }
        
		return true;
	}
    
    /**
     * Send the VRFY command.
     * 
     * @param string The string to verify
     *
     * @return mixed Returns a PEAR_Error with an error message on any
     *               kind of failure, or true on success.
     * @access public
     */
	function vrfy($string) {
		/* Note: 251 is also a valid response code */
		if (PEAR::isError($this->socket->write("VRFY $string\r\n"))) { return new PEAR_Error('write to socket failed'); }
		if (!($this->validateResponse('250'))) { return new PEAR_Error('250 OK not received'); }
        
		return true;
	}
    
    /**
     * Send the NOOP command.
     * 
     * @return mixed Returns a PEAR_Error with an error message on any
     *               kind of failure, or true on success.
     * @access public
     */
	function noop() {
		if (PEAR::isError($this->socket->write("NOOP\r\n"))) { return new PEAR_Error('write to socket failed'); }
		if (!($this->validateResponse('250'))) { return new PEAR_Error('250 OK not received'); }
        
		return true;
	}
    
    /**
     * Attempt to send the EHLO command and obtain a list of ESMTP
     * extensions available, and failing that just send HELO.
     * 
     * @return mixed Returns a PEAR_Error with an error message on any
     *               kind of failure, or true on success.
     * @access private
     */
	function identifySender() {
		if (PEAR::isError($this->socket->write("EHLO $this->localhost\r\n"))) { return new PEAR_Error('write to socket failed'); }
        
        $extensions = array();
		if (!($this->validateAndParseResponse('250', $extensions))) { 
			if (PEAR::isError($this->socket->write("HELO $this->localhost\r\n"))) { return new PEAR_Error('write to socket failed'); }
			if (!($this->validateResponse('250'))) { return new PEAR_Error('HELO not accepted', $this->code); }
			return true;
		}	
        
		for ($i = 0; $i < count($extensions); $i++) {
			$verb = strtok($extensions[$i], ' ');
			$arguments = substr($extensions[$i], strlen($verb) + 1, strlen($extensions[$i]) - strlen($verb) - 2);
			$this->esmtp[$verb] = $arguments;
		}
		return true;
	}
    
    /**
     * Read a response from the server and see if the response code
     * matches what we are expecting.
     * 
     * @param int The response code we are expecting.
     *
     * @return boolean True if we get what we expect, false otherwise.
     * @access private
     */
	function validateResponse($code) {
		while ($response = $this->socket->readLine()) {
			$reply_code = strtok($response, ' ');
			if (!(strcmp($code, $reply_code))) {
				$this->code = $reply_code;
				return true;
			} else {
				$reply_code = strtok($response, '-');
				if (strcmp($code, $reply_code)) {
					$this->code = $reply_code;
					return false;
				}
			}
		}
        
        return false;
	}
    
    /**
     * Read a response from the server and see if the response code
     * matches what we are expecting. Also save the rest of the
     * response in the array passed by reference as the second
     * argument.
     *
     * @param int The response code we are expecting.
     * @param array An array to dump the rest of the response into.
     *
     * @return boolean True if we get what we expect, false otherwise.
     * @access private
     */
	function validateAndParseResponse($code, &$arguments) {
		$arguments = array();
        
		while ($response = $this->socket->readLine()) {
			$reply_code = strtok($response, ' ');
			if (!(strcmp($code, $reply_code))) {
				$arguments[] = substr($response, strlen($code) + 1, strlen($response) - strlen($code) - 1);
				$this->code = $reply_code;
				return true;
			} else {
				$reply_code = strtok($response, '-');
				if (strcmp($code, $reply_code)) {
					$this->code = $reply_code;
					return false;
				}
			}
			$arguments[] = substr($response, strlen($code) + 1, strlen($response) - strlen($code) - 1);
		}
        
        return false;
	}
    
}
?>
