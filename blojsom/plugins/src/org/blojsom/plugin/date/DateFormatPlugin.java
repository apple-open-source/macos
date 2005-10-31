package org.blojsom.plugin.date;

import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.blog.Blog;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.util.BlojsomUtils;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.Map;
import java.util.TimeZone;
import java.util.Locale;
import java.text.DateFormat;
import java.text.SimpleDateFormat;

/**
 * DateFormatPlugin
 *
 * @author David Czarnecki
 * @since blojsom 1.9.1
 * @version $Id: DateFormatPlugin.java,v 1.2.2.1 2005/07/21 04:30:29 johnan Exp $
 */
public class DateFormatPlugin implements BlojsomPlugin {

    private Log _logger = LogFactory.getLog(DateFormatPlugin.class);

    private static final String BLOG_TIMEZONE_ID_IP = "blog-timezone-id";
    private static final String BLOG_DATEFORMAT_PATTERN_IP = "blog-dateformat-pattern";

    /**
     * Key under which the date format of the blog will be placed
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_DATE_FORMAT = "BLOJSOM_DATE_FORMAT";

    /**
     * Default constructor
     */
    public DateFormatPlugin() {
    }

    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link BlojsomConfiguration} information
     * @throws BlojsomPluginException If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
    }

    /**
     * Process the blog entries
     *
     * @param httpServletRequest Request
     * @param httpServletResponse Response
     * @param user {@link BlogUser} instance
     * @param context Context
     * @param entries Blog entries retrieved for the particular request
     * @return Modified set of blog entries
     * @throws BlojsomPluginException If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest,
                               HttpServletResponse httpServletResponse,
                               BlogUser user,
                               Map context,
                               BlogEntry[] entries) throws BlojsomPluginException {

        Blog blog = user.getBlog();

        TimeZone _blogTimeZone;
        String _blogDateFormatPattern;

        Locale _blogLocale;
        DateFormat _blogDateFormat;

        String blogTimeZoneId = blog.getBlogProperty(BLOG_TIMEZONE_ID_IP);
        if (BlojsomUtils.checkNullOrBlank(blogTimeZoneId)) {
            blogTimeZoneId = TimeZone.getDefault().getID();
        }
        _logger.debug("Timezone ID: " + blogTimeZoneId);
        // Defaults to GMT if the Id is invalid
        _blogTimeZone = TimeZone.getTimeZone(blogTimeZoneId);

        String blogDateFormatPattern = blog.getBlogProperty(BLOG_DATEFORMAT_PATTERN_IP);
        if (BlojsomUtils.checkNullOrBlank(blogDateFormatPattern)) {
            _blogDateFormatPattern = null;
            _logger.debug("No value supplied for blog-dateformat-pattern");
        } else {
            _blogDateFormatPattern = blogDateFormatPattern;
            _logger.debug("Date format pattern: " + blogDateFormatPattern);
        }

        // Get a DateFormat for the specified TimeZone
        _blogLocale = new Locale(blog.getBlogLanguage());
        _blogDateFormat = DateFormat.getDateTimeInstance(DateFormat.FULL, DateFormat.FULL, _blogLocale);
        _blogDateFormat.setTimeZone(_blogTimeZone);
        if (_blogDateFormatPattern != null) {
            try {
                SimpleDateFormat sdf = (SimpleDateFormat) _blogDateFormat;
                sdf.applyPattern(_blogDateFormatPattern);
                _blogDateFormat = sdf;
            } catch (IllegalArgumentException ie) {
                _logger.error("Date format pattern \"" + _blogDateFormatPattern + "\" is invalid - using DateFormat.FULL");
            } catch (ClassCastException ce) {
                _logger.warn("Cannot cast to SimpleDateFormat to apply date format pattern");
            }
        }

        context.put(BLOJSOM_DATE_FORMAT, _blogDateFormat);
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
