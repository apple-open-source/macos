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
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;

import javax.servlet.ServletConfig;
import java.io.IOException;
import java.io.InputStream;
import java.util.Properties;


/**
 * WeatherPluginUtils
 *
 * @author Mark Lussier
 * @version $Id: WeatherPluginUtils.java,v 1.1.2.1 2005/07/21 04:30:43 johnan Exp $
 * @since Blojsom 2.23
 */
public class WeatherPluginUtils {

    private static Log _logger = LogFactory.getLog(WeatherPluginUtils.class);

    /**
     * Read the {@link Weather} configuration information for a given blog
     *
     * @param blojsomConfiguration {@link BlojsomConfiguration}
     * @param servletConfig        {@link ServletConfig}
     * @param blogUser             {@link BlogUser}
     * @return {@link Weather} information or <code>null</code> if there is an error reading the configuration settings
     */
    public static Weather readWeatherSettingsForUser(BlojsomConfiguration blojsomConfiguration, ServletConfig servletConfig,
                                                     BlogUser blogUser) {
        String weatherConfiguration = servletConfig.getInitParameter(WeatherPlugin.PLUGIN_WEATHER_CONFIGURATION_IP);

        Weather weather = new Weather();
        weather.setEnabled(true);

        String user = blogUser.getId();

        Properties weatherProperties = new Properties();
        String configurationFile = blojsomConfiguration.getBaseConfigurationDirectory() + user + "/" + weatherConfiguration;

        InputStream is = servletConfig.getServletContext().getResourceAsStream(configurationFile);
        if (is == null) {
            _logger.info("No weather configuration file found: " + configurationFile);
        } else {
            try {
                weatherProperties.load(is);
                is.close();

                if (weatherProperties.size() > 0) {
                    weather.setBlogUser(blogUser);
                    String stationCode = weatherProperties.getProperty(WeatherPlugin.PROPERTY_WEATHER_CODE, WeatherPlugin.DEFAULT_WEATHER_CODE);
                    String providerClass = weatherProperties.getProperty(WeatherPlugin.PROPERTY_WEATHER_PROVIDER, WeatherPlugin.DEFAULT_WEATHER_PROVIDER);
                    weather.setStationCode(stationCode);
                    weather.setProviderClass(providerClass);
                }
            } catch (IOException e) {
                _logger.error(e);

                return null;
            }
        }

        return weather;
    }
}
