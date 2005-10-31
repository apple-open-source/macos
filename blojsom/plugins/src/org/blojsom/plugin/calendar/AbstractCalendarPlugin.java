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
package org.blojsom.plugin.calendar;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.Blog;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.*;

/**
 * AbstractCalendarPlugin is a base plugin that is used by the various calendar plugins
 * to filter content.
 *
 * @author Mark Lussier
 * @version $Id: AbstractCalendarPlugin.java,v 1.2.2.1 2005/07/21 04:30:26 johnan Exp $
 */
public abstract class AbstractCalendarPlugin implements BlojsomPlugin {

    private Log _logger = LogFactory.getLog(AbstractCalendarPlugin.class);

    protected static final String BLOJSOM_CALENDAR_LOCALE = "BLOJSOM_CALENDAR_LOCALE";

    /**
     * Key under which the blog calendar will be placed
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_CALENDAR = "BLOJSOM_CALENDAR";

    /**
     * Key under which the blog calendar vtl helper will be placed
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_CALENDAR_VTLHELPER = "BLOJSOM_CALENDAR_VTLHELPER";

    /**
     * Request parameter for the "year"
     */
    protected static final String YEAR_PARAM = "year";

    /**
     * Request parameter for the "month"
     */
    protected static final String MONTH_PARAM = "month";

    /**
     * Request parameter for the "day"
     */
    protected static final String DAY_PARAM = "day";

    /**
     * Format String for Calendar Month
     * (Example: March 2003)
     */
    protected static final String BLOJSOM_CALENDAR_FORMAT = "MMMMM yyyy";

    /**
     * Short Format String for Previous/Next Calendar Month(s)
     * (Example: Mar)
     */
    protected static final String BLOJSOM_CALENDAR_SHORTFORMAT = "MMM";


    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig        Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link BlojsomConfiguration} information
     * @throws BlojsomPluginException If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
    }

    /**
     * Process the blog entries
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @param user                {@link BlogUser} instance
     * @param context             Context
     * @param entries             Blog entries retrieved for the particular request
     * @return Modified set of blog entries
     * @throws BlojsomPluginException If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest,
                               HttpServletResponse httpServletResponse,
                               BlogUser user,
                               Map context,
                               BlogEntry[] entries) throws BlojsomPluginException {

        Blog blog = user.getBlog();
        Locale locale = Locale.getDefault();

        // If blog-language is set in blojsom.properties, use it instead
        String localeLanguage = blog.getBlogLanguage();

        // If no locale is configured, use the system default
        if (localeLanguage != null) {
            locale = new Locale(localeLanguage);
        }
        context.put(BLOJSOM_CALENDAR_LOCALE, locale);

        String requestedDateKey;
        String calendarUrl = user.getBlog().getBlogURL();

        // Default to the Current Month and Year
        Calendar calendar = Calendar.getInstance(locale);
        calendar.set(Calendar.DAY_OF_MONTH, 1);

        int currentMonth = calendar.get(Calendar.MONTH);
        int currentYear = calendar.get(Calendar.YEAR);
        int currentDay = calendar.get(Calendar.DAY_OF_MONTH);

        // Determine a calendar-based request
        String year = null;
        String month = null;
        String day = null;


        year = httpServletRequest.getParameter(YEAR_PARAM);
        if (year != null) {

            // Must be a 4 digit year
            if (year.length() != 4) {
                year = null;
            } else {
                try {
                    currentYear = Integer.parseInt(year);
                    calendar.set(Calendar.YEAR, currentYear);
                } catch (NumberFormatException e) {
                    year = "";
                    _logger.error("Invalid Year Param submitted and ignored: " + year);
                }

                month = httpServletRequest.getParameter(MONTH_PARAM);

                if (month == null) {
                    month = "";
                } else if (month.length() < 2) {
                    month = "0" + month;
                }

                if (!month.equals("")) {
                    try {
                        currentMonth = Integer.parseInt(month) - 1; // Damm Sun!
                        calendar.set(Calendar.MONTH, currentMonth);
                    } catch (NumberFormatException e) {
                        month = "";
                        _logger.error("Invalid Month Param submitted and ignored: " + month);
                    }
                }

                day = httpServletRequest.getParameter(DAY_PARAM);
                if (day == null) {
                    day = "";
                } else if (day.length() < 2) {
                    day = "0" + day;
                }

                if (!day.equals("")) {
                    try {
                        currentDay = Integer.parseInt(day);
                        if (currentDay > calendar.getActualMaximum(Calendar.DAY_OF_MONTH)) {
                            _logger.info("Adjusting day of month to max maximum for selected month");
                            currentDay = calendar.getActualMaximum(Calendar.DAY_OF_MONTH);
                        }
                        calendar.set(Calendar.DAY_OF_MONTH, currentDay);                        
                    } catch (NumberFormatException e) {
                        _logger.error("Invalid Day Param submitted and ignored: " + day);
                    }
                }
            }

            requestedDateKey = year + month + day;

        } else {
            requestedDateKey = null;
        }

        BlogCalendar blogCalendar = new BlogCalendar(calendar, calendarUrl, locale);
        blogCalendar.setRequestedDateKey(requestedDateKey);

        context.put(BLOJSOM_CALENDAR, blogCalendar);

        return entries;
    }

    /**
     * Perform any cleanup for the plugin. Called after {@link #process}.
     *
     * @throws BlojsomPluginException If there is an error performing cleanup for this plugin
     */
    public void cleanup() throws BlojsomPluginException {
    }

    /**
     * Called when BlojsomServlet is taken out of service
     *
     * @throws BlojsomPluginException If there is an error in finalizing this plugin
     */
    public void destroy() throws BlojsomPluginException {
    }

}
