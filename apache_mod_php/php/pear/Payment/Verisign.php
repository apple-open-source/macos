<?php /* -*- C++ -*- */
/*
   +----------------------------------------------------------------------+
   | PHP version 4.0                                                      |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2001 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: David Croft: <david@infotrek.co.uk>                          |
   +----------------------------------------------------------------------+
*/

/* $Id: Verisign.php,v 1.1.1.2 2001/07/19 00:20:50 zarzycki Exp $ */

/*                 ******** NOTICE ********
 *
 *   This code is completely untested. Do not use it!
 *   It is here for peer review. It will undergo revision, bugfixes
 *   and probably API changes. It must not be used yet!
 */

/* Verisign Payment Processing class.
 *
 * Some test cards:
 * Amex:       378282246310005
 * Discover:   6011111111111117
 * Mastercard: 5105105105105100 or 5555555555551111
 * Visa:       4242424242424242 or 4111111111111111
*/

require_once 'PEAR.php';


/* Symbolic names for decoded response codes */
/* We want as few of these as possible so calling code has fewer branches */

define("VERISIGN_SUCCESS",                 0);

/* 10+ - user errors */

define("VERISIGN_BAD_CARD",               10);
define("VERISIGN_INVALID_EXPIRY",         11);
define("VERSIGIN_BAD_CARD_TYPE",          12);

/* 20+ - our errors */

define("VERISIGN_CONFIG_ERROR",           20);
define("VERISIGN_UNRECOGNISED_RESPONSE",  21);

/* 30+ - temporary errors, try again later */

define("VERISIGN_TEMPORARY_ERROR",        30);
define("VERISIGN_REFERRAL",               31);


class Payment_Verisign
{
  /* These are all private variables. DO NOT ACCESS THEM DIRECTLY. */
  /* There are specific functions to set each of these */

  var $hostaddress = 'test.signio.com';
  var $hostport = 443;

  var $timeout = 30;

  var $proxyaddress;
  var $proxyport;
  var $proxylogon;
  var $proxypassword;

  var $username;
  var $password;

  var $cardnumber;
  var $expiredate;

  var $params = array();

  function Payment_Verisign()
  {
    /* can you return stuff from a constructor? */

    if (!extension_loaded('pfpro')) {
      return new VerisignException('Payflow Pro module is not compiled into PHP. Compile PHP using --with-pfpro.');
    }
  }

  function set_host($hostaddress, $hostport = 443)
  {
    if (!$hostaddress or !strlen($hostaddress)) {
      return new VerisignException('Invalid host address');
    }

    if (!$hostport or $hostport < 1) {
      return new VerisignException('Invalid host port');
    }

    $this->hostaddress = $hostaddress;
    $this->hostport = $hostport;
  }

  function set_timeout($timeout)
  {
    if ($timeout < 1 or $timeout > 3600) {
      return new VerisignException('Invalid timeout value');
    }

    $this->timeout = $timeout;
  }

  function set_logon($username, $password)
  {
    if (!$username or !strlen($username)) {
      return new VerisignException('Invalid user name');
    }

    if (!$password or !strlen($password)) {
      return new VerisignException('Invalid password');
    }

    $this->username = $username;
    $this->password = $password;
  }

  function set_proxy($proxyaddress, $proxyport = 443, $proxylogon = '', $proxypassword = '')
  {
    if (!$proxyaddress or !strlen($proxyaddress)) {
      return new VerisignException('Invalid proxy address');
    }

    if (!$proxyport or $proxyport < 1) {
      return new VerisignException('Invalid proxy port');
    }
    
    $this->proxyaddress = $proxyaddress;
    $this->proxyport = $proxyport;

    if ($proxylogon and strlen($proxylogon)) {
      $this->proxypassword = $proxylogon;
    }

    if ($proxylogon and strlen($proxypassword)) {
      $this->proxypassword = $proxypassword;
    }
  }

  function set_card($cardnumber, $expiremonth, $expireyear)
  {
    $this->cardnumber = $cardnumber;

    if ($expiremonth < 1 or $expiremonth > 12) {
      return new VerisignException('Invalid expiry month');
    }

    if (($expireyear > 99 and $expireyear < 2000) or $expireyear > 2100) {
      return new VerisignException('Invalid expiry year');
    }

    $this->expiredate = sprintf("%02d%02d", $expiremonth, ($expireyear > 100) ? ($expireyear % 100) : $expireyear);

  }

  function set_avs($street, $zip)
  {
    if (!$street or !is_string($street) or !strlen($street)) {
      return new VerisignException('AVS Street was not specified or was not a string');
    }
    if (!$zip or !is_string($zip) or !strlen($zip)) {
      return new VerisignException('AVS Zip was not specified or was not a string');
    }

    $this->params['STREET'] = $street;
    $this->params['ZIP'] = $zip;
  }

  function add_parameter($key, $value)
  {
    if (!$key or !is_string($key) or !strlen($key)) {
      return new VerisignException('Key must be a string');
    }

    $this->params[$key] = $value;
  }

