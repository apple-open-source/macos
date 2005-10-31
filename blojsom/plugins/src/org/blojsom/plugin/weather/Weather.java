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
package org.blojsom.plugin.weather;

import org.blojsom.blog.BlogUser;

/**
 * Weather
 *
 * @author Mark Lussier
 * @version $Id: Weather.java,v 1.1.2.1 2005/07/21 04:30:43 johnan Exp $
 * @since Blojsom 2.23
 */
public class Weather {

    private boolean _enabled = false;
    private BlogUser _blogUser;
    private String _stationCode;
    private String _providerClass;

    /**
     * Default constructor. Not enabled by default.
     */
    public Weather() {
        _enabled = false;
    }

    /**
     * Get the {@link BlogUser} associated with this weather object
     *
     * @return {@link BlogUser} data
     */
    public BlogUser getBlogUser() {
        return _blogUser;
    }

    /**
     * Set the {@link BlogUser} object for this weather object
     *
     * @param blogUser {@link BlogUser}
     */
    public void setBlogUser(BlogUser blogUser) {
        _blogUser = blogUser;
    }

    /**
     * Get the weather station ID for retrieving a feed of weather data
     *
     * @return Weather station ID
     */
    public String getStationCode() {
        return _stationCode;
    }

    /**
     * Set the weather station ID for retrieving a feed of weather data
     *
     * @param stationCode Weather station ID
     */
    public void setStationCode(String stationCode) {
        _stationCode = stationCode;
    }

    /**
     * Check to see if this weather object is enabled
     *
     * @return <code>true</code> if enabled, <code>false</code> otherwise
     */
    public boolean isEnabled() {
        return _enabled;
    }

    /**
     * Set whether or not this weather object is enabled
     *
     * @param enabled <code>true</code> to enable it, <code>false</code> otherwise
     */
    public void setEnabled(boolean enabled) {
        _enabled = enabled;
    }

    /**
     * Retrieve the fully-qualified classname handling the parsing of weather information
     *
     * @return Fully-qualified classname handling parsing of weather information
     */
    public String getProviderClass() {
        return _providerClass;
    }

    /**
     * Set the fully-qualified classname handling the parsing of weather information
     *
     * @param providerClass Fully-qualified classname handling parsing of weather information
     */
    public void setProviderClass(String providerClass) {
        _providerClass = providerClass;
    }
}
