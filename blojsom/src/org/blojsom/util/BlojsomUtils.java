/**
 * Copyright (c) 2003-2004 , David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2004  by Mark Lussier
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
package org.blojsom.util;

import org.blojsom.BlojsomException;
import org.blojsom.blog.FileBackedBlogEntry;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.*;
import java.net.URLDecoder;
import java.net.URLEncoder;
import java.nio.channels.FileChannel;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.text.SimpleDateFormat;
import java.util.*;

/**
 * BlojsomUtils
 *
 * @author David Czarnecki
 * @version $Id: BlojsomUtils.java,v 1.7 2005/02/24 20:55:31 johnan Exp $
 */
public class BlojsomUtils implements BlojsomConstants {

    /**
     * Private constructor so that the class cannot be instantiated.
     */
    private BlojsomUtils() {
    }

    /**
     * Filter only directories
     */
    private static final FileFilter DIRECTORY_FILTER = new FileFilter() {

        /**
         * Tests whether or not the specified abstract pathname should be
         * included in a pathname list.
         *
         * @param pathname The abstract pathname to be tested
         * @return <code>true</code> if and only if <code>pathname</code>
         *         should be included
         */
        public boolean accept(File pathname) {
            return (pathname.isDirectory());
        }
    };

    /**
     * Filter only files
     */
    private static final FileFilter FILE_FILTER = new FileFilter() {

        /**
         * Tests whether or not the specified abstract pathname should be
         * included in a pathname list.
         *
         * @param pathname The abstract pathname to be tested
         * @return <code>true</code> if and only if <code>pathname</code>
         *         should be included
         */
        public boolean accept(File pathname) {
            return (!pathname.isDirectory());
        }
    };

    /**
     * RFC-822 format
     * SimpleDateFormats are not threadsafe, but we should not need more than one per
     * thread.
     */
    private static final ThreadLocal RFC_822_DATE_FORMAT_OBJECT = new ThreadLocal() {
        protected Object initialValue() {
            return new SimpleDateFormat(RFC_822_DATE_FORMAT);
        }
    };

    /**
     * ISO-8601 format
     * SimpleDateFormats are not threadsafe, but we should not need more than one per
     * thread.
     */
    private static final ThreadLocal ISO_8601_DATE_FORMAT_OBJECT = new ThreadLocal() {
        protected Object initialValue() {
            SimpleDateFormat sdf = new SimpleDateFormat(ISO_8601_DATE_FORMAT);
            sdf.getTimeZone().setID("+00:00");
            return sdf;
        }
    };

    /**
     * UTC format
     * SimpleDateFormats are not threadsafe, but we should not need more than one per
     * thread.
     */
    private static final ThreadLocal UTC_DATE_FORMAT_OBJECT = new ThreadLocal() {
        protected Object initialValue() {
            return new SimpleDateFormat(UTC_DATE_FORMAT);
        }
    };

    /**
     * Return a file filter which only returns directories
     *
     * @return File filter appropriate for filtering only directories
     */
    public static FileFilter getDirectoryFilter() {
        return DIRECTORY_FILTER;
    }

    /**
     * Return a file filter which only returns directories that are not one of a list
     * of excluded directories
     *
     * @param excludedDirectories List of directories to exclude
     * @return File filter appropriate for filtering only directories
     */
    public static FileFilter getDirectoryFilter(final String[] excludedDirectories) {
        if (excludedDirectories == null) {
            return DIRECTORY_FILTER;
        }

        return new FileFilter() {
            public boolean accept(File pathname) {
                if (!pathname.isDirectory()) {
                    return false;
                } else {
                    for (int i = 0; i < excludedDirectories.length; i++) {
                        String excludedDirectory = excludedDirectories[i];
                        if (pathname.toString().matches(excludedDirectory)) {
                            return false;
                        }
                    }
                }
                return true;
            }
        };
    }

    /**
     * Return a date in RFC 822 style
     *
     * @param date Date
     * @return Date formatted as RFC 822
     */
    public static String getRFC822Date(Date date) {
        return ((SimpleDateFormat) RFC_822_DATE_FORMAT_OBJECT.get()).format(date);
    }

    /**
     * Return a date formatted date
     *
     * @param date   Date
     * @param format Date Format String
	 * @param localeName Locale name string
     * @return Date formatted date
     */
    public static String getFormattedDate(Date date, String format, String localeName) {
		if (localeName == null) {
			return getFormattedDate(date, format);
		}
		Locale locale = new Locale(localeName);
        SimpleDateFormat sdf = new SimpleDateFormat(format, locale);
        return sdf.format(date);
    }

    /**
     * Return a date formatted date
     * 
     * @param date   Date
     * @param format Date Format String
     * @return Date formatted date
     */
    public static String getFormattedDate(Date date, String format) {
        SimpleDateFormat sdf = new SimpleDateFormat(format);
        return sdf.format(date);
    }

