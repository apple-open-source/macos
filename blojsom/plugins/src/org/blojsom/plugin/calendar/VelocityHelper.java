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
package org.blojsom.plugin.calendar;

import org.blojsom.util.BlojsomUtils;

import java.util.Calendar;
import java.util.Locale;
import java.text.DateFormatSymbols;

/**
 * VelocityHelper is a class used to help render a visual calendar using the VTL.
 * 
 * @author Mark Lussier
 * @version $Id: VelocityHelper.java,v 1.5 2005/01/10 21:52:34 johnan Exp $
 */
public class VelocityHelper {

    private BlogCalendar _calendar;

    // [Row][Col]
    private String[][] visualcalendar = new String[6][7];

    private static final String VTL_SPACER = "&nbsp;";

    private static final String HREF_PREFIX = "<a href=\"";
    private static final String HREF_SUFFIX = "</a>";

    /**
     * Public Constructor
     */
    public VelocityHelper() {
    }

    /**
     * Public Constructor
     * 
     * @param calendar BlogCalendar to render
     */
    public VelocityHelper(BlogCalendar calendar) {
        _calendar = calendar;
    }

    /**
     * Sets the BlogCalendar to render
     * 
     * @param calendar BlogCalendar
     */
    public void setCalendar(BlogCalendar calendar) {
        _calendar = calendar;
    }

    /**
     * Builds the visual calendar model
     */
    public void buildCalendar() {
        int fdow = _calendar.getFirstDayOfMonth() - 1;
        int ldom = _calendar.getDaysInMonth();
        int dowoffset = 0;
        for (int x = 0; x < 6; x++) {
            for (int y = 0; y < 7; y++) {
                if ((x == 0 && y < fdow) || (dowoffset >= ldom)) {
                    visualcalendar[x][y] = VTL_SPACER;
                } else {
                    dowoffset += 1;
                    if (!_calendar.dayHasEntry(dowoffset)) {
                        visualcalendar[x][y] = new Integer(dowoffset).toString();
                    } else {
                        StringBuffer _url = new StringBuffer(HREF_PREFIX);
                        String _calurl = BlojsomUtils.getCalendarNavigationUrl(_calendar.getCalendarUrl(), (_calendar.getCurrentMonth() + 1), dowoffset, _calendar.getCurrentYear());
                        _url.append(_calurl);
                        _url.append("\">").append(dowoffset).append(HREF_SUFFIX);
                        visualcalendar[x][y] = _url.toString();
                    }

                }
            }
        }
    }

    /**
     * Get the visual content for a given calendar row
     * 
     * @param row   the row
     * @param clazz the css style apply
     * @return the visual calendar row
     */
    public String getCalendarRow(int row, String clazz) {
        StringBuffer result = new StringBuffer();
        if (row > 0 && row <= visualcalendar.length) {
            for (int x = 0; x < 7; x++) {
				String dayOfMonthLink = visualcalendar[row - 1][x];
				Integer currentDayInt = new Integer(_calendar.getCurrentDay());
				String dayOfMonthText = dayOfMonthLink.replaceAll("<[^>]+>", "");
				if (dayOfMonthLink.indexOf("<a") >= 0 && dayOfMonthText.equals(currentDayInt.toString())) {
					result.append("<td class=\"").append(clazz).append("-h\">").append(dayOfMonthText).append("</td>");
				}
				else if (dayOfMonthLink.indexOf("<a") == (-1)) {
					result.append("<td class=\"").append(clazz).append("\">").append(dayOfMonthLink).append("</td>");
				} else {
					result.append("<td class=\"").append(clazz).append("-a\">").append(dayOfMonthLink).append("</td>");
				}
            }
        }
        return result.toString();
    }

    /**
     * Get the visual control for navigating to Today
     * 
     * @return the today navigation control
     */
    public String getToday() {
        StringBuffer result = new StringBuffer();
        result.append(HREF_PREFIX).append(_calendar.getCalendarUrl()).append("\">Today").append(HREF_SUFFIX);
        return result.toString();
    }

    /**
     * Get the visual control for navigating to the previous month
     * 
	 * @param localeString Locale name string
     * @return the previous month navigation control
     */
    public String getPreviousMonth(String localeString) {
        StringBuffer result = new StringBuffer();
        _calendar.getCalendar().add(Calendar.MONTH, -1);
        result.append(HREF_PREFIX);
        String prevurl = BlojsomUtils.getCalendarNavigationUrl(_calendar.getCalendarUrl(),
                (_calendar.getCalendar().get(Calendar.MONTH) + 1),
                -1, _calendar.getCalendar().get(Calendar.YEAR));
        result.append(prevurl);
        result.append("\"> &lt;").append(VTL_SPACER).append(VTL_SPACER);

		if (localeString == null) {
			result.append(_calendar.getShortMonthName(_calendar.getCalendar().get(Calendar.MONTH)));
		}
		else {
			Locale locale = new Locale(localeString);
			DateFormatSymbols symbols = new DateFormatSymbols(locale);
			result.append(symbols.getShortMonths()[_calendar.getCalendar().get(Calendar.MONTH)]);
		}

        result.append(HREF_SUFFIX);
        _calendar.getCalendar().add(Calendar.MONTH, 1);
        return result.toString();
    }

    /**
     * Get the visual control for navigating to the previous month
     * 
     * @return the previous month navigation control
     */
    public String getPreviousMonth() {
		return getPreviousMonth(null);
    }

    /**
     * Get the visual control for navigating to the next month
     * 
	 * @param localeString Locale name string
     * @return the next month navigation control
     */
    public String getNextMonth(String localeString) {
        StringBuffer result = new StringBuffer();
        _calendar.getCalendar().add(Calendar.MONTH, 1);

        result.append(HREF_PREFIX);
        String nexturl = BlojsomUtils.getCalendarNavigationUrl(_calendar.getCalendarUrl(),
                (_calendar.getCalendar().get(Calendar.MONTH) + 1),
                -1, _calendar.getCalendar().get(Calendar.YEAR));

        result.append(nexturl);
        result.append("\"> ");
		
		if (localeString == null) {
			result.append(_calendar.getShortMonthName(_calendar.getCalendar().get(Calendar.MONTH)));
		}
		else {
			Locale locale = new Locale(localeString);
			DateFormatSymbols symbols = new DateFormatSymbols(locale);
			result.append(symbols.getShortMonths()[_calendar.getCalendar().get(Calendar.MONTH)]);
		}
		
        result.append(VTL_SPACER).append(VTL_SPACER).append("&gt;").append(HREF_SUFFIX);
        _calendar.getCalendar().add(Calendar.MONTH, -1);

        return result.toString();
    }
	
    /**
     * Get the visual control for navigating to the next month
     * 
     * @return the next month navigation control
     */
    public String getNextMonth() {
		return getNextMonth(null);
	}
}
