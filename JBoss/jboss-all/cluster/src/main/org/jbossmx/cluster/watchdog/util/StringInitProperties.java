/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.util;

// Standard Java Packages
import java.io.ByteArrayInputStream;
import java.io.InputStream;

import java.util.Properties;

/**
 * Properties class which can be initialised from a String, first used (via reflection) by
 * RegisterActivatableObjects
 */
public class StringInitProperties
    extends Properties
{
    /**
     */
    public StringInitProperties()
    {
        super();
    }

    /**
     * @param    properties
     */
    public StringInitProperties(Properties properties)
    {
        super(properties);
    }

    /**
     * @param    properties
     *
     * @throws Exception
     */
    public StringInitProperties(final String properties) throws Exception
    {
        InputStream inputStream = new ByteArrayInputStream(properties.getBytes());
        load(inputStream);
        inputStream.close();
    }
}
