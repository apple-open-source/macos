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

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.weather.beans.NWSInformation;
import org.blojsom.plugin.weather.beans.WeatherInformation;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;


/**
 * WeatherPlugin
 *
 * @author Mark Lussier
 * @version $Id: WeatherPlugin.java,v 1.1.2.1 2005/07/21 04:30:43 johnan Exp $
 * @since Blojsom 2.23
 */
public class WeatherPlugin implements BlojsomPlugin, BlojsomConstants {

    private Log _logger = LogFactory.getLog(WeatherPlugin.class);

    /**
     * Default poll time (60 minutes)
     */
    private static final int DEFAULT_POLL_TIME = 3200;

    /**
     * Weather configuration parameter for forcast polling time (60 minutes)
     */
    public static final String PLUGIN_WEATHER_POLL_TIME = "plugin-weather-poll-time";

    /**
     * Weather confifguration parameter for web.xml
     */
    public static final String PLUGIN_WEATHER_CONFIGURATION_IP = "plugin-weather";

    /**
     *
     */
    public static final String PROPERTY_WEATHER_CODE = "weather-station-code";

    /**
     *
     */
    public static final String PROPERTY_WEATHER_PROVIDER = "weather-provider";

    /**
     *
     */
    public static final String DEFAULT_WEATHER_PROVIDER = "org.blojsom.plugin.weather.beans.NWSInformation";

    /**
     * Default Weather Station Code - San Jose Internation Airport - San Jose, CA USA
     */
    public static final String DEFAULT_WEATHER_CODE = "KSJC";


    private WeatherChecker _weatherThread;
    private boolean _finished = false;
    private int _pollTime;
    private ServletConfig _servletConfig;
    private BlojsomConfiguration _blojsomConfiguration;
    private Map _userWeatherMap;


    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig        Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {

        _userWeatherMap = new HashMap(10);

        String weatherPollTime = servletConfig.getInitParameter(PLUGIN_WEATHER_POLL_TIME);
        if (BlojsomUtils.checkNullOrBlank(weatherPollTime)) {
            _pollTime = DEFAULT_POLL_TIME;
        } else {
            try {
                _pollTime = Integer.parseInt(weatherPollTime);
            } catch (NumberFormatException e) {
                _logger.error("Invalid time specified for: " + PLUGIN_WEATHER_POLL_TIME);
                _pollTime = DEFAULT_POLL_TIME;
            }
        }

        _servletConfig = servletConfig;
        _blojsomConfiguration = blojsomConfiguration;
        _weatherThread = new WeatherChecker();
        _weatherThread.setDaemon(true);

        _weatherThread.start();

        _logger.debug("Initialized weather plugin.");

    }

    /**
     * Process the blog entries
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @param user                {@link org.blojsom.blog.BlogUser} instance
     * @param context             Context
     * @param entries             Blog entries retrieved for the particular request
     * @return Modified set of blog entries
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, Map context, BlogEntry[] entries) throws BlojsomPluginException {
        WeatherInformation info = (WeatherInformation) _userWeatherMap.get(user.getId());

        if (info != null) {
            context.put(WeatherConstants.BLOJSOM_WEATHER_INFORMATION, info);
        }

        return entries;
    }

    /**
     * Perform any cleanup for the plugin. Called after {@link #process}.
     *
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error performing cleanup for this plugin
     */
    public void cleanup() throws BlojsomPluginException {
    }

    /**
     * Called when BlojsomServlet is taken out of service
     *
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error in finalizing this plugin
     */
    public void destroy() throws BlojsomPluginException {
        _finished = true;
    }


    private class WeatherChecker extends Thread {

        private WeatherFetcher _fetcher;

        /**
         * Allocates a new <code>Thread</code> object. This constructor has
         * the same effect as <code>Thread(null, null,</code>
         * <i>gname</i><code>)</code>, where <b><i>gname</i></b> is
         * a newly generated name. Automatically generated names are of the
         * form <code>"Thread-"+</code><i>n</i>, where <i>n</i> is an integer.
         *
         * @see Thread#Thread(ThreadGroup,
                *      Runnable, String)
         */
        public WeatherChecker() {
            super();
            _fetcher = new WeatherFetcher();
        }

        /**
         * Collect the weather information
         *
         * @param weather {@link Weather}
         * @return {@link WeatherInformation}
         */
        private WeatherInformation collectWeatherData(Weather weather) {
            WeatherInformation result = null;

            try {
                _logger.info("Fetching forecast for " + weather.getStationCode());
                WeatherInformation nws = new NWSInformation(weather.getStationCode());
                result = _fetcher.retrieveForecast(nws);
                _logger.info(result);
            } catch (IOException e) {
                _logger.error(e);
            }

            return result;
        }

        /**
         * If this thread was constructed using a separate
         * <code>Runnable</code> run object, then that
         * <code>Runnable</code> object's <code>run</code> method is called;
         * otherwise, this method does nothing and returns.
         * <p/>
         * Subclasses of <code>Thread</code> should override this method.
         *
         * @see Thread#start()
         * @see Thread#stop()
         * @see Thread#Thread(ThreadGroup,
                *      Runnable, String)
         * @see Runnable#run()
         */
        public void run() {
            try {
                while (!_finished) {
                    _logger.debug("Weather plugin is waking up and looking for updated forecasts");
                    String[] users = _blojsomConfiguration.getBlojsomUsers();
                    for (int i = 0; i < users.length; i++) {
                        String user = users[i];
                        BlogUser blogUser = null;
                        try {
                            blogUser = _blojsomConfiguration.loadBlog(user);

                            Weather weather = WeatherPluginUtils.readWeatherSettingsForUser(_blojsomConfiguration, _servletConfig, blogUser);
                            if (weather != null) {
                                if (weather.isEnabled()) {
                                    WeatherInformation info = collectWeatherData(weather);
                                    if (info != null) {
                                        _userWeatherMap.put(blogUser.getId(), info);
                                    }
                                }
                            }
                        } catch (BlojsomException e) {
                            _logger.error(e);
                        }
                    }

                    _logger.debug("Weather plugin off to take a nap");
                    sleep(_pollTime * 1000);
                }
            } catch (InterruptedException e) {
                _logger.error(e);
            }
        }
    }
}
