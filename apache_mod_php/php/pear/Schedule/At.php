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
// | Authors: Colin Viebrock <colin@easydns.com>                          |
// +----------------------------------------------------------------------+
//
// $Id: At.php,v 1.1.1.1 2001/07/19 00:20:51 zarzycki Exp $
//
// Interface to the UNIX "at" program

/**
* Class to interface to the UNIX "at" program
*
* @author Colin Viebrock <colin@easydns.com>
*/

require_once 'PEAR.php';

class Schedule_At extends PEAR {

    var $AT_PROG    = '/usr/bin/at';

    var $error      = false;
    var $runtime    = false;
    var $job        = false;

    var $lastexec   = '';


    /**
    * Constructor: instantiates the class.
    *
    * @access public
    *
    */
    function Schedule_At()
    {
	$this->PEAR();
        $this->_reset();
    }


    /**
    * Adds an at command
    *    This makes an "at" job, where $cmd is the shell command to run
    *    and $timespec describes when the function should run.  See the
    *    at man page for a description of the spec.
    *
    *    $queue is an optional 1 character string [a-zA-Z] that can define
    *    which queue to put the job in.
    *
    *    If $mail is set, then an email will be sent when the job runs,
    *    even if the job doesn't output anything.  The mail gets sent to
    *    the user running the script (probably the webserver, i.e.
    *    nobody@localhost).
    *
    *    The add() method returns false on error (in which case, check
    *    $at->error for the message), or the job number on success.
    *    On succes, $at->runtime is also set with the timestamp corresponding
    *    to when the job will run.
    *
    * @param $cmd        shell command
    * @param $timespec   time when command should run, formatted accoring to the spec for at
    * @param $queue      optional at queue specifier
    * @param $mail       optional flag to specify whether to send email
    *
    * @access public
    *
    */
    function add($cmd, $timespec, $queue = false, $mail = false )
    {

        $this->_reset();

        if ($queue && !preg_match('/^[a-zA-Z]{1,1}$/', $queue) ) {
            $this->error = new Schedule_At_Error('Invalid queue specification');
            return $this->error;
        }

        $cmd = escapeShellCmd($cmd);

        $exec = sprintf("echo \"%s\" | %s %s %s %s 2>&1",
            addslashes($cmd),
            $this->AT_PROG,
            ($queue ? '-q '.$queue : ''),
            ($mail ? '-m' : ''),
            $timespec
        );

        $result = $this->_doexec($exec);

        if (preg_match('/garbled time/i', $result) ) {
            $this->error = new Schedule_At_Error('Garbled time');
            return $this->error;
        }

        if (preg_match('/job (\d+) at (.*)/i', $result, $m) ) {
            $this->runtime = $this->_parsedate($m[2]);
            $this->job = $m[1];
            return $this->job;
        } else {
            $this->error = new Schedule_At_Error('Exec Error: '.$result);
            return $this->error;
        }

    }


    /**
    * Shows commands in the at queue
    *
    *    This returns an array listing all queued jobs.  The array's keys
    *    are the job numbers, and each entry is itself an associative array
    *    listing the runtime (timestamp) and queue (char).
    *
    *    You can optionally provide a queue character to only list the jobs
    *    in that queue.
    *
    * @param $queue        optional queue specifier
    *
    * @access public
    *
    */
    function show($queue = false)
    {

        $this->_reset();

        if ($queue && !preg_match('/^[a-zA-Z]{1,1}$/', $queue) ) {
            $this->error = new Schedule_At_Error('Invalid queue specification');
            return $this->error;
        }

        $exec = sprintf("%s -l %s",
            $this->AT_PROG,
            ($queue ? '-q '.$queue : '')
        );

        $result = $this->_doexec($exec);
        $lines = explode("\n", $result);

        $return = array();

        foreach($lines as $line) {
            if (trim($line)) {
                list($job, $day, $time, $queue) = preg_split('/\s+/', trim($line) );
                $return[$job] = array(
                    'runtime'    => $this->_parsedate($day.' '.$time),
                    'queue'        => $queue
                );
            }
        }

        return $return;

    }


    /**
    * Remove job from the at queue
    *
    *    This removes jobs from the queue.  Returns false if the job doesn't
    *    exist or on failure, or true on success.
    *
    * @param $job        job to remove
    *
    * @access public
    *
    */
    function remove($job = false)
    {
        $this->_reset();

        if (!$job) {
            $this->error = new Schedule_At_Error('No job specified');
            return $this->error;
        }

        $queue = $this->show();

        if (!isset($queue[$job]) ) {
            $this->error = new Schedule_At_Error('Job ' . $job . ' does not exist');
            return $this->error;
        }

        $exec = sprintf("%s -d %s",
            $this->AT_PROG,
            $job
        );

        $this->_doexec($exec);

        /* this is required since the shell command doesn't return anything on success */

        $queue = $this->show();
        return !isset($queue[$job]);

    }


    /**
    * PRIVATE: Reset class
    *
    *
    * @access private
    *
    */
    function _reset()
    {
        $this->error      = false;
        $this->runtime    = false;
        $this->job        = false;
        $this->lastexec   = '';
    }


    /**
    * PRIVATE: Parse date string returned from shell command
    *
    * @param $str    date string to parse
    *
    * @access private
    *
    */
    function _parsedate($str)
    {
        if (preg_match('/(\d{4})-(\d{2})-(\d{2}) (\d{2}):(\d{2})/i', $str, $m) ) {
            return mktime($m[4], $m[5], 0, $m[2], $m[3], $m[1]);
        } else {
            return false;
        }
    }


    /**
    * PRIVATE: Run a shell command
    *
    * @param $cmd    command to run
    *
    * @access private
    *
    */
    function _doexec($cmd)
    {
        $this->lastexec = $cmd;
        return `$cmd`;
    }

}



class Schedule_At_Error extends PEAR_Error
{
    var $classname             = 'Schedule_At_Error';
    var $error_message_prepend = 'Error in Schedule_At';
                                        
    function Schedule_At_Error ($message, $code = 0, $mode = PEAR_ERROR_RETURN, $level = E_USER_NOTICE)
    {
        $this->PEAR_Error ($message, $code, $mode, $level);
    }

}        
                

?>