    /**
     * Return a date in ISO 8601 style
     * http://www.w3.org/TR/NOTE-datetime
     *
     * @param date Date
     * @return Date formatted as ISO 8601
     */
    public static String getISO8601Date(Date date) {
        return ((SimpleDateFormat) ISO_8601_DATE_FORMAT_OBJECT.get()).format(date).replaceAll("GMT", "");
    }

    /**
     * Return a date in UTC style
     *
     * @param date Date
     * @return Date formatted as ISO 8601
     * @since blojsom 1.9.4
     */
    public static String getUTCDate(Date date) {
        return ((SimpleDateFormat) UTC_DATE_FORMAT_OBJECT.get()).format(date);
    }

    /**
     * Return a file filter which takes a list of regular expressions to look for
     *
     * @param expressions List of regular expressions for files to retrieve
     * @return File filter appropriate for filtering out a set of files based on regular expressions
     */
    public static FileFilter getRegularExpressionFilter(final String[] expressions) {
        return new FileFilter() {

            private Date today = new Date();

            public boolean accept(File pathname) {
                for (int i = 0; i < expressions.length; i++) {
                    if (pathname.isDirectory()) {
                        return false;
                    }
                    String expression = expressions[i];
                    if (pathname.getName().matches(expression)) {
                        if (pathname.lastModified() <= today.getTime()) {
                            return true;
                        } else {
                            return false;
                        }
                    }
                }
                return false;
            }
        };
    }

    /**
     * Return a file filter which takes a list of file extensions to look for
     *
     * @param extensions List of file extensions
     * @return File filter appropriate for filtering out a set of file extensions
     */
    public static FileFilter getExtensionsFilter(final String[] extensions) {
        return new FileFilter() {
            public boolean accept(File pathname) {
                for (int i = 0; i < extensions.length; i++) {
                    if (pathname.isDirectory()) {
                        return false;
                    }
                    String extension = extensions[i];
                    if (pathname.getName().endsWith(extension)) {
                        return true;
                    }
                }
                return false;
            }
        };
    }

    /**
     * Return a file filter which takes a single file extension to look for
     *
     * @param extension File extension
     * @return File filter appropriate for filtering out a single file extension
     */
    public static FileFilter getExtensionFilter(final String extension) {
        return getExtensionsFilter(new String[]{extension});
    }

    /**
     * Parse a comma-separated list of values; also parses over internal spaces
     *
     * @param commaList Comma-separated list
     * @return Individual strings from the comma-separated list
     */
    public static String[] parseCommaList(String commaList) {
        return parseDelimitedList(commaList, ", ");
    }

    /**
     * Parse a delimited list of values
     *
     * @param delimitedList Delimited list
     * @param delimiter     Field Delimiter
     * @return Individual strings from the comma-separated list
     */
    public static String[] parseDelimitedList(String delimitedList, String delimiter) {
        if (delimitedList == null) {
            return null;
        }

        StringTokenizer tokenizer = new StringTokenizer(delimitedList, delimiter);
        ArrayList list = new ArrayList();
        while (tokenizer.hasMoreTokens()) {
            list.add(tokenizer.nextToken());
        }
        if (list.size() == 0) {
            return new String[]{};
        }
        return (String[]) list.toArray(new String[list.size()]);
    }


    /**
     * Convert the request parameters to a string
     *
     * @param request Servlet request
     * @return Request parameters in the form &amp;name=value
     */
    public static String convertRequestParams(HttpServletRequest request) {
        Enumeration paramNames = request.getParameterNames();
        StringBuffer buffer = new StringBuffer();
        while (paramNames.hasMoreElements()) {
            String name = (String) paramNames.nextElement();
            String value = request.getParameter(name);
            try {
                buffer.append(URLEncoder.encode(name, "UTF-8")).append("=").append(URLEncoder.encode(value, "UTF-8"));
            } catch (UnsupportedEncodingException e) {
            }
            if (paramNames.hasMoreElements()) {
                buffer.append("&");
            }
        }
        return buffer.toString();
    }


    /**
     * Strip off the blog home directory for a requested blog category
     *
     * @param blogHome          Blog home value
     * @param requestedCategory Requested blog category
     * @return Blog category only
     */
    public static String getBlogCategory(String blogHome,
                                         String requestedCategory) {
        requestedCategory = requestedCategory.replace('\\', '/');
        int indexOfBlogHome = requestedCategory.indexOf(blogHome);
        if (indexOfBlogHome == -1) {
            return "";
        }
        indexOfBlogHome += blogHome.length();
        String returnCategory = requestedCategory.substring(indexOfBlogHome);
        returnCategory = removeInitialSlash(returnCategory);
        return '/' + returnCategory;
    }

    /**
     * Return a URL to the main blog site without the servlet path requested
     *
     * @param blogURL     URL for the blog
     * @param servletPath Servlet path under which the blog is placed
     * @return URL to the blog up to the servlet path
     */
    public static String getBlogSiteURL(String blogURL, String servletPath) {
        if (servletPath == null || "".equals(servletPath)) {
            return blogURL;
        }
        int servletPathIndex = blogURL.indexOf(servletPath, 7);
        if (servletPathIndex == -1) {
            return blogURL;
        }

        return blogURL.substring(0, servletPathIndex);
    }

