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

/**
 * BlojsomEventBroadcaster
 *
 * @author David Czarnecki
 * @since blojsom 2.18
 * @version $Id: BlojsomEventBroadcaster.java,v 1.1.2.1 2005/07/21 14:11:02 johnan Exp $
 */
public interface BlojsomEventBroadcaster {

    /**
     * Add a event to this event broadcaster
     *
     * @param listener {@link BlojsomListener}
     */
    public void addListener(BlojsomListener listener);

    /**
     * Add a event to this event broadcaster. Events are filtered using the {@link BlojsomFilter} instance
     * passed to this method.
     *
     * @param listener {@link BlojsomListener}
     * @param filter {@link BlojsomFilter} used to filter events
     */
    public void addListener(BlojsomListener listener, BlojsomFilter filter);

    /**
     * Remove a event from this event broadcaster
     *
     * @param listener {@link BlojsomListener}
     */
    public void removeListener(BlojsomListener listener);

    /**
     * Broadcast an event to all listeners
     *
     * @param event {@link org.blojsom.event.BlojsomEvent} to be broadcast to all listeners
     */
    public void broadcastEvent(BlojsomEvent event);

    /**
     * Process an event with all listeners
     *
     * @param event {@link BlojsomEvent} to be processed by all listeners
     * @since blojsom 2.24
     */
    public void processEvent(BlojsomEvent event);
}