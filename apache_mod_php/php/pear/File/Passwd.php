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
// $Id: Passwd.php,v 1.1.1.1 2001/07/19 00:20:48 zarzycki Exp $
//
// Manipulate standard UNIX passwd,.htpasswd and CVS pserver passwd files

/**
* Class to manage passwd-style files
*
* @author Rasmus Lerdorf <rasmus@php.net>
*/
class File_Passwd {
	var $filename, $users, $cvs, $fplock, $locked;
	var $lockfile = './passwd.lock';

	function File_Passwd($file,$lock=0) {
		$this->filename = $file;

		$this->fplock = fopen($this->lockfile, 'w');
		if($lock) {
			flock($this->fplock, LOCK_EX);
			$this->locked = true;
		}

		$fp = fopen($file,'r') or die("Unable to open $file");
		while(!feof($fp)) {
			$line = fgets($fp, 128);
			list($user,$pass,$cvsuser) = explode(':',$line);
			if(strlen($user)) {
				$this->users[$user] = $pass;
				$this->cvs[$user] = trim($cvsuser);	
			}
		}
		fclose($fp);
	}

	/**
	* Adds a user
	*
	* @param $user new user id
	* @param $pass password for new user
	* @param $cvs  cvs user id (needed for pserver passwd files)
	*/
	function addUser($user,$pass,$cvsuser) {
		if(!isset($this->users[$user]) && $this->locked) {
			$this->users[$user] = crypt($pass);
			$this->cvs[$user] = $cvsuser;
			return true;
		} else {
			return false;
		}
	}

	/**
	* Modifies a user
	*
	* @param $user user id
	* @param $pass new password for user
	* @param $cvs  cvs user id (needed for pserver passwd files)
	*/
	function modUser($user,$pass,$cvsuser) {
		if(isset($this->users[$user]) && $this->locked) {
			$this->users[$user] = crypt($pass);
			$this->cvs[$user] = $cvsuser;
			return true;
		} else {
			return false;
		}
	}

	/**
	* Deletes a user
	*
	* @param $user user id
	*/
	function delUser($user) {
		if(isset($this->users[$user]) && $this->locked) {
			unset($this->users[$user]);
			unset($this->cvs[$user]);
		} else {
			return false;
		}	
	}

	/**
	* Verifies a user's password
	*
	* @param $user user id
	* @param $pass password for user
	*/
	function verifyPassword($user,$pass) {
		if(isset($this->users[$user])) {
			if($this->users[$user] == crypt($pass,substr($this->users[$user],0,2))) return true;
		}
		return false;
	}

	/**
	* Writes changes to passwd file and unlocks it
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
	}
}
?>