    /**
     * Return an escaped string where &amp;, &lt;, &gt;, &quot;, and &apos; are converted to their HTML equivalents
     *
     * @param input Unescaped string
     * @return Escaped string containing HTML equivalents for &amp;, &lt;, &gt;, &quot;, and &apos;
     */
    public static String escapeString(String input) {
        if (input == null) {
            return null;
        }

        String unescaped = replace(input, "&", "&amp;");
        unescaped = replace(unescaped, "<", "&lt;");
        unescaped = replace(unescaped, ">", "&gt;");
        unescaped = replace(unescaped, "\"", "&quot;");
        unescaped = replace(unescaped, "'", "&#39;");
        return unescaped;
    }

    /**
     * Return an escaped string where &lt;meta, &lt;link tags are escaped
     *
     * @param input Unescaped string
     * @return Escaped string where &lt;meta, &lt;link tags are escaped
     */
    public static String escapeMetaAndLink(String input) {
        if (input == null) {
            return null;
        }

        String cleanedInput = input.replaceAll("<[mM][eE][tT][aA]", "&lt;meta");
        cleanedInput = cleanedInput.replaceAll("<[lL][iI][nN][kK]", "&lt;link");
        return cleanedInput;
    }

    /**
     * Replace any occurances of a string pattern within a string with a different string.
     *
     * @param str     The source string.  This is the string that will be searched and have the replacements
     * @param pattern The pattern to look for in str
     * @param replace The string to insert in the place of <i>pattern</i>
     * @return String with replace occurences
     */
    public static String replace(String str, String pattern, String replace) {
        if (str == null || "".equals(str)) {
            return str;
        }

        if (replace == null) {
            return str;
        }

        if ("".equals(pattern)) {
            return str;
        }

        int s = 0;
        int e = 0;
        StringBuffer result = new StringBuffer();

        while ((e = str.indexOf(pattern, s)) >= 0) {
            result.append(str.substring(s, e));
            result.append(replace);
            s = e + pattern.length();
        }
        result.append(str.substring(s));
        return result.toString();
    }

    /**
     * Return the file extension for a given filename or <code>null</code> if no file extension
     * is present
     *
     * @param filename Filename
     * @return File extension without the . or <code>null</code> if no file extension is present
     */
    public static String getFileExtension(String filename) {
        if (filename == null) {
            return null;
        }

        int dotIndex = filename.lastIndexOf(".");
        if (dotIndex == -1) {
            return null;
        } else {
            return filename.substring(dotIndex + 1);
        }
    }

    /**
     * Return the filename without extension for a given filename
     *
     * @param filename Filename
     * @return Filename up to the .
     * @since blojsom 1.9
     */
    public static String getFilename(String filename) {
        int dotIndex = filename.lastIndexOf(".");
        if (dotIndex == -1) {
            return filename;
        } else {
            return filename.substring(0, dotIndex);
        }
    }

    /**
     * Returns the base file name from the supplied file path. On the surface,
     * this would appear to be a trivial task. Apparently, however, some Linux
     * JDKs do not implement <code>File.getName()</code> correctly for Windows
     * paths, so we attempt to take care of that here.
     *
     * @param filenameWithPath The full path to the file.
     * @return The base file name, from the end of the path.
     * @since blojsom 2.12
     */
    public static String getFilenameFromPath(String filenameWithPath) {
        // First, ask the JDK for the base file name.
        String fileName = new File(filenameWithPath).getName();

        // Now check for a Windows file name parsed incorrectly.
        int colonIndex = fileName.indexOf(":");
        if (colonIndex == -1) {
            // Check for a Windows SMB file path.
            colonIndex = fileName.indexOf("\\\\");
        }
        int backslashIndex = fileName.lastIndexOf("\\");

        if (colonIndex > -1 && backslashIndex > -1) {
            // Consider this filename to be a full Windows path, and parse it
            // accordingly to retrieve just the base file name.
            fileName = fileName.substring(backslashIndex + 1);
        }

        return fileName;
    }

    /**
     * Return a string of "YYYYMMDD"
     *
     * @param date Date from which to extract "key"
     * @return String of "YYYYMMDD"
     */
    public static String getDateKey(Date date) {
        StringBuffer value = new StringBuffer();
        Calendar calendar = Calendar.getInstance();
        long l = 0;

        calendar.setTime(date);
        value.append(calendar.get(Calendar.YEAR));
        // month and date need to be 2 digits; otherwise it is
        // impossible to distinguish between e.g. November (11)
        // and January (1) when using the date as a prefix
        l = calendar.get(Calendar.MONTH) + 1;
        if (l < 10) {
            value.append("0");
        }
        value.append(l);
        l = calendar.get(Calendar.DAY_OF_MONTH);
        if (l < 10) {
            value.append("0");
        }
        value.append(l);
        // highest possible values above are 12 and 31, so no need to
        // be generic & handle arbitrary-length digits

        return value.toString();
    }

