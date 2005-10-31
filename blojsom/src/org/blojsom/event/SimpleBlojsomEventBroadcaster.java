/**
 * Copyright (c) 2003-2005 , David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2005  by Mark Lussier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of the "David A. Czarnecki" and "blojsom" nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * Products derived from this software may not be called "blojsom",
 * nor may "blojsom" appear in their name, without prior written permission of
 * David A. Czarnecki.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
package org.blojsom.event;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import java.util.*;

/**
 * SimpleBlojsomEventBroadcaster.
 * <p></p>
 * Events are broadcast to each event in a separate thread so that the broadcaster is not a bottleneck.
 * No defined order is set for how each event will receive an event, so you should not assume any order
 * in listeners being called. No steps are taken to ensure a event does not receive an event if it is
 * removed at the same time an event is being broadcast.
 * <p></p>
 * The addition of the {@link #processEvent(BlojsomEvent)} method adds the capability for components to have an
 * event processed after the call instead of asynchronously as with the {@link #broadcastEvent(BlojsomEvent)} method. 
 *
 * @author David Czarnecki
 * @version $Id: SimpleBlojsomEventBroadcaster.java,v 1.1.2.1 2005/07/21 14:11:03 johnan Exp $
 * @since blojsom 2.18
 */
public class SimpleBlojsomEventBroadcaster implements BlojsomEventBroadcaster {

    private static final Log _logger = LogFactory.getLog(SimpleBlojsomEventBroadcaster.class);
    private Set _listeners;
    private Map _listenerToHandler;

    /**
     * Default constructor.
     */
    public SimpleBlojsomEventBroadcaster() {
        _listeners = new HashSet();
        _listenerToHandler = new HashMap();
    }

    /**
     * Add a event to this event broadcaster
     *
     * @param listener {@link BlojsomListener}
     */
    public void addListener(BlojsomListener listener) {
        EventHandler handler = new EventHandler(listener, new BlojsomFilter() {
            /**
             * Determines whether or not a particular event should be processed
             *
             * @param event {@link BlojsomEvent} to be processed
             * @return <code>true</code> if the event should be processed, <code>false</code> otherwise
             */
            public boolean processEvent(BlojsomEvent event) {
                return true;
            }
        });

        if (!_listenerToHandler.containsKey(listener.getClass().getName())) {
            _listeners.add(handler);
            _listenerToHandler.put(listener.getClass().getName(), handler);
            _logger.debug("Added event: " + listener.getClass().getName() + " with process all events filter");
        }
    }

    /**
     * Add a event to this event broadcaster. Events are filtered using the {@link org.blojsom.event.BlojsomFilter} instance
     * passed to this method.
     *
     * @param listener {@link BlojsomListener}
     * @param filter   {@link BlojsomFilter} used to filter events
     */
    public void addListener(BlojsomListener listener, BlojsomFilter filter) {
        EventHandler handler = new EventHandler(listener, filter);

        if (!_listenerToHandler.containsKey(listener.getClass().getName())) {
            _listeners.add(handler);
            _listenerToHandler.put(listener.getClass().getName(), handler);
            _logger.debug("Added event: " + listener.getClass().getName() + " with filter: " + filter.getClass().getName());
        }
    }

    /**
     * Remove a event from this event broadcaster
     *
     * @param listener {@link BlojsomListener}
     */
    public void removeListener(BlojsomListener listener) {
        if (_listenerToHandler.containsKey(listener.getClass().getName())) {
            EventHandler handler = (EventHandler) _listenerToHandler.get(listener.getClass().getName());
            _listeners.remove(handler);
            _listenerToHandler.remove(listener.getClass().getName());
        }

        _logger.debug("Removed event: " + listener.getClass().getName());
    }

    /**
     * Broadcast an event to all listeners
     *
     * @param event {@link BlojsomEvent} to be broadcast to all listeners
     */
    public void broadcastEvent(BlojsomEvent event) {
        Thread eventBroadcaster = new Thread(new AsynchronousEventBroadcaster(event));
        eventBroadcaster.setDaemon(true);
        eventBroadcaster.start();
    }

    /**
     * Process an event with all listeners
     *
     * @param event {@link BlojsomEvent} to be processed by all listeners
     * @since blojsom 2.24
     */
    public void processEvent(BlojsomEvent event) {
        Iterator handlerIterator = _listeners.iterator();
        while (handlerIterator.hasNext()) {
            EventHandler eventHandler = (EventHandler) handlerIterator.next();
            if (eventHandler._filter.processEvent(event)) {
                eventHandler._listener.processEvent(event);
            }
        }
    }

    /**
     * Event handler helper class.
     */
    protected class EventHandler {

        protected BlojsomListener _listener;
        protected BlojsomFilter _filter;

        /**
         * Create a new event handler with event and filter instances.
         *
         * @param listener {@link BlojsomListener}
         * @param filter   {@link BlojsomFilter}
         */
        protected EventHandler(BlojsomListener listener, BlojsomFilter filter) {
            _listener = listener;
            _filter = filter;
        }
    }

    /**
     * Thread to handle broadcasting an event to registered listeners.
     */
    private class AsynchronousEventBroadcaster implements Runnable {

        private BlojsomEvent _event;

        public AsynchronousEventBroadcaster(BlojsomEvent event) {
            _event = event;
        }

        /**
         * Iterates over the set of {@link EventHandler} registered with this broadcaster and calls
         * the {@link BlojsomListener#handleEvent(BlojsomEvent)} method with the
         * {@link BlojsomEvent}.
         */
        public void run() {
            Iterator handlerIterator = _listeners.iterator();
            while (handlerIterator.hasNext()) {
                EventHandler eventHandler = (EventHandler) handlerIterator.next();
                if (eventHandler._filter.processEvent(_event)) {
                    eventHandler._listener.handleEvent(_event);
                }
            }
        }
    }
}