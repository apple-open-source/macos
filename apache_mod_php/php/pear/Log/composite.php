<?php
// $Horde: horde/lib/Log/composite.php,v 1.2 2000/06/28 21:36:13 jon Exp $

/**
 * The Log_composite:: class implements a Composite pattern which
 * allows multiple Log implementations to get sent the same events.
 *
 * @author  Chuck Hagenbuch <chuck@horde.org>
 * @version $Revision: 1.1.1.1 $
 * @since Horde 1.3
*/
class Log_composite {
    
    // {{{ properties
    
    /** Array holding all Log instances which should be sent events
        sent to the composite. */
    var $children = array();
	
    // }}}
    
    
    // {{{ constructor
    /**
     * Constructs a new composite Log object.
     * 
     * @param $log_name (optional) This is ignored.
     * @param $ident    (optional) This is ignored.
     * @param $conf     (optional) This is ignored.
     */
    function Log_composite ($log_name = false, $ident = false, $conf = false) {
    }
    // }}}
    
	
    // {{{ open()
    /**
     * Open the log connections of each and every child of this
     * composite.
	 */
    function open () {
        if (!$this->opened) {
			reset($this->children);
			foreach ($this->children as $child) {
				$child->open();
			}
		}
    }
    // }}}
    
    // {{{ close()
    /**
     * If we've gone ahead and opened each child, go through and close
     * each child.
	 */
    function close () {
        if ($this->opened) {
			reset($this->children);
			foreach ($this->children as $child) {
				$child->close();
			}
		}
	}
	// }}}
    
    // {{{ log()
    /**
     * Sends $message and $priority to every child of this composite.
     * 
     * @param $message  The textual message to be logged.
     * @param $priority (optional) The priority of the message. Valid
     *                  values are: LOG_EMERG, LOG_ALERT, LOG_CRIT,
     *                  LOG_ERR, LOG_WARNING, LOG_NOTICE, LOG_INFO,
     *                  and LOG_DEBUG. The default is LOG_INFO.
     */
    function log ($message, $priority = LOG_INFO) {
        reset($this->children);
        foreach ($this->children as $child) {
            $child->log($message, $priority);
        }
		
        $this->notifyAll(array('priority' => $priority, 'message' => $message));
    }
    // }}}
	
    // {{{ isComposite()
    /**
     * @return a Boolean: true if this is a composite class, false
     * otherwise. Always returns true since this is the composite
     * subclass.
	 */
    function isComposite () {
		return true;
    }
    // }}}
	
	// {{{ addChild()
	/**
	 * Add a Log instance to the list of children that messages sent
	 * to us should be passed on to.
	 *
	 * @param $child The Log instance to add.
	 */
	function addChild (&$child) {
        if (!is_object($child))
            return false;
        
        $child->_childID = uniqid(rand());
        
        $this->children[$child->_listenerID] = &$child;
	}
	// }}}
	
	// {{{ removeChild()
	/**
	 * Remove a Log instance from the list of children that messages
	 * sent to us should be passed on to.
	 *
	 * @param $child The Log instance to remove.
	 */
	function removeChild ($child) {
		if (isset($this->children[$child->_childID]))
			unset($this->children[$child->_childID]);
	}
	// }}}
	
}

?>
