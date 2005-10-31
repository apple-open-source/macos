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
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import java.text.MessageFormat;

/**
 * NWSInformation
 *
 * @author Mark Lussier
 * @version $Id: NWSInformation.java,v 1.1.2.1 2005/07/21 04:30:46 johnan Exp $
 * @since Blojsom 2.23
 */
public class NWSInformation implements WeatherInformation {

    public static final String NWS_URL_FORMAT = "http://www.nws.noaa.gov/data/current_obs/{0}.xml";

    public static final String TAG_CREDIT = "credit";
    public static final String TAG_CREDIT_URL = "credit_URL";
    public static final String TAG_LOCATION = "location";
    public static final String TAG_OBSERVATION = "observation_time";
    public static final String TAG_WEATHER = "weather";
    public static final String TAG_TEMP_STRING = "temperature_string";
    public static final String TAG_TEMP_F = "temp_f";
    public static final String TAG_TEMP_C = "temp_c";
    public static final String TAG_HUMIDITY = "relative_humidity";
    public static final String TAG_WIND_STRING = "wind_string";
    public static final String TAG_WIND_DIRECTION = "wind_dir";
    public static final String TAG_WIND_DEGREES = "wind_degrees";
    public static final String TAG_WIND_MPH = "wind_mph";
    public static final String TAG_WIND_GUST_MPH = "wind_gust_mph";
    public static final String TAG_STATION = "station_id";
    public static final String TAG_VISIBILITY = "visibility";
    public static final String TAG_HISTORY = "two_day_history_url";

    private String _temperatureF = "-0 F";
    private String _temperatureC = "-0 C";
    private String _stationCode = "";
    private String _location = "a";
    private String _visibility = "";
    private String _wind = "";
    private String _history = "#";
    private Document _document;

    /**
     * Public Constructor
     *
     * @param stationCode The Provider Station Id
     */
    public NWSInformation(String stationCode) {
        _stationCode = stationCode;

    }

    /**
     * Parse an XML document containing weather related information
     *
     * @param document XML document with weather information
     */
    public void parseDocument(Document document) {
        _document = document;

        _temperatureC = getValueOfTag(TAG_TEMP_C);
        _temperatureF = getValueOfTag(TAG_TEMP_F);
        _stationCode = getValueOfTag(TAG_STATION);
        _location = getValueOfTag(TAG_LOCATION);
        _visibility = getValueOfTag(TAG_VISIBILITY);
        _wind = getValueOfTag(TAG_WIND_STRING);
        _history = getValueOfTag(TAG_HISTORY);
    }

    /**
     * Get Contents of a Tag in the Weather Data
     *
     * @param tag The Tag Name
     * @return Value of the Element as a String
     */
    private String getValueOfTag(String tag) {
        String result = null;
        NodeList nodeList = _document.getElementsByTagName(tag);
        if (nodeList != null) {
            Node tempNode = nodeList.item(0);
            Node value = tempNode.getFirstChild();
            if (value != null) {
                result = value.getNodeValue();
            }
        }

        return result;
    }

    /**
     * Get the Location of the Weather Station
     *
     * @return The Weather Station name as a String
     */
    public String getLocation() {
        return _location;
    }

    /**
     * Get the Station Id
     *
     * @return The Station Id as a String
     */
    public String getStationCode() {
        return _stationCode;
    }

    /**
     * Get the current temperature as Farenheit
     *
     * @return A String containing the current temperature in Farenheit
     */
    public String getFahrenheit() {
        return _temperatureF + " F";
    }

    /**
     * Get the current temperate as Celcius
     *
     * @return A String containing the current temperature as Celcius
     */
    public String getCelcius() {
        return _temperatureC + " C";
    }

    /**
     * Get the current Visibility
     *
     * @return The current visbility as a String
     */
    public String getVisibility() {
        return _visibility;
    }

    /**
     * Get the current Wind conditions
     *
     * @return The current wind conditions as a String
     */
    public String getWind() {
        return _wind;
    }

    /**
     * Get the URL containing a link to weather history information
     *
     * @return URL for weather history information
     */
    public String getHistoryUrl() {
        return _history;
    }

    /**
     * Gets the URL required to fetch this resource
     *
     * @return The resource location as a String
     */
    public String getProviderUrl() {
        return MessageFormat.format(NWS_URL_FORMAT, new Object[]{_stationCode});
    }

    /**
     * Get the value for a given tag from the parsed XML weather information
     *
     * @param tag Tag to retrieve
     * @return Value of tag or <code>null</code> if the tag is not present
     * @since blojsom 2.24
     */
    public String getValueForTag(String tag) {
        return getValueOfTag(tag);
    }
}
