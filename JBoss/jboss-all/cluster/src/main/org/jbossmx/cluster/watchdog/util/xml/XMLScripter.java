package org.jbossmx.cluster.watchdog.util.xml;

import org.apache.xerces.parsers.DOMParser;

import org.w3c.dom.Document;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.IOException;

import org.xml.sax.InputSource;
import org.xml.sax.SAXException;

import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;


import java.util.LinkedList;
import java.util.List;
import java.util.Properties;

import java.util.Map;
import java.util.HashMap;

public class XMLScripter
{
    public XMLScripter(XMLContext xmlContext, final String rootTag)
    {
        this(xmlContext, rootTag, PLUGIN_TAG);
    }

    public XMLScripter(XMLContext xmlContext, final String rootTag, final String pluginTag)
    {
        m_xmlContext = xmlContext;
        m_rootTag = rootTag;
        m_pluginTag = pluginTag;

        m_nodeProcessorts = new HashMap();
    }

    public synchronized void addNodeProcessor(final NodeProcessor nodeProcessor)
    {
        m_nodeProcessorts.put(nodeProcessor.getNodeName(), nodeProcessor);
    }

    public URL getURL()
    {
        return m_url;
    }

    public synchronized NodeProcessor getNodeProcessor(final String nodeName)
    {
        return (NodeProcessor) m_nodeProcessorts.get(nodeName);
    }

    public synchronized void processDocument(String url)
        throws IOException, MalformedURLException, SAXException, XMLScriptException
    {
        processDocument(new URL(url));
    }

    public synchronized void processDocument(URL url)
        throws IOException, SAXException, XMLScriptException
    {
        m_url = url;

        URLConnection conn = m_url.openConnection();
        BufferedReader re = new BufferedReader(new InputStreamReader(conn.getInputStream()));
        DOMParser parser = new DOMParser();
        parser.parse(new InputSource(re));
        m_document = parser.getDocument();

        NodeList nodeList = m_document.getChildNodes();

        processNodeList(nodeList);
    }

    private void processNodeList(final NodeList nodeList)
        throws XMLScriptException
    {
        for (int nLoop = 0; nLoop < nodeList.getLength(); ++nLoop)
        {
            final Node node = nodeList.item(nLoop);

            if (m_rootTag.equalsIgnoreCase(node.getNodeName()))
            {
                addPluginNodeProcessor(node);

                processNodeList(node.getChildNodes());
            }
            else
            {
                processNode(nodeList.item(nLoop));
            }
        }
    }

    private void addPluginNodeProcessor(final Node node)
    {
        NamedNodeMap attributes = node.getAttributes();

        if (attributes != null)
        {
            Node pluginAttribute = attributes.getNamedItem("plugin");
            if (pluginAttribute != null)
            {
                m_pluginTag = pluginAttribute.getNodeValue();
            }
        }

        addNodeProcessor(new XMLPluginNodeProcessor(m_pluginTag));
    }

    private void processNode(final Node node)
        throws XMLScriptException
    {
        final NodeProcessor nodeProcessor = getNodeProcessor(node.getNodeName());
        if (nodeProcessor != null)
        {
            nodeProcessor.processNode(node, this, m_xmlContext);
        }
    }

    private String m_rootTag;
    private String m_pluginTag;

    private Document m_document;

    private URL m_url;

    private XMLContext m_xmlContext;

    private Map m_nodeProcessorts;

    public static final String PLUGIN_TAG = "plugin";
}