<?php
// $Horde: horde/lib/Log/syslog.php,v 1.6 2000/06/28 21:36:13 jon Exp $

/**
 * The Log_syslog class is a concrete implementation of the Log::
 * abstract class which sends messages to syslog on UNIX-like machines
 * (PHP emulates this with the Event Log on Windows machines).
 * 
 * @author  Chuck Hagenbuch <chuck@horde.org>
 * @version $Revision: 1.1.1.1 $
 * @since   Horde 1.3
 */
class Log_syslog extends Log {
    
    // {{{ properties
    
    /** Integer holding the log facility to use. */
    var $name = LOG_SYSLOG;
    
    // }}}
    
    
    // {{{ constructor
    /**
     * Constructs a new syslog object.
     * 
     * @param $log_name (optional) The syslog facility.
     * @param $ident    (optional) The identity string.
     * @param $conf     (optional) The configuration array.
     */
    function Log_syslog ($log_name = LOG_SYSLOG, $ident = '', $conf = false) {
        $this->name = $log_name;
        $this->ident = $ident;
    }
    // }}}
    
    
    // {{{ open()
    /**
     * Opens a connection to the system logger, if it has not already
     * been opened.  This is implicitly called by log(), if necessary.
     */
    function open () {
        if (!$this->opened) {
            openlog($this->ident, LOG_PID, $this->name);
            $this->opened = true;
        }
    }
    // }}}
    
    // {{{ close()
    /**
     * Closes the connection to the system logger, if it is open.
     */
    function close () {
        if ($this->opened) {
            closelog();
            $this->opened = false;
        }
    }
    // }}}
    
    // {{{ log()
    /**
     * Sends $message to the currently open syslog * connection.  Calls
     * open() if necessary. Also passes the message along to any Log_observer
     * instances that are observing this Log.
     * 
     * @param $message  The textual message to be logged.
     * @param $priority (optional) The priority of the message.  Valid
     *                  values are: LOG_EMERG, LOG_ALERT, LOG_CRIT,
     *                  LOG_ERR, LOG_WARNING, LOG_NOTICE, LOG_INFO,
     *                  and LOG_DEBUG.  The default is LOG_INFO.
     */
    function log ($message, $priority = LOG_INFO) {
        if (!$this->opened)
            $this->open();
        
        syslog($priority, $message);
        $this->notifyAll(array('priority' => $priority, 'message' => $message));
    }
    // }}}
    
}

?>