    /**
     * Remove the initial "/" from a string
     *
     * @param input Input string
     * @return Input string without initial "/" removed or <code>null</code> if the input was null
     */
    public static String removeInitialSlash(String input) {
        if (input == null) {
            return null;
        }

        if (!input.startsWith("/")) {
            return input;
        } else {
            return input.substring(1);
        }
    }

    /**
     * Remove the trailing "/" from a string
     *
     * @param input Input string
     * @return Input string with trailing "/" removed or <code>null</code> if the input was null
     */
    public static String removeTrailingSlash(String input) {
        if (input == null) {
            return null;
        }

        if (!input.endsWith("/")) {
            return input;
        } else {
            return input.substring(0, input.length() - 1);
        }
    }


    /**
     * Extracts the first line in a given string, otherwise returns the first n bytes
     *
     * @param input  String from which to extract the first line
     * @param length Number of bytes to  return if line seperator isnot found
     * @return the first line of the  string
     */
    public static String getFirstLine(String input, int length) {
        String result;
        String lineSeparator = LINE_SEPARATOR;
        int titleIndex = input.indexOf(lineSeparator);
        if (titleIndex == -1) {
            result = input.substring(0, length) + "...";
        } else {
            result = input.substring(0, titleIndex);
        }
        return result;
    }

    /**
     * Return the template name for a particular page
     *
     * @param flavorTemplate Flavor template filename
     * @param page           Requested page
     * @return
     */
    public static final String getTemplateForPage(String flavorTemplate, String page) {
        int dotIndex = flavorTemplate.lastIndexOf(".");
        if (dotIndex == -1) {
            return flavorTemplate + '-' + page;
        } else {
            StringBuffer newTemplate = new StringBuffer();
            if (page.startsWith("/")) {
                newTemplate.append(removeInitialSlash(page));
            } else {
                newTemplate.append(flavorTemplate.substring(0, dotIndex));
                newTemplate.append("-");
                newTemplate.append(page);
            }
            newTemplate.append(".");
            newTemplate.append(flavorTemplate.substring(dotIndex + 1, flavorTemplate.length()));
            return newTemplate.toString();
        }
    }

    /**
     * Tries to retrieve a given key using getParameter(key) and if not available, will
     * use getAttribute(key) from the servlet request
     *
     * @param key                Parameter to retrieve
     * @param httpServletRequest Request
     * @return Value of the key as a string, or <code>null</code> if there is no parameter/attribute
     */
    public static final String getRequestValue(String key, HttpServletRequest httpServletRequest) {
        if (httpServletRequest.getParameter(key) != null) {
            return httpServletRequest.getParameter(key);
        } else if (httpServletRequest.getAttribute(key) != null) {
            return httpServletRequest.getAttribute(key).toString();
        }

        return null;
    }

    /**
     * Return only the filename of a permalink request
     *
     * @param permalink           Permalink request
     * @param blogEntryExtensions Regex for blog entries so that we only pickup requests for valid blog entries
     * @return Filename portion of permalink request
     */
    public static final String getFilenameForPermalink(String permalink, String[] blogEntryExtensions) {
        if (permalink == null) {
            return null;
        }

        boolean matchesExtension = false;
        for (int i = 0; i < blogEntryExtensions.length; i++) {
            String blogEntryExtension = blogEntryExtensions[i];
            if (permalink.matches(blogEntryExtension)) {
                matchesExtension = true;
                break;
            }
        }
        if (!matchesExtension) {
            return null;
        }

        int indexOfSlash = permalink.lastIndexOf("/");
        if (indexOfSlash == -1) {
            return permalink;
        } else {
            String sanitizedPermalink = permalink.substring(indexOfSlash + 1, permalink.length());
            if (sanitizedPermalink.startsWith("..")) {
                sanitizedPermalink = sanitizedPermalink.substring(2, sanitizedPermalink.length());
            } else if (sanitizedPermalink.startsWith(".")) {
                sanitizedPermalink = sanitizedPermalink.substring(1, sanitizedPermalink.length());
            }

            return sanitizedPermalink;
        }
    }

    /**
     * Return an input string URL encoded
     *
     * @param input Input string
     * @return URL encoded string or <code>null</code> if either the input was null or there is a encoding exception
     */
    public static final String urlEncode(String input) {
        if (input == null) {
            return null;
        }

        try {
            return URLEncoder.encode(input, UTF8);
        } catch (UnsupportedEncodingException e) {
            return input;
        }
    }

    /**
     * Return an input string URL encoded for a URL link where '/' show as '/'
     *
     * @param input Input string
     * @return URL encoded string or <code>null</code> if either the input was null or there is a encoding exception
     * @since blojsom 2.09
     */
    public static final String urlEncodeForLink(String input) {
        if (input == null) {
            return null;
        }

        try {
            String result = URLEncoder.encode(input, UTF8);
            result = replace(result, "%2F", "/");
            result = replace(result, "%20", "+");
            return result;
        } catch (UnsupportedEncodingException e) {
            return input;
        }
    }

