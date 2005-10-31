/**
 * Copyright (c) 2003-2005, David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2005 by Mark Lussier
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

import java.util.Date;

/**
 * BlojsomEvent
 *
 * @author David Czarnecki
 * @since blojsom 2.18
 * @version $Id: BlojsomEvent.java,v 1.1.2.1 2005/07/21 14:11:02 johnan Exp $
 */
public class BlojsomEvent {

    protected Object _source;
    protected Date _timestamp;
    protected boolean _eventHandled = false;

    /**
     * Create a new event.
     *
     * @param source Source of the event
     * @param timestamp Event timestamp
     */
    public BlojsomEvent(Object source, Date timestamp) {
        _source = source;
        _timestamp = timestamp;
    }

    /**
     * Retrieve the source of the event
     *
     * @return Event source
     */
    public Object getSource() {
        return _source;
    }

    /**
     * Retrieve the timestamp when the event occurred
     *
     * @return Event timestamp
     */
    public Date getTimestamp() {
        return _timestamp;
    }

    /**
     * Check to see if the event has already been handled or not
     *
     * @return <code>true</code> if the event has been handled, <code>false</code> otherwise
     * @since blojsom 2.22
     */
    public boolean isEventHandled() {
        return _eventHandled;
    }

    /**
     * Set whether or not the event has been handled
     *
     * @param eventHandled <code>true</code> if the event has been handled, <code>fasle</code> otherwise
     * @since blojsom 2.22
     */
    public void setEventHandled(boolean eventHandled) {
        _eventHandled = eventHandled;
    }
}