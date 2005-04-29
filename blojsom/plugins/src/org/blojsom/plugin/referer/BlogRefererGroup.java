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
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

/**
 * BlogRefererGroup
 * <p />
 * This plugin manages either a flavor based hitcounter or a group of referers
 *
 * @author Mark Lussier
 * @version $Id: BlogRefererGroup.java,v 1.2 2004/08/27 01:06:40 whitmore Exp $
 */
public class BlogRefererGroup {

    private Map _groups;
    private int _grouptotal = 0;
    private boolean _hitcount = false;
    private Date _lasthit;

    /**
     * BlogRefererGroup constructor
     *
     * @param ishitcount Whether or not this is a hit counter group
     */
    public BlogRefererGroup(boolean ishitcount) {
        _groups = new HashMap(25);
        _hitcount = ishitcount;
    }

    /**
     * Add a referer to this referer group
     *
     * @param flavor Flavor
     * @param url URL
     * @param date Date of the referral
     */
    public void addReferer(String flavor, String url, Date date) {
        if (_groups.containsKey(url)) {
            BlogReferer br = (BlogReferer) _groups.get(url);
            br.increment();
            br.setLastReferral(date);
        } else {
            _groups.put(url, new BlogReferer(flavor, url, date, 1));
        }

        _grouptotal += 1;
    }

    /**
     * Add a hit count for a given date
     *
     * @param date Date of referral
     */
    public void addHitCount(Date date) {
        addHitCount(date, 1);
    }

    /**
     * Add a hit count for the given date
     *
     * @param date Date
     * @param count New hit count
     */
    public void addHitCount(Date date, int count) {
        if (_lasthit == null) {
            _lasthit = date;
        }
        if (date.compareTo(_lasthit) > 0) {
            _lasthit = date;
        }
        _grouptotal += count;
    }

    /**
     * Add a referral to this group
     *
     * @param flavor Flavor
     * @param url URL
     * @param date Date
     * @param total Total number of hits
     */
    public void addReferer(String flavor, String url, Date date, int total) {
        if (_groups.containsKey(url)) {
            BlogReferer br = (BlogReferer) _groups.get(url);
            br.setLastReferral(date);
            br.setCount(total);
        } else {
            _groups.put(url, new BlogReferer(flavor, url, date, total));
        }

        _grouptotal += total;
    }

    /**
     * Return the number of referral groups
     *
     * @return Number of blog referer groups
     */
    public int size() {
        return _groups.size();
    }

    /**
     * Get a referer from the group for a given key (URL)
     *
     * @param key Referer key
     * @return <code>BlogReferer</code> for the given key (URL)
     */
    public Object get(Object key) {
        return _groups.get(key);
    }

    /**
     * Return the keys for the referer groups
     *
     * @return Keys for the referer groups
     */
    public Set keySet() {
        return _groups.keySet();
    }

    /**
     * Get the total referer count
     *
     * @return Total referer count
     */
    public int getReferralCount() {
        return _grouptotal;
    }

    /**
     * Check whether or not this blog referer group collects hits
     *
     * @return
     */
    public boolean isHitCounter() {
        return _hitcount;
    }


    /**
     * Get the date of the last referel for a hitcounter
     *
     * @return Last hit date
     */
    public Date getLastReferralDate() {
        return _lasthit;
    }
}
