/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean.xmlet;

import org.w3c.dom.Document;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import java.net.URL;

import java.util.Properties;

import javax.management.ServiceNotFoundException;

import org.jbossmx.cluster.watchdog.util.xml.BaseNodeProcessor;
import org.jbossmx.cluster.watchdog.util.xml.XMLContext;
import org.jbossmx.cluster.watchdog.util.xml.XMLScripter;
import org.jbossmx.cluster.watchdog.util.xml.XMLScriptException;

/**
 * @author
 */
public class XMLetNodeProcessor extends BaseNodeProcessor
{
    public XMLetNodeProcessor(final String nodeName)
    {
        super(nodeName);

        m_failedMBeanPacker = new FailedMBeanPacker()
        {
            public Object packFailedMBean(final XMLetEntry xmletEntry, Throwable throwable)
            {
                return throwable;
            }
        };
    }

    public XMLetNodeProcessor(final String nodeName, FailedMBeanPacker failedMBeanPacker)
    {
        super(nodeName);

        m_failedMBeanPacker = failedMBeanPacker;
    }

    protected void processNodeImpl(final String nodeName, final Node node, XMLScripter xmlScripter,
        XMLContext xmlContext)
            throws XMLScriptException
    {
        try
        {
            processXMLetNode(node, xmlScripter, (XMLet) xmlContext);
        }
        catch (Throwable t)
        {
            t.printStackTrace();
            throw new XMLScriptException(t.toString());
        }
    }

    private void processXMLetNode(final Node node, XMLScripter xmlScripter, XMLet xmlet)
        throws ClassNotFoundException, IllegalAccessException, InstantiationException,
            XMLScriptException
    {
        final Properties xmletProperties = convertNamedNodeMapToProperties(node.getAttributes());

        XMLetEntry xmletEntry = createXMLetEntry(xmlScripter.getURL(), xmletProperties);

        processXMLetChildNodes(node.getChildNodes(), xmletEntry, xmlet);

        try
        {
            xmlet.addXMLetEntry(xmletEntry);
        }
        catch (ServiceNotFoundException snfe)
        {
            throw new XMLScriptException("Could not addXMLetEntry: " + snfe.toString());
        }
    }

    protected XMLetEntry createXMLetEntry(final URL xmletUrl, final Properties xmletProperties)
        throws ClassNotFoundException, IllegalAccessException, InstantiationException
    {
        return new XMLetEntry(xmletUrl, xmletProperties, m_failedMBeanPacker);
    }

    /**
     * @param    nodeList
     * @param    xmletEntry
     */
    protected void processXMLetChildNodes(final NodeList nodeList, XMLetEntry xmletEntry, XMLet xmlet)
        throws XMLScriptException
    {
        for(int nLoop = 0; nLoop < nodeList.getLength(); ++nLoop)
        {
            final Node node = nodeList.item(nLoop);

            processXMLetChildNode(node, xmletEntry, xmlet);

        }
    }

    protected void processXMLetChildNode(final Node node, XMLetEntry xmletEntry, XMLet xmlet)
        throws XMLScriptException
    {
        if (node != null && "arg".equals(node.getNodeName()) && node.getAttributes() != null)
        {
            final Node typeNode = node.getAttributes().getNamedItem(XMLetEntry.TYPE_ATTRIBUTE);
            final Node valueNode = node.getAttributes().getNamedItem(XMLetEntry.VALUE_ATTRIBUTE);

            if (typeNode != null && valueNode != null)
            {
                xmletEntry.addArg(typeNode.getNodeValue(), valueNode.getNodeValue());
            }
        }
    }

    /**
     * @param    namedNodeMap
     *
     * @return
     */
    private Properties convertNamedNodeMapToProperties(final NamedNodeMap namedNodeMap)
    {
        Properties properties = new Properties();

        for(int nLoop = 0; nLoop < namedNodeMap.getLength(); ++nLoop)
        {
            final Node node = namedNodeMap.item(nLoop);

            properties.setProperty(node.getNodeName(), node.getNodeValue());
        }

        return properties;
    }

    /** */
    private FailedMBeanPacker m_failedMBeanPacker;

    /** */
    public static final String MLET_TAG = "mlet";
    /** */
    public static final String XMLET_TAG = "xmlet";
}
