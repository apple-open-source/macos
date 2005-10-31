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

import org.blojsom.util.BlojsomUtils;

import java.util.Calendar;
import java.util.Locale;
import java.text.DateFormatSymbols;

/**
 * VelocityHelper is a class used to help render a visual calendar using the VTL.
 * 
 * @author Mark Lussier
 * @version $Id: VelocityHelper.java,v 1.5.2.1 2005/07/21 04:30:26 johnan Exp $
 */
public class VelocityHelper {

    private BlogCalendar _calendar;

    // [Row][Col]
    private String[][] visualcalendar = new String[6][7];

    private static final String VTL_SPACER = "&nbsp;";

    private String HREF_PREFIX = "<a href=\"";
    private String HREF_SUFFIX = "</a>";
    private String _today = "Today";

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
     * Retrieve the {@link BlogCalendar} object used to construct this object
     *
     * @return {@link BlogCalendar}
     * @since blojsom 2.21
     */
    public BlogCalendar getBlogCalendar() {
        return _calendar;
    }

    /**
     * Builds the visual calendar model
     */
    public void buildCalendar() {
        int fdow = _calendar.getFirstDayOfMonth() - _calendar.getCalendar().getFirstDayOfWeek();
        if (fdow == -1) {
            fdow = 6;
        }
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
                        _url.append("\"><span>").append(dowoffset).append("</span>").append(HREF_SUFFIX);
                        visualcalendar[x][y] = _url.toString();
                    }

                }
            }
        }
    }

    /**
     * Get the visual content for a given calendar row. If <code>clazz</code> is null, no <code>class</code> attribute
     * will be included in the &lt;td&gt; tag.
     *
     * @param row   the row
     * @return the visual calendar row
     */
    public String getCalendarRow(int row) {
        return getCalendarRow(row, null);
    }

    /**
     * Get the visual content for a given calendar row. If <code>clazz</code> is null, no <code>class</code> attribute
     * will be included in the &lt;td&gt; tag.  
     * 
     * @param row   the row
     * @param clazz the css style apply
     * @return the visual calendar row
     */
    public String getCalendarRow(int row, String clazz) {
        StringBuffer result = new StringBuffer();
        if (row > 0 && row <= visualcalendar.length) {
            for (int x = 0; x < 7; x++) {
                if (clazz != null) {
                    result.append("<td class=\"").append(clazz).append("\">").append(visualcalendar[row - 1][x]).append("</td>");
                } else {
                    result.append("<td>").append(visualcalendar[row - 1][x]).append("</td>");
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
        result.append(HREF_PREFIX).append(_calendar.getCalendarUrl()).append("\">").append(_calendar.getShortMonthName(_calendar.getCurrentMonth())).append(HREF_SUFFIX);
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

    /**
     * Get the link for navigating to the previous month
     *
     * @return the previous month link
     * @since blojsom 2.21
     */
    public String getPreviousMonthLink() {

        StringBuffer result = new StringBuffer();
        _calendar.getCalendar().add(Calendar.MONTH, -1);

        String prevurl =
                BlojsomUtils.getCalendarNavigationUrl(_calendar.getCalendarUrl(),
                        (_calendar.getCalendar().get(Calendar.MONTH) + 1),
                        -1, _calendar.getCalendar().get(Calendar.YEAR));

        result.append(prevurl);
        _calendar.getCalendar().add(Calendar.MONTH, 1);

        return result.toString();
    }

    /**
     * Get the visual control for navigating to the previous month
     *
     * @return the previous month navigation control
     * @since blojsom 2.21
     */
    public String getPreviousMonthName() {
        StringBuffer result = new StringBuffer();
        _calendar.getCalendar().add(Calendar.MONTH, -1);

        result.append(_calendar.getMonthName(_calendar.getCalendar().get(Calendar.MONTH)));
        _calendar.getCalendar().add(Calendar.MONTH, 1);

        return result.toString();
    }

    /**
     * Get the link for navigating to the current month
     *
     * @return the current month link
     * @since blojsom 2.21
     */
    public String getCurrentMonthLink() {

        StringBuffer result = new StringBuffer();
        result.append(_calendar.getCalendarUrl());

        return result.toString();
    }

    /**
     * Get the link for navigating to the next month
     *
     * @return the next month link
     * @since blojsom 2.21
     */
    public String getCurrentMonthName() {
        StringBuffer result = new StringBuffer();

        result.append(_calendar.getMonthName(_calendar.getCalendar().get(Calendar.MONTH)));

        return result.toString();
    }


    /**
     * Get the link for navigating to the next month
     *
     * @return the next month link
     * @since blojsom 2.21
     */
    public String getNextMonthLink() {

        StringBuffer result = new StringBuffer();
        _calendar.getCalendar().add(Calendar.MONTH, 1);

        String nexturl =
                BlojsomUtils.getCalendarNavigationUrl(_calendar.getCalendarUrl(),
                        (_calendar.getCalendar().get(Calendar.MONTH) + 1),
                        -1, _calendar.getCalendar().get(Calendar.YEAR));

        result.append(nexturl);
        _calendar.getCalendar().add(Calendar.MONTH, -1);

        return result.toString();
    }

    /**
     * Get the name for navigating to the next month
     *
     * @return the next month name
     * @since blojsom 2.21
     */
    public String getNextMonthName() {
        StringBuffer result = new StringBuffer();
        _calendar.getCalendar().add(Calendar.MONTH, 1);

        result.append(_calendar.getMonthName(_calendar.getCalendar().get(Calendar.MONTH)));
        _calendar.getCalendar().add(Calendar.MONTH, -1);

        return result.toString();
    }

    /**
     * Set the text displayed for the "Today" link
     *
     * @param today Text for "Today" link
     * @since blojsom 2.22
     */
    public void setTodayText(String today) {
        _today = today;
    }

    /**
     * Retrieve the text displayed for the "Today" link
     *
     * @return Text for "Today link
     * @since blojsom 2.25
     */
    public String getTodayText() {
        return _today;
    }
}