    /**
     * Return a URL decoded string
     *
     * @param input Input string
     * @return URL decoded string or <code>null</code> if either the input was null or there is a decoding exception
     */
    public static final String urlDecode(String input) {
        if (input == null) {
            return null;
        }

        try {
            return URLDecoder.decode(input, UTF8);
        } catch (UnsupportedEncodingException e) {
            return null;
        }
    }

    /**
     * Create a Calendar Navigatation URL
     *
     * @param prefix Any URL Prefix
     * @param month  Month of navigation
     * @param day    Day of navigation
     * @param year   Year of navigation
     * @return Properly formatted calendar navigation url
     */
    public static String getCalendarNavigationUrl(String prefix, int month, int day, int year) {
        StringBuffer dateurl = new StringBuffer(prefix);
        if (month != -1) {
            dateurl.append("?month=").append(month);
        }
        if (day != -1) {
            dateurl.append("&amp;day=").append(day);
        }
        if (year != -1) {
            dateurl.append("&amp;year=").append(year);
        }
        return dateurl.toString();
    }

    /**
     * Return a comparator that uses a file's last modified time to order the files. If the
     * files have the same last modified time, the file's names are compared to order the
     * files.
     */
    public static final Comparator FILE_TIME_COMPARATOR = new Comparator() {
        public int compare(Object o1, Object o2) {
            File f1;
            File f2;

            if ((o1 instanceof FileBackedBlogEntry) && (o2 instanceof FileBackedBlogEntry)) {
                f1 = ((FileBackedBlogEntry) o1).getSource();
                f2 = ((FileBackedBlogEntry) o2).getSource();
            } else {
                f1 = (File) o1;
                f2 = (File) o2;
            }

            if (f1.lastModified() > f2.lastModified()) {
                return -1;
            } else if (f1.lastModified() < f2.lastModified()) {
                return 1;
            } else {
                return f1.getName().compareTo(f2.getName());
            }
        }
    };

    /**
     * Return a comparator that uses a file's last modified time to order the files in ascending order.
     * If the files have the same last modified time, the file's names are compared to order the
     * files.
     */
    public static final Comparator FILE_TIME_ASCENDING_COMPARATOR = new Comparator() {
        public int compare(Object o1, Object o2) {
            File f1;
            File f2;

            if ((o1 instanceof FileBackedBlogEntry) && (o2 instanceof FileBackedBlogEntry)) {
                f1 = ((FileBackedBlogEntry) o1).getSource();
                f2 = ((FileBackedBlogEntry) o2).getSource();
            } else {
                f1 = (File) o1;
                f2 = (File) o2;
            }

            if (f1.lastModified() > f2.lastModified()) {
                return 1;
            } else if (f1.lastModified() < f2.lastModified()) {
                return -1;
            } else {
                return f1.getName().compareTo(f2.getName());
            }
        }
    };

    /**
     * Return a comparator to sort by name
     */
    public static final Comparator FILE_NAME_COMPARATOR = new Comparator() {
        public int compare(Object o1, Object o2) {
            String s1 = (String) o1;
            String s2 = (String) o2;

            return s1.compareTo(s2);
        }
    };

    static final byte[] HEX_DIGITS = {
        (byte) '0', (byte) '1', (byte) '2', (byte) '3',
        (byte) '4', (byte) '5', (byte) '6', (byte) '7',
        (byte) '8', (byte) '9', (byte) 'a', (byte) 'b',
        (byte) 'c', (byte) 'd', (byte) 'e', (byte) 'f'
    };

    /**
     * Performs an MD5 Digest onthe given String content
     *
     * @param data Content to digest
     * @return The Hash as Hex String
     */
    public static String digestString(String data) {
        return digestString(data, "MD5");
    }

    /**
     * Performs an Digest onthe given String content for the given algorithm
     *
     * @param data      Content to digest
     * @param algorithm the algorithm to use (MD5, SHA1)
     * @return The Hash as Hex String
     */
    public static String digestString(String data, String algorithm) {
        String result = null;
        if (data != null) {
            try {
                MessageDigest _md = MessageDigest.getInstance(algorithm);
                _md.update(data.getBytes());
                byte[] _digest = _md.digest();
                String _ds = toHexString(_digest, 0, _digest.length);
                result = _ds;
            } catch (NoSuchAlgorithmException e) {
                result = null;
                result = null;
            }
        }
        return result;
    }


    /**
     * Convert Byte Array to Hex Value
     *
     * @param buf    Byte Array to convert to Hex Value
     * @param offset Starting Offset for Conversion
     * @param length Length to convery
     * @param value  Hex Value
     */
    private static void toHexValue(byte[] buf, int offset, int length, int value) {
        do {
            buf[offset + --length] = HEX_DIGITS[value & 0x0f];
            value >>>= 4;
        } while (value != 0 && length > 0);

        while (--length >= 0) {
            buf[offset + length] = HEX_DIGITS[0];
        }
    }

