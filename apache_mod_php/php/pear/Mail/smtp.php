<?php
//
// +----------------------------------------------------------------------+
// | PHP version 4.0                                                      |
// +----------------------------------------------------------------------+
// | Copyright (c) 1997, 1998, 1999, 2000 The PHP Group                   |
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

require_once 'Mail.php';

/**
 * SMTP implementation of the PEAR Mail:: interface. Requires the PEAR
 * Net_SMTP:: class.
 */
class Mail_smtp extends Mail {
    
	/**
     * The SMTP host to connect to.
     * @var	string
     */
    var $host = 'localhost';
    
	/**
     * The port the SMTP server is on.
     * @var	int
     */
    var $port = 25;
    
	/**
     * Whether or not to attempt to authenticate to the SMTP server.
     * @var boolean
     */
    var $auth = false;
    
	/**
     * The username to use if the SMTP server requires authentication.
     * @var string
     */
    var $username = '';
    
	/**
     * The password to use if the SMTP server requires authentication.
     * @var string
     */
    var $password = '';
    
	/**
     * Constructor.
     * 
     * Instantiates a new Mail_smtp:: object based on the parameters
     * passed in. It looks for the following parameters:
     *     host        The server to connect to. Defaults to localhost.
     *     port        The port to connect to. Defaults to 25.
     *     auth        Whether or not to use SMTP auth. Defaults to false.
     *     username    The username to use for SMTP auth. No default.
     *     password    The password to use for SMTP auth. No default.
     *
     * If a parameter is present in the $params array, it replaces the
     * default.
     *
     * @param array Hash containing any parameters different from the
     *              defaults.
     */	
    function Mail_smtp($params)
    {
        if (isset($params['host'])) $this->host = $params['host'];
        if (isset($params['port'])) $this->host = $params['port'];
        if (isset($params['auth'])) $this->host = $params['auth'];
        if (isset($params['username'])) $this->host = $params['username'];
        if (isset($params['password'])) $this->host = $params['password'];
    }
    
	/**
     * Implements Mail::send() function using SMTP.
     * 
     * @param mixed Either a comma-seperated list of recipients
     *              (RFC822 compliant), or an array of recipients,
     *              each RFC822 valid. This may contain recipients not
     *              specified in the headers, for Bcc:, resending
     *              messages, etc.
     *
     * @param array The array of headers to send with the mail, in an
     *              associative array, where the array key is the
     *              header name (ie, 'Subject'), and the array value
     *              is the header value (ie, 'test'). The header
     *              produced from those values would be 'Subject:
     *              test'.
     *
     * @param string The full text of the message body, including any
     *               Mime parts, etc.
     *
     * @return mixed Returns true on success, or a PEAR_Error
     *               containing a descriptive error message on
     *               failure.
     * @access public
     */
    function send($recipients, $headers, $body)
    {
        include_once 'Net/SMTP.php';
        
        if (!($smtp = new Net_SMTP($this->host, $this->port))) { return new PEAR_Error('unable to instantiate Net_SMTP object'); }
        if (!$smtp->connect()) { return new PEAR_Error('unable to connect to smtp server ' . $this->host . ':' . $this->port); }
        
        if ($this->auth) {
            if (!$smtp->auth($this->username, $this->password)) { return new PEAR_Error('unable to authenticate to smtp server'); }
            if (!$smtp->identifySender()) { return new PEAR_Error('unable to identify smtp server'); }
        }
        
        list($from, $text_headers) = $this->prepareHeaders($headers);
        if (!isset($from)) {
            return new PEAR_Error('No from address given');
        }
        
        if (!($smtp->mailFrom($from))) { return new PEAR_Error('unable to set sender to [' . $from . ']'); }
        
        $recipients = $this->parseRecipients($recipients);
        foreach($recipients as $recipient) {
            if (!$smtp->rcptTo($recipient)) { return new PEAR_Error('unable to add recipient [' . $recipient . ']'); }
        }
		
        if (!$smtp->data($text_headers . "\n" . $body)) { return new PEAR_Error('unable to send data'); }
        
        $smtp->disconnect();
        return true;
    }
    
}
?>
