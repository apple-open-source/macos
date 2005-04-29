/**
 * Copyright (c) 2003-2004, David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2004 by Mark Lussier
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
package org.blojsom.plugin.referer;

import java.util.Date;

/**
 * BlogReferer
 *
 * @author Mark Lussier
 * @version $Id: BlogReferer.java,v 1.2 2004/08/27 01:06:40 whitmore Exp $
 */
public class BlogReferer {

    private String _url;
    private String _flavor;
    private Date _lastreferal;
    private int _count;
    private boolean _istoday = false;

    /**
     * Blog referer constructor
     *
     * @param flavor Flavor
     * @param url URL
     * @param date Date
     * @param count Hit count
     */
    public BlogReferer(String flavor, String url, Date date, int count) {
        _url = url;
        _lastreferal = date;
        _count = count;
        _flavor = flavor;
        _istoday = determineToday();
    }

    /**
     * Get the flavor for the blog referer
     *
     * @return Flavor for blog referer
     */
    public String getFlavor() {
        return _flavor;
    }

    /**
     * Set the flavor for this blog referer
     *
     * @param flavor Flavor
     */
    public void setFlavor(String flavor) {
        _flavor = flavor;
    }

    /**
     * Get the URL for this blog referer
     *
     * @return URL for blog referer
     */
    public String getUrl() {
        return _url;
    }

    /**
     * Set the URL for this blog referer
     *
     * @param url URL
     */
    public void setUrl(String url) {
        _url = url;
    }

    /**
     * Get the last date of referral for this URL
     *
     * @return Date of last referral
     */
    public Date getLastReferral() {
        return _lastreferal;
    }

    /**
     * Check whether this blog referer has been seen today
     *
     * @return <code>true</code> if it has been seen today, <code>false</code> otherwise
     */
    public boolean isToday() {
        return _istoday;

    }

    /**
     * Set the date of last referral for this blog referer
     *
     * @param lastreferal Last referral date
     */
    public void setLastReferral(Date lastreferal) {
        if (lastreferal.compareTo(_lastreferal) < 0) {
            _lastreferal = lastreferal;
            _istoday = determineToday();
        }
    }

    /**
     * Get the referer count
     *
     * @return Number of referrals
     */
    public int getCount() {
        return _count;
    }

    /**
     * Increment the referer count by 1
     */
    public void increment() {
        _count += 1;
    }

    /**
     * Set the referer count
     *
     * @param count Referer count
     */
    public void setCount(int count) {
        _count = count;
    }

    /**
     * Determines if this referer has been seen TODAY
     *
     * @return a boolean indicating if it was set today
     */
    private boolean determineToday() {
        return (RefererLogPlugin.getRefererDate(new Date()).equals(RefererLogPlugin.getRefererDate(_lastreferal)));
    }
}