  function add_parameters($newparams)
  {
    foreach ($newparams as $key => $value) {
      if (!$key or !is_string($key) or !strlen($key)) {
	return new VerisignException('Keys must be strings');
      }
    }

    $this->params = array_merge($this->params, $newparams);
  }

  function reset_parameters()
  {
    $this->params = array();
  }

  function set_comment1($comment)
  {
    $this->params['COMMENT1'] = $comment;
  }

  function set_comment2($comment)
  {
    $this->params['COMMENT2'] = $comment;
  }

  /* Functions to process transactions */

  /* sale: authorise and immediate capture */

  function sale($amount)
  {
    if (!$amount or $amount <= 0) {
      return new VerisignException('You cannot perform a sale on a negative or zero amount');
    }

    $this->params['AMT'] = number_format($amount, 2, '.', '');

    return $this->process('S');
  }

  /* authorise: authorise only for capture later with capture() */

  function authorize($amount)
  {
    /* for the yanks */
    return $this->authorise($amount);
  }

  function authorise($amount)
  {
    if (!$amount or $amount <= 0) {
      return new VerisignException('You cannot perform an authorisation on a negative or zero amount');
    }

    $this->params['AMT'] = number_format($amount, 2, '.', '');

    return $this->process('A');
  }

  /* capture: capture an authorised transaction */

  function capture($origid)
  {
    if (!$origid or !strlen($origid)) {
      return new VerisignException('You must provide the original PNREF id for a delayed capture');
    }

    $this->params['ORIGID'] = $origid;

    return $this->process('D');
  }

  /* refund: give money back (ouch) */

  function refund($amount)
  {
    if (!$amount or $amount <= 0) {
      return new VerisignException('You cannot perform an authorisation on a negative or zero amount');
    }

    $this->params['AMT'] = number_format($amount, 2, '.', '');

    return $this->process('C');
  }

  /* refund_transaction: refund a specific earlier transaction - no card required */

  function refund_transaction($origid)
  {
    if (!$origid or !strlen($origid)) {
      return new VerisignException('You must provide the original PNREF id for a transaction refund');
    }

    $this->params['ORIGID'] = $origid;

    return $this->process('C');
  }

  /* voice_auth: voice authorised transaction */

  function voice_auth($amount, $authcode)
  {
    if (!$amount or $amount <= 0) {
      return new VerisignException('You cannot perform an authorisation on a negative or zero amount');
    }

    if (!$authcode or !strlen($authcode)) {
      return new VerisignException('You must provide an authcode for a voice authorisation');
    }

    $this->params['AMT'] = number_format($amount, 2, '.', '');

    $this->params['AUTHCODE'] = $authcode;

    return $this->process('F');
  }

  function void($origid)
  {
    if (!$origid or !strlen($origid)) {
      return new VerisignException('You must provide the original PNREF id for a transaction refund');
    }

    $this->params['ORIGID'] = $origid;

    return $this->process('V');
  }

  /* Central processing function */

  function process($trxtype, $tender = 'C')
  {
    if (!extension_loaded('pfpro')) {
      return new VerisignException('Payflow Pro module is not compiled into PHP. Compile PHP using --with-pfpro.');
    }

    if (!$this->params['USER']) {
      if ($this->username and strlen($this->username)) {
	$this->params['USER'] = $this->username;
      }
      else {
	return new VerisignException('You have not specified the Verisign user name');
      }
    }

    if (!$this->params['PWD']) {
      if ($this->password and strlen($this->password)) {
	$this->params['PWD'] = $this->username;
      }
      else {
	return new VerisignException('You have not specified the Verisign password');
      }
    }

    if (!$this->params['ACCT']) {
      if ($this->cardnumber and strlen($this->cardnumer)) {
	$this->params['ACCT'] = $this->cardnumber;
      }
      else if ($this->params['ORIGID'] and
	       ($trxtype == 'C' or $trxtype == 'D' or $trxtype == 'V')) {
	/* don't need card number if we're crediting or capturing
	 * or voiding a specific transaction */
      }
      else {
	return new VerisignException('You have not specified the card number');
      }
    }

    if (!$this->params['EXPDATE']) {
      if ($this->expiredate and strlen($this->expiredate)) {
	$this->params['EXPDATE'] = $this->expiredate;
      }
      else if ($this->params['ORIGID'] and
	       ($trxtype == 'C' or $trxtype == 'D' or $trxtype == 'V')) {
	/* don't need expiry date if we're crediting or capturing
	 * or voiding a specific transaction */
      }
      else {
	return new VerisignException('You have not specified the expiry date');
      }
    }

    if (!$trxtype or !strlen($trxtype)) {
      return new VerisignException('Invalid transaction type');
    }

    $this->params['TRXTYPE'] = $trxtype;

    if (!$tender or !strlen($tender)) {
      return new VerisignException('Invalid tender');
    }

    $this->params['TENDER'] = $tender;

    /* Some final validation of our member variables */

    if (!$this->hostaddress or !strlen($this->hostaddress)) {
      return new VerisignException('No host address specified');
    }

    if (!$this->hostport or $this->hostport < 1) {
      return new VerisignException('No host port specified');
    }

    /* Now process it */

    if ($this->proxypassword and strlen($this->proxypassword)) {
      $response = pfpro_process($this->params, $this->hostaddress, $this->hostport, $this->timeout, $this->proxyaddress, $this->proxyport, $this->proxylogon, $this->proxypassword);
    }
    else if ($this->proxylogon and strlen($this->proxylogon)) {
      $response = pfpro_process($this->params, $this->hostaddress, $this->hostport, $this->timeout, $this->proxyaddress, $this->proxyport, $this->proxylogon);
    }
    else if ($this->proxyport and $this->proxyport > 0) {
      $response = pfpro_process($this->params, $this->hostaddress, $this->hostport, $this->timeout, $this->proxyaddress, $this->proxyport);
    }
    else if ($this->proxyaddress and strlen($this->proxyaddress)) {
      $response = pfpro_process($this->params, $this->hostaddress, $this->hostport, $this->timeout, $this->proxyaddress);
    }
    else if ($this->timeout and $this->timeout > 0) {
      $response = pfpro_process($this->params, $this->hostaddress, $this->hostport, $this->timeout);
    }
    else if ($this->hostport and $this->hostport > 0) {
      $response = pfpro_process($this->params, $this->hostaddress, $this->hostport);
    }
    else if ($this->hostaddress and ($this->hostaddress)) {
      $response = pfpro_process($this->params, $this->hostaddress);
    }
    else {
      $response = pfpro_process($this->params);
    }

    if (!$response) {
      return new VerisignException('No response received from Payflow Pro!');
    }

    if (!$response['RESULT']) {
      $response['RESULT'] = -999;
    }

    return $response;
  }

