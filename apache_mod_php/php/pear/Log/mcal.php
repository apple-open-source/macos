<?php
// $Horde: horde/lib/Log/mcal.php,v 1.2 2000/06/28 21:36:13 jon Exp $

/**
 * The Log_mcal class is a concrete implementation of the Log::
 * abstract class which sends messages to a local or remote calendar
 * store accessed through MCAL.
 * 
 * @author  Chuck Hagenbuch <chuck@horde.org>
 * @version $Revision: 1.1.1.1 $
 * @since Horde 1.3
 */
class Log_mcal extends Log {
    
    // {{{ properties
    
    /** String holding the calendar specification to connect to. */
    var $calendar = '{localhost/mstore}';
	
	/** String holding the username to use. */
	var $username = '';
	
	/** String holding the password to use. */
	var $password = '';
	
	/** Integer holding the options to pass to the calendar stream. */
	var $options = 0;
	
    /** ResourceID of the MCAL stream. */
    var $stream = '';
	
    /** Integer holding the log facility to use. */
    var $name = LOG_SYSLOG;
	
    // }}}
    
    
    // {{{ constructor
    /**
     * Constructs a new Log_mcal object.
     * 
     * @param $log_name (optional) The category to use for our events.
     * @param $ident    (optional) The identity string.
     * @param $conf     (optional) The configuration array.
     */
    function Log_mcal ($log_name = LOG_SYSLOG, $ident = '', $conf = false) {
        $this->name = $log_name;
        $this->ident = $ident;
		$this->calendar = $conf['calendar'];
		$this->username = $conf['username'];
		$this->password = $conf['password'];
		$this->options = $conf['options'];
    }
    // }}}
    
    
    // {{{ open()
    /**
     * Opens a calendar stream, if it has not already been
     * opened. This is implicitly called by log(), if necessary.
	 */
    function open () {
        if (!$this->opened) {
            $this->stream = mcal_open($this->calendar, $this->username, $this->password, $this->options);
            $this->opened = true;
        }
    }
    // }}}
    
    // {{{ close()
    /**
     * Closes the calendar stream, if it is open.
     */
    function close () {
        if ($this->opened) {
            mcal_close($this->stream);
            $this->opened = false;
        }
    }
    // }}}
    
    // {{{ log()
    /**
     * Logs $message and associated information to the currently open
     * calendar stream. Calls open() if necessary. Also passes the
     * message along to any Log_observer instances that are observing
     * this Log.
     * 
     * @param $message  The textual message to be logged.
     * @param $priority (optional) The priority of the message. Valid
     *                  values are: LOG_EMERG, LOG_ALERT, LOG_CRIT,
     *                  LOG_ERR, LOG_WARNING, LOG_NOTICE, LOG_INFO,
     *                  and LOG_DEBUG. The default is LOG_INFO.
	 */
    function log ($message, $priority = LOG_INFO) {
        if (!$this->opened)
            $this->open();
		
        $date_str = date('Y:n:j:G:i:s');
		$dates = explode(':', $date_str);
		
		mcal_event_init($this->stream);
		mcal_event_set_title($this->stream, $this->ident);
		mcal_event_set_category($this->stream, $this->name);
		mcal_event_set_description($this->stream, $message);
		mcal_event_add_attribute($this->stream, 'priority', $priority);
		mcal_event_set_start($this->stream, $dates[0], $dates[1], $dates[2], $dates[3], $dates[4], $dates[5]);
		mcal_event_set_end($this->stream, $dates[0], $dates[1], $dates[2], $dates[3], $dates[4], $dates[5]);
		mcal_append_event($this->stream);
		
        $this->notifyAll(array('priority' => $priority, 'message' => $message));
    }
    // }}}
    
}

?>