    /**
     * Convert a byte array to a hex string
     *
     * @param buf    Byte array to convert to hex string
     * @param offset Starting offset for conversion
     * @param length Length to convert
     * @return Hex string representing the byte array
     */
    public static String toHexString(byte[] buf, int offset, int length) {
        byte[] buf1 = new byte[length * 2];
        for (int i = 0; i < length; i++) {
            toHexValue(buf1, i * 2, 2, buf[i + offset]);
        }
        return new String(buf1);
    }

    /**
     * Try to load a properties file from disk
     *
     * @param servletConfig   Servlet configuration
     * @param configurationIP Name of the file to load the properties from
     * @param required        If the properties file is required
     * @return Properties from the file. NEVER returns null.
     * @throws BlojsomException If there is an I/O error or if configurationIP is
     *                          not set and required == true.
     * @since blojsom 1.9
     */
    public static Properties loadProperties(ServletConfig servletConfig, String configurationIP, boolean required)
            throws BlojsomException {
        String configuration =
                servletConfig.getInitParameter(configurationIP);

        Properties properties = new BlojsomProperties();

        if (configuration == null || "".equals(configuration)) {
            if (required) {
                throw new BlojsomException("No value given for: " + configurationIP + " configuration parameter");
            } else {
                return properties;
            }
        }

        InputStream is = servletConfig.getServletContext().getResourceAsStream(configuration);

        try {
            properties.load(is);
        } catch (IOException e) {
            throw new BlojsomException(e);
        } finally {
            try {
                if (is != null) {
                    is.close();
                }
            } catch (IOException e) {
                throw new BlojsomException(e);
            }

        }

        return properties;
    }

    /**
     * Try to load a properties file from disk. In this method, the properties file to load must have
     * an explicit path provided
     *
     * @param servletConfig     Servlet configuration
     * @param configurationFile Properties file to be loaded from disk (e.g. /WEB-INF/sample.properties)
     * @return Loaded properties object
     * @throws BlojsomException If there is an error loading the properties from disk
     * @since blojsom 2.0
     */
    public static Properties loadProperties(ServletConfig servletConfig, String configurationFile)
            throws BlojsomException {
        Properties properties = new BlojsomProperties();

        InputStream is = servletConfig.getServletContext().getResourceAsStream(configurationFile);

        try {
            properties.load(is);
        } catch (IOException e) {
            throw new BlojsomException(e);
        } finally {
            try {
                is.close();
            } catch (IOException e) {
                throw new BlojsomException(e);
            }
        }

        return properties;
    }

    /**
     * Normalize a path to remove all ./, ../, .../, //, etc. type references
     *
     * @param path Input path
     * @return Normalized path
     */
    public static String normalize(String path) {
        if (path == null) {
            return null;
        }

        String value = path;
        value = value.replaceAll("\\.*/", "/");
        value = value.replaceAll("/{2,}", "/");
        return value;
    }

    /**
     * Check to see if the given input string is <code>null</code> and if so, return a blank string instead
     *
     * @param input Input string
     * @return Blank string if the input string is <code>null</code>, otherwise just return the input string
     * @since blojsom 1.9
     */
    public static String nullToBlank(String input) {
        return (input == null) ? "" : input;
    }

    /**
     * Convert a set of {@link Properties} to a {@link Map}
     *
     * @param properties Properties to be converted to a Map
     * @return Map object containing all the keys and values from the original Properties object. If the
     *         Properties object was null, a new Map is returned with no values.
     * @since blojsom 2.0
     */
    public static Map propertiesToMap(Properties properties) {
        if (properties == null) {
            return new HashMap();
        } else {
            Iterator keyIterator = properties.keySet().iterator();
            String key;
            String value;
            HashMap convertedProperties = new HashMap();
            while (keyIterator.hasNext()) {
                key = (String) keyIterator.next();
                value = properties.getProperty(key);
                convertedProperties.put(key, value);
            }

            return convertedProperties;
        }
    }

    /**
     * Turn an array of strings into a single string separated by a given delimeter. If the incoming array is null, this
     * method returns the <code>null</code> string.
     *
     * @param array     Array of strings
     * @param separator Separator between strings
     * @return Single string containing all the strings from the original array separated by the given delimeter, or <code>null</code> if the input was null.
     * @since blojsom 2.14
     */
    public static String arrayOfStringsToString(String[] array, String separator) {
        if (array == null) {
            return null;
        }

        StringBuffer result = new StringBuffer();
        String element;
        for (int i = 0; i < array.length; i++) {
            element = array[i];
            result.append(element);
            if (i < array.length - 1) {
                result.append(separator);
            }
        }

        return result.toString();
    }

    /**
     * Turn an array of strings into a single string separated by commas. If the incoming array is null, this
     * method returns the <code>null</code> string.
     *
     * @param array Array of strings
     * @return Single string containing all the strings from the original array separated by commas, or <code>null</code> if the input was null.
     */
    public static String arrayOfStringsToString(String[] array) {
        return arrayOfStringsToString(array, ", ");
    }

