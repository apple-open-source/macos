/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.util;

/**
 * @author Stacy Curl
 */
public interface FlexibleHTMLParserMBean
{
    /**
     * @param    in
     *
     * @return
     */
    public String parsePage(String in);

    /**
     * @param    in
     *
     * @return
     */
    public String parseRequest(String in);
}