  /* Decode the myriad of Verisign response codes into a meaningful few */



  function decode_response($code)
  {
    if (is_array($code)) {
      $code = $code['RESULT'];
    }
    else if (!is_integer($code)) {
      return new VerisignException('Result code must be an integer');
    }

    switch ($code) {

    case 0:    /* Approved                                  */
      return VERISIGN_SUCCESS;

    case 12:   /* Declined                                  */
    case 23:   /* Invalid account number                    */
    case 50:   /* Insufficient funds available              */
    case 112:  /* Failed AVS check                          */
      return VERISIGN_BAD_CARD;

    case 24:   /* Invalid expiry date                       */
      return VERISIGN_INVALID_EXPIRY;

    case 2:    /* Invalid tender                            */
	return VERISIGN_BAD_CARD_TYPE;

    case 1:    /* User authentication failed                */
    case 3:    /* Invalid transaction type                  */
    case 4:    /* Invalid amount                            */
    case 5:    /* Invalid merchant information              */
    case 7:    /* Field format error                        */
    case 19:   /* Original transaction ID not found         */
    case 20:   /* Cannot find the customer reference number */
    case 22:   /* Invalid ABA number                        */
    case 25:   /* Transaction type not mapped to this host  */
    case 100:  /* Invalid transaction returned from host    */
    case 105:  /* Credit error                              */
    case 108:  /* Void error                                */
    case 111:  /* Capture error                             */
    case 113:  /* Cannot exceed sales cap                   */
      return VERISIGN_CONFIG_ERROR;

    case 11:   /* Client timeout waiting for response       */
    case 101:  /* Timeout value too small                   */
    case 102:  /* Host unavailable                          */
    case 103:  /* Error reading response from host          */
    case 104:  /* Tiemout waiting for host response         */
    case -1:   /* Server socket unavailable                 */
    case -2:   /* Hostname lookup failed                    */
    case -3:   /* Client time out                           */
    case -4:   /* Socket initialisation error               */
    case -5:   /* SSL context initialisation failed         */
    case -6:   /* SSL verification policy error             */
    case -7:   /* SSL verify location error                 */
    case -8:   /* X509 certification verification error     */
    case -99:  /* Internal error to the library             */
    case -999: /* Error from this class, no result found    */
      return VERISIGN_TEMPORARY_ERROR;

    case 13:   /* Referral                                  */
      return VERISIGN_REFERRAL;
    }

    /* 99: General error */
    /* 1000: Generic host error */
    /* and any others */

    return VERISIGN_UNRECOGNISED_RESPONSE;
  }


  /*
   * bool isError ($var)
   *  Determine whether or not a variable is a PEAR exception
   */
  function isError ($var)
  {
    return (is_object ($var) and
	    substr (get_class ($var), -9) == 'Exception');
  }
	
  /*
   *    double Payment_Verisign_version(void)
   *      Returns the current Payment_Verisign version.
   */
  function Payment_Verisign_version()
  {
    return (0.1);
  }

}

class VerisignException extends PEAR_Error
{
  var $classname             = 'VerisignException';
  var $error_message_prepend = 'Error in Payment_Verisign';

  function VerisignException($message)
  {
    $this->PEAR_Error($message);
  }
}

?>
