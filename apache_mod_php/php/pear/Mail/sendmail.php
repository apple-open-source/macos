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
 * Sendmail implementation of the PEAR Mail:: interface.
 */
class Mail_sendmail extends Mail {
    
	/**
     * The location of the sendmail binary on the filesystem.
     * @var string
     */
    var $sendmail_path = '/usr/sbin/sendmail';
    
	/**
     * Constructor.
     * 
     * Instantiates a new Mail_sendmail:: object based on the parameters
     * passed in. It looks for the following parameters:
     *     sendmail_path    The location of the sendmail binary on the
     *                      filesystem. Defaults to '/usr/sbin/sendmail'.
     *
     * If a parameter is present in the $params array, it replaces the
     * default.
     *
     * @param array Hash containing any parameters different from the
     *              defaults.
     */	
    function Mail_sendmail($params)
    {
        if (isset($params['sendmail_path'])) $this->sendmail_path = $params['sendmail_path'];
    }
    
	/**
     * Implements Mail::send() function using the sendmail
     * command-line binary.
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
        $recipients = escapeShellCmd(implode(' ', $this->parseRecipients($recipients)));
        
        list($from, $text_headers) = $this->prepareHeaders($headers);
        if (!isset($from)) {
            return new PEAR_Error('No from address given');
        }
        
        $result = 0;
        if (@is_executable($this->sendmail_path)) {
            $from = '"' . escapeShellCmd($from) . '"';
            $mail = popen($this->sendmail_path . " -i -f$from -- $recipients", 'w');
            fputs($mail, $text_headers);
            fputs($mail, "\n");  // newline to end the headers section
            fputs($mail, $body);
            $result = pclose($mail) >> 8 & 0xFF; // need to shift the pclose result to get the exit code
        } else {
            return new PEAR_Error('sendmail [' . $this->sendmail_path . '] not executable');
        }
        
        // Return.
        if ($result != 0) {
            return new PEAR_Error('sendmail returned error code ' . $result);
        }
        
        return true;
    }
    
}
?>
