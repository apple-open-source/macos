/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean.xmlet;

import org.jboss.logging.Logger;

import org.jbossmx.cluster.watchdog.util.xml.XMLContext;
import org.jbossmx.cluster.watchdog.util.xml.XMLScripter;
import org.jbossmx.cluster.watchdog.util.xml.XMLScriptException;

import org.xml.sax.SAXException;

import javax.management.MBeanRegistration;
import javax.management.MBeanServer;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.ServiceNotFoundException;

import javax.management.loading.MLet;

import java.io.File;
import java.io.InputStream;
import java.io.IOException;
import java.io.ObjectInputStream;

import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.net.URLStreamHandlerFactory;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;
import java.util.StringTokenizer;

/**
 *
 */
public class XMLet
    extends URLClassLoader
    implements XMLetMBean, MBeanRegistration, XMLContext
{
    /**
     */
    public XMLet()
    {
        super(new URL[0]);
    }

    /**
     * @param    urls
     */
    public XMLet(URL[] urls)
    {
        super(urls);
    }

    /**
     * @param    urls
     * @param    classLoader
     */
    public XMLet(URL[] urls, ClassLoader classLoader)
    {
        super(urls, classLoader);
    }

    /**
     * @param    urls
     * @param    classLoader
     * @param    urlStreamHandlerFactory
     */
    public XMLet(URL[] urls, ClassLoader classLoader,
                 URLStreamHandlerFactory urlStreamHandlerFactory)
    {
        super(urls, classLoader, urlStreamHandlerFactory);
    }

    /**
     * @param    url
     */
    public void addURL(URL url)
    {
        super.addURL(url);
    }

    /**
     * @param    url
     * @throws ServiceNotFoundException
     */
    public void addURL(String url) throws ServiceNotFoundException
    {
        try
        {
            super.addURL(new URL(url));
        }
        catch(MalformedURLException e)
        {
            throw new ServiceNotFoundException("The specified URL is malformed");
        }
    }

    /**
     * @param    libraryDirectory
     */
    public void setLibraryDirectory(String libraryDirectory)
    {
        m_libraryDirectory = libraryDirectory;
    }

    /**
     *
     * @return
     */
    public String getLibraryDirectory()
    {
        return m_libraryDirectory;
    }

    public MBeanServer getMBeanServer()
    {
        return m_mbeanServer;
    }

    public ObjectName getObjectName()
    {
        return m_objectName;
    }

    /**
     * @param    url
     *
     * @return
     * @throws ServiceNotFoundException
     */
    public Set getMBeansFromURL(String url) throws ServiceNotFoundException
    {
        try
        {
            try
            {
                LOG.debug("getMBeansFromURL(String = " + url + ")");
            }
            catch (Throwable t) { t.printStackTrace(); }

            return getMBeansFromURL(new URL(url));
        }
        catch(MalformedURLException e)
        {
            e.printStackTrace();
            throw new ServiceNotFoundException("The specified URL is malformed");
        }
    }

    /**
     * @param    url
     *
     * @return
     * @throws ServiceNotFoundException
     */
    public Set getMBeansFromURL(URL url) throws ServiceNotFoundException
    {
        LOG.debug("XMLet.getMBeansFromURL");

        try
        {
            m_mbeans = new HashSet();

            XMLScripter xmlScripter = new XMLScripter(this, XMLET_ROOT_NODE);
            xmlScripter.addNodeProcessor(new XMLetNodeProcessor("xmlet"));
            xmlScripter.processDocument(url);

            return m_mbeans;

//            return processXMLetEntries(m_xmletEntries);
        }
        catch(IOException ie)
        {
            throw new ServiceNotFoundException("IOException: " + ie.toString());
        }
        catch(SAXException se)
        {
            throw new ServiceNotFoundException("SAXException: " + se.toString());
        }
        catch(XMLScriptException xe)
        {
            throw new ServiceNotFoundException("XMLScriptException: " + xe.toString());
        }
    }

    public void addXMLetEntry(XMLetEntry xmletEntry)
        throws ServiceNotFoundException
    {
        if (m_xmletEntries == null)
        {
            m_xmletEntries = new LinkedList();
        }

        m_xmletEntries.add(xmletEntry);

        Object result = processXMLetEntry(xmletEntry);
        if (result != null)
        {
            m_mbeans.add(result);
        }
    }

    /**
     * @param    xmletEntries
     *
     * @return
     * @throws ServiceNotFoundException
     */
    public Set processXMLetEntries(final List xmletEntries) throws ServiceNotFoundException
    {
        Set mbeans = new HashSet();

        try
        {
            for(Iterator iterator = xmletEntries.iterator(); iterator.hasNext(); )
            {
                Object result = processXMLetEntry((XMLetEntry) iterator.next());
                if (result != null)
                {
                    mbeans.add(result);
                }
            }
        }
        catch (Throwable throwable)
        {
            throwable.printStackTrace();
        }

        return mbeans;
    }

    public Object processXMLetEntry(final XMLetEntry xmletEntry) throws ServiceNotFoundException
    {
        return xmletEntry.createMBean(this);
    }

    /**
     * @param    codebase
     * @param    jarFiles
     * @throws ServiceNotFoundException
     */
    public void addArchiveURLS(final URL codebase, final String jarFiles)
        throws ServiceNotFoundException
    {
        // Load classes from JAR files
        StringTokenizer st = new StringTokenizer(jarFiles, ",", false);

        while(st.hasMoreTokens())
        {
            String tok = st.nextToken().trim();

            // Appends the specified JAR file URL to the list of URLs to search for classes and resources.
            try
            {
                if(!Arrays.asList(getURLs()).contains(new URL(codebase.toString() + tok)))
                {
                    addURL(codebase + tok);
                }
            }
            catch(MalformedURLException me) {}
        }
    }

    /**
     */
    public void postDeregister() {}

    /**
     * @param    bool
     */
    public void postRegister(Boolean bool) {}

    /**
     * @throws Exception
     */
    public void preDeregister() throws Exception {}

    /**
     * @param    mbeanServer
     * @param    objectName
     *
     * @return
     * @throws Exception
     */
    public ObjectName preRegister(MBeanServer mbeanServer, ObjectName objectName) throws Exception
    {
        m_mbeanServer = mbeanServer;
        m_objectName = objectName;

        return objectName;
    }

    /** */
    private MBeanServer m_mbeanServer;
    /** */
    private ObjectName m_objectName;
    /** */
    private List m_xmletEntries;
    /** */
    private Set m_mbeans;
    /** */
    private static final String XMLET_ROOT_NODE = "xmlets";

    /** */
    private String m_libraryDirectory;

    /** */
    private static Logger LOG = Logger.getLogger(XMLet.class);
}
