<?php
/* vim: set ts=4 sw=4: */
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
// | Authors: Rasmus Lerdorf <rasmus@php.net>                             |
// +----------------------------------------------------------------------+
//
// $Id: Passwd.php,v 1.1.1.2 2001/12/14 22:14:50 zarzycki Exp $
//
// Manipulate standard UNIX passwd,.htpasswd and CVS pserver passwd files

require_once 'PEAR.php' ;

/**
* Class to manage passwd-style files
*
* @author Rasmus Lerdorf <rasmus@php.net>
*/
class File_Passwd {
	
	/**
	* Passwd file
	* @var string
	*/
	var $filename ;
	
	/**
	* Hash list of users
	* @var array
	*/
    var $users ;
    
    /**
    * hash list of csv-users
    * @var array
    */
    var $cvs ;
    
    /**
    * filehandle for lockfile
    * @var int
    */
    var $fplock ;
    
    /**
    * locking state
    * @var boolean
    */
    var $locked ;
    
    /**
    * name of the lockfile
    * @var string    
    */ 
	var $lockfile = './passwd.lock';

	/**
	* Constructor
	* Requires the name of the passwd file. This functions opens the file and read it.
	* Changes to this file will written first in the lock file, so it is still possible
	* to access the passwd file by another programs. The lock parameter controls the locking
	* oft the lockfile, not of the passwd file! ( Swapping $lock and $lockfile would
	* breaks bc to v1.3 and smaller).
	* Don't forget to call close() to save changes!
	* 
	* @param $file		name of the passwd file
	* @param $lock		if 'true' $lockfile will be locked
	* @param $lockfile	name of the temp file, where changes are saved
	*
	* @access public
	* @see close() 
	*/

	function File_Passwd($file, $lock=0, $lockfile="") {
		$this->filename = $file;
		if( !empty( $lockfile) ) {
			$this->lockfile = $lockfile ;
			}

		$this->fplock = fopen($this->lockfile, 'w');
		if($lock) {
			flock($this->fplock, LOCK_EX);
			$this->locked = true;
		}

		$fp = fopen($file,'r') ;
		if( !$fp) {
			return new PEAR_Error( "Couldn't open '$file'!", 1, PEAR_ERROR_RETURN) ;
			}
		while(!feof($fp)) {
			$line = fgets($fp, 128);
			list($user,$pass,$cvsuser) = explode(':',$line);
			if(strlen($user)) {
				$this->users[$user] = $pass;
				$this->cvs[$user] = trim($cvsuser);	
			}
		}
		fclose($fp);
	} // end func File_Passwd()

	/**
	* Adds a user
	*
	* @param $user new user id
	* @param $pass password for new user
	* @param $cvs  cvs user id (needed for pserver passwd files)
	*
	* @return mixed returns PEAR_Error, if the user already exists
	* @access public
	*/
	function addUser($user,$pass,$cvsuser="") {
		if(!isset($this->users[$user]) && $this->locked) {
			$this->users[$user] = crypt($pass);
			$this->cvs[$user] = $cvsuser;
			return true;
		} else {
			return new PEAR_Error( "Couldn't add user '$user', because the user already exists!", 2, PEAR_ERROR_RETURN) ;
		}
	} // end func addUser()

	/**
	* Modifies a user
	*
	* @param $user user id
	* @param $pass new password for user
	* @param $cvs  cvs user id (needed for pserver passwd files)
	*
	* @return mixed returns PEAR_Error, if the user doesn't exists
	* @access public
	*/
	
	function modUser($user,$pass,$cvsuser="") {
		if(isset($this->users[$user]) && $this->locked) {
			$this->users[$user] = crypt($pass);
			$this->cvs[$user] = $cvsuser;
			return true;
		} else {
			return new PEAR_Error( "Couldn't modify user '$user', because the user doesn't exists!", 3, PEAR_ERROR_RETURN) ;
		}
	} // end func modUser()

	/**
	* Deletes a user
	*
	* @param $user user id
	*
	* @return mixed returns PEAR_Error, if the user doesn't exists
	* @access public	
	*/
	
	function delUser($user) {
		if(isset($this->users[$user]) && $this->locked) {
			unset($this->users[$user]);
			unset($this->cvs[$user]);
		} else {
			return new PEAR_Error( "Couldn't delete user '$user', because the user doesn't exists!", 3, PEAR_ERROR_RETURN) ; 
		}	
	} // end func delUser()

	/**
	* Verifies a user's password
	*
	* @param $user user id
	* @param $pass password for user
	*
	* @return boolean true if password is ok
	* @access public		
	*/
	function verifyPassword($user,$pass) {
		if(isset($this->users[$user])) {
			if($this->users[$user] == crypt($pass,substr($this->users[$user],0,2))) return true;
		}
		return false;
	} // end func verifyPassword()

	/**
	* Writes changes to passwd file and unlocks it
	*
	* @access public			
	*/
	function close() {
		if($this->locked) {
			foreach($this->users as $user => $pass) {
				if($this->cvs[$user]) {
					fputs($this->fplock, "$user:$pass:".$this->cvs[$user]."\n");
				} else {
					fputs($this->fplock, "$user:$pass\n");
				}
			}
			rename($this->lockfile,$this->filename);
			flock($this->fplock, LOCK_UN);
			$this->locked = false;
			fclose($this->fplock);
		}
	} // end func close()
}
?>
