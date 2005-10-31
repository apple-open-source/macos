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
package org.blojsom.plugin.weather.beans;

import org.w3c.dom.Document;


/**
 * WeatherInformation
 *
 * @author Mark Lussier
 * @version $Id: WeatherInformation.java,v 1.1.2.1 2005/07/21 04:30:46 johnan Exp $
 * @since Blojsom 2.23
 */
public interface WeatherInformation {


    /**
     * Parse an XML document containing weather related information
     *
     * @param document XML document with weather information
     */
    void parseDocument(Document document);

    /**
     * Get the current temperature as Farenheit
     *
     * @return A String containing the current temperature in Farenheit
     */
    String getFahrenheit();

    /**
     * Get the current temperate as Celcius
     *
     * @return A String containing the current temperature as Celcius
     */
    String getCelcius();

    /**
     * Get the Location of the Weather Station
     *
     * @return The Weather Station name as a String
     */
    String getLocation();

    /**
     * Get the Station Id
     *
     * @return The Station Id as a String
     */
    String getStationCode();

    /**
     * Get the current Visibility
     *
     * @return The current visbility as a String
     */
    String getVisibility();

    /**
     * Get the current Wind conditions
     *
     * @return The current wind conditions as a String
     */
    String getWind();

    /**
     * Get the URL containing a link to weather history information
     *
     * @return URL for weather history information
     */
    String getHistoryUrl();

    /**
     * Gets the URL required to fetch this resource
     *
     * @return The resource location as a String
     */
    String getProviderUrl();

    /**
     * Get the value for a given tag from the parsed XML weather information
     *
     * @param tag Tag to retrieve
     * @return Value of tag or <code>null</code> if the tag is not present
     * @since blojsom 2.24
     */
    String getValueForTag(String tag);
}