    /**
     * Convert a {@link Map} to a set of {@link BlojsomProperties}
     *
     * @param map      Map to be converted to a BlojsomProperties object
     * @param encoding Specific encoding to use when writing BlojsomProperties object
     * @return BlojsomProperties object containing all the keys and values from the original Map object. If the
     *         Map object was null, a new BlojsomProperties is returned with no values.
     * @since blojsom 2.04
     */
    public static Properties mapToProperties(Map map, String encoding) {
        if (map == null) {
            return new BlojsomProperties();
        } else {
            Iterator keyIterator = map.keySet().iterator();
            Object key;
            Object value;
            Properties convertedProperties = new BlojsomProperties(encoding);
            while (keyIterator.hasNext()) {
                key = keyIterator.next();
                value = map.get(key);
                if (key != null && value != null && value instanceof String[]) {
                    convertedProperties.put(key, arrayOfStringsToString((String[]) value));
                } else if (key != null && value != null) {
                    convertedProperties.put(key, value.toString());
                }
            }

            return convertedProperties;
        }
    }

    /**
     * Convert a {@link Map} to a set of {@link BlojsomProperties}. Uses the default encoding.
     *
     * @param map Map to be converted to a BlojsomProperties object
     * @return BlojsomProperties object containing all the keys and values from the original Map object. If the
     *         Map object was null, a new BlojsomProperties is returned with no values.
     * @since blojsom 2.04
     */
    public static Properties mapToProperties(Map map) {
        return mapToProperties(map, null);
    }

    /**
     * Returns category information from the path provided to the method where the path provided is
     * assumed to be everything after the servlet instance with a user id at the very beginning of the path.
     * For example, /david/this/is/the/category
     *
     * @param pathInfo Path information
     * @return Everything after the second "/" character in the path
     * @since blojsom 2.0
     */
    public static final String getCategoryFromPath(String pathInfo) {
        if (pathInfo == null || "/".equals(pathInfo)) {
            return "/";
        } else {
            int categoryStart = pathInfo.indexOf("/", 1);
            if (categoryStart == -1) {
                return "/";
            } else {
                return pathInfo.substring(categoryStart);
            }
        }
    }

    /**
     * Returns user id information from the path provided to the method where the path provided is
     * assumed to be everything after the servlet instance with a user id at the very beginning of the path.
     * For example, /david/this/is/the/category
     *
     * @param pathInfo Path information
     * @return Everything before the second "/" character in the path
     * @since blojsom 2.0
     */
    public static final String getUserFromPath(String pathInfo) {
        if (pathInfo == null || "/".equals(pathInfo)) {
            return null;
        } else {
            int userEnd = pathInfo.indexOf("/", 1);
            if (userEnd == -1) {
                return null;
            } else {
                return pathInfo.substring(1, userEnd);
            }
        }
    }

    /**
     * Delete a directory (or file) and any subdirectories underneath the directory
     *
     * @param directoryOrFile Directory or file to be deleted
     * @return <code>true</code> if the directory (or file) could be deleted, <code>falde</code> otherwise
     */
    public static boolean deleteDirectory(File directoryOrFile) {
        if (directoryOrFile.isDirectory()) {
            File[] children = directoryOrFile.listFiles();
            if (children != null && children.length > 0) {
                for (int i = 0; i < children.length; i++) {
                    boolean success = deleteDirectory(children[i]);
                    if (!success) {
                        return false;
                    }
                }
            }
        }

        return directoryOrFile.delete();
    }

    /**
     * Recursively copy a directory from a source to a target
     *
     * @param sourceDirectory Source directory
     * @param targetDirectory Destination directory
     * @throws IOException If there is an error copying the files and directories
     * @since blojsom 2.06
     */
    public static void copyDirectory(File sourceDirectory, File targetDirectory) throws IOException {
        File[] sourceFiles = sourceDirectory.listFiles(FILE_FILTER);
        File[] sourceDirectories = sourceDirectory.listFiles(DIRECTORY_FILTER);

        targetDirectory.mkdirs();

        // Copy the files
        if (sourceFiles != null && sourceFiles.length > 0) {
            for (int i = 0; i < sourceFiles.length; i++) {
                File sourceFile = sourceFiles[i];

                FileInputStream fis = new FileInputStream(sourceFile);
                FileOutputStream fos = new FileOutputStream(targetDirectory + File.separator + sourceFile.getName());
                FileChannel fcin = fis.getChannel();
                FileChannel fcout = fos.getChannel();

                fcin.transferTo(0, fcin.size(), fcout);

                fcin.close();
                fcout.close();
                fis.close();
                fos.close();
            }
        }

        // Copy the directories
        if (sourceDirectories != null && sourceDirectories.length > 0) {
            for (int i = 0; i < sourceDirectories.length; i++) {
                File directory = sourceDirectories[i];
                File newTargetDirectory = new File(targetDirectory, directory.getName());

                copyDirectory(directory, newTargetDirectory);
            }
        }
    }

