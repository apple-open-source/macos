<?php
// $Horde: horde/lib/Log/observer.php,v 1.5 2000/06/28 21:36:13 jon Exp $

/**
 * The Log_observer:: class implements the Observer end of a
 * Subject-Observer pattern for watching log activity and taking
 * actions on exceptional events.
 *
 * @author  Chuck Hagenbuch <chuck@horde.org>
 * @version $Revision: 1.1.1.1 $
 * @since   Horde 1.3
 */
class Log_observer {
    
    // {{{ properties
    
    /** The minimum priority level of message that we want to hear
        about. 0 (LOG_EMERG) is the highest priority, so we will only
        hear messages with an integer priority value less than or
        equal to ours. It defaults to LOG_INFO, which listens to
        everything except LOG_DEBUG. */
    var $priority = LOG_INFO;
	
    // }}}
	
	
	// {{{ constructor
	/**
	 * Basic Log_observer instance that just prints out messages
	 * received.
	 */
	function Log_observer ($priority = LOG_INFO) {
		$this->priority = $priority;
	}
	// }}}
	
	
    // {{{ factory()
    /**
     * Attempts to return a concrete Log_observer instance of the
     * specified type.
     * 
     * @param $observer_type    The type of concrate Log_observer subclass
     *                          to return.  We will attempt to dynamically
     *                          include the code for this subclass.
     * @return                  The newly created concrete Log_observer
     * instance, or an false on an error.  */
    function factory ($observer_type, $priority = LOG_INFO) {
        $classfile = substr(__FILE__, 0, -(strlen(basename(__FILE__)))) . 'Log/' . $observer_type . '.php';
        if (file_exists($classfile)) {
            include_once $classfile;
            $class = 'Log_' . $observer_type;
            return new $observer_type($priority);
        } else {
            return false;
        }
    }
    // }}}
    
    // {{{ notify()
    /**
     * This is a stub method to make sure that LogObserver classes do
     * something when they are notified of a message. The default
     * behavior is to just print the message, which is obviously not
     * desireable in practically any situation - which is why you need
     * to override this method. :)
     * 
     * @param $messageOb    A hash containing all information - the text
     *                      message itself, the priority, what log it came
     *                      from, etc.
     */
    function notify ($messageOb) {
        print_r($messageOb);
    }
    // }}}
    
}

?>