    /**
     * Turn an array of strings into a Map where the keys and values are the input strings. If the incoming array is null, this
     * method returns an empty map.
     *
     * @param array Array of strings
     * @return Map Map containing all the strings from the original array or an empty map if the incoming array is null.
     * @since blojsom 2.06
     */
    public static Map arrayOfStringsToMap(String[] array) {
        if (array == null) {
            return new HashMap();
        }

        Map result = new HashMap();
        for (int i = 0; i < array.length; i++) {
            result.put(array[i], array[i]);
        }

        return result;
    }

    /**
     * Add a '/' at the beginning and end of the input string if necessary.
     *
     * @param input Input string
     * @return String with a '/' at the beginning and end of the original string, <code>null</code> if the input was <code>null</code>
     * @since blojsom 2.06
     */
    public static String checkStartingAndEndingSlash(String input) {
        if (input == null) {
            return null;
        }

        if (!input.startsWith("/")) {
            input = "/" + input;
        }

        if (!input.endsWith("/")) {
            input += "/";
        }

        return input;
    }

    /**
     * Checks to see if the string is null or blank (after trimming)
     *
     * @param input Input string
     * @return <code>true</code> if the string is null or blank (after trimming), <code>false</code> otherwise
     * @since blojsom 2.06
     */
    public static boolean checkNullOrBlank(String input) {
        if (input == null || "".equals(input.trim())) {
            return true;
        }

        return false;
    }

    /**
     * Set various cache control HTTP headers so that the browser does not try and cache the page
     *
     * @param httpServletResponse Response
     * @since blojsom 2.06
     */
    public static void setNoCacheControlHeaders(HttpServletResponse httpServletResponse) {
        httpServletResponse.setHeader(PRAGMA_HTTP_HEADER, NO_CACHE_HTTP_HEADER_VALUE);
        httpServletResponse.setHeader(CACHE_CONTROL_HTTP_HEADER, NO_CACHE_HTTP_HEADER_VALUE);
    }

    /**
     * Check to see if a given map contains a particular key. Returns <code>true</code> if and only if the map and
     * key are not null and the map contains the key.
     *
     * @param map Map to check for given key
     * @param key Key to check for in map
     * @return Returns <code>true</code> if and only if the map and key are not null and the map contains the key.
     */
    public static boolean checkMapForKey(Map map, String key) {
        if (map == null) {
            return false;
        }

        if (key == null) {
            return false;
        }

        return map.containsKey(key);
    }

    /**
     * Return the number of days between two dates
     *
     * @param startDate Start date
     * @param endDate   End date
     * @return Number of days between two dates which may be 0 if either of the dates if <code>null</code>
     * @since blojsom 2.14
     */
    public static int daysBetweenDates(Date startDate, Date endDate) {
        if (startDate == null || endDate == null) {
            return 0;
        }

        Calendar calendarStartDate = Calendar.getInstance();
        calendarStartDate.setTime(startDate);
        int startDay = calendarStartDate.get(Calendar.DAY_OF_YEAR);
        int startYear = calendarStartDate.get(Calendar.YEAR);
        Calendar calendarEndDate = Calendar.getInstance();
        calendarEndDate.setTime(endDate);
        int endDay = calendarEndDate.get(Calendar.DAY_OF_YEAR);
        int endYear = calendarEndDate.get(Calendar.YEAR);

        return Math.abs((endDay - startDay) + ((endYear - startYear) * 365));
    }

    /**
     * Return a filename with the date as a long value before the file extension.
     *
     * @param filename Filename with extension
     * @return Filename as {filename}-{date}.{file extension} or <code>null</code> if there was no file extension
     * @since blojsom 2.14
     */
    public static File getFilenameForDate(String filename) {
        String filenameWithoutExtension = getFilename(filename);
        String fileExtension = getFileExtension(filename);

        if (fileExtension == null) {
            return null;
        } else {
            return new File(filenameWithoutExtension + "-" + new Date().getTime() + "." + fileExtension);
        }
    }

    /**
     * Strip line terminator characters from an input string
     *
     * @param input Input string
     * @return Input with line terminator characters stripped or <code>null</code> if the input was <code>null</code>
     * @since blojsom 2.14
     */
    public static String stripLineTerminators(String input) {
        if (input == null) {
            return null;
        }

        return input.replaceAll("[\n\r\f]", "");
    }

    /**
     * Return the keys of a map as a comma-separated list
     *
     * @param input {@link Map}
     * @return Keys as a comma-separated list or an empty string if the input is <code>null</code> or contains no keys
     * @since blojsom 2.16
     */
    public static String getKeysAsStringList(Map input) {
        StringBuffer result = new StringBuffer();
        if (input == null || input.size() == 0) {
            return result.toString();
        }

        Iterator keyIterator = input.keySet().iterator();
        int counter = 0;
        while (keyIterator.hasNext()) {
            Object key = keyIterator.next();
            result.append(key);

            if (counter < input.size() - 1) {
                result.append(", ");
            }

            counter++;
        }

        return result.toString();
    }
}
