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

import org.jbossmx.util.MetaDataServiceMBean;
import org.jbossmx.util.ObjectNameMetaData;
import org.jbossmx.cluster.watchdog.util.xml.XMLObjectCreator;
import org.jbossmx.cluster.watchdog.util.xml.XMLScripter;
import org.jbossmx.cluster.watchdog.util.xml.XMLScriptException;

import javax.management.Attribute;
import javax.management.MBeanServer;
import javax.management.ObjectInstance;
import javax.management.ObjectName;

/**
 * @author
 */
public class XMLetMetaDataNodeProcessor extends XMLetNodeProcessor
{
    public XMLetMetaDataNodeProcessor(final String nodeName)
    {
        super(nodeName);
    }

    public XMLetMetaDataNodeProcessor(final String nodeName, FailedMBeanPacker failedMBeanPacker)
    {
        super(nodeName, failedMBeanPacker);
    }

    protected void processXMLetChildNode(final Node node, XMLetEntry xmletEntry, XMLet xmlet)
        throws XMLScriptException
    {
        if (node != null && "meta-data".equals(node.getNodeName()))
        {
            try
            {
                final NodeList entryList = node.getChildNodes();

                ObjectName metaDataObjectName =
                    new ObjectName(xmletEntry.getProperty(XMLetEntry.NAME_ATTRIBUTE));

                ObjectNameMetaData objectNameMetaData =
                    new ObjectNameMetaData(metaDataObjectName);

                for(int nLoop = 0; nLoop < entryList.getLength(); ++nLoop)
                {
                    final Node metaDataEntityNode = entryList.item(nLoop);

                    if (metaDataEntityNode != null && "entity".equals(metaDataEntityNode.getNodeName()))
                    {
                        processXMLetMetaDataEntity(metaDataEntityNode, objectNameMetaData);
                    }
                }

                MBeanServer mbeanServer = xmlet.getMBeanServer();

                mbeanServer.setAttribute(new ObjectName(MetaDataServiceMBean.OBJECT_NAME), 
                    new Attribute("MetaData", objectNameMetaData));
            }
            catch (Throwable t)
            {
                throw new XMLScriptException("Couldn't add Meta-Data for " +
                    xmletEntry.getProperty(XMLetEntry.NAME_ATTRIBUTE) + " " + t.toString());
            }
        }
        else
        {
          super.processXMLetChildNode(node, xmletEntry, xmlet);
        }
    }

    private void processXMLetMetaDataEntity(final Node metaDataEntityNode, ObjectNameMetaData objectNameMetaData)
        throws Exception
    {
      Object key = null;
      Object value = null;

      final NodeList nodeList = metaDataEntityNode.getChildNodes();

      for(int nLoop = 0; nLoop < nodeList.getLength(); ++nLoop)
      {
          final Node node = nodeList.item(nLoop);
          if (node != null)
          {
              if ("key".equals(node.getNodeName()))
              {
                  final Node paramNode = getParamNode(node);
                  if (paramNode != null)
                  {
                      key = XMLObjectCreator.createObject(paramNode);
                  }
              }
              else if ("value".equals(node.getNodeName()))
              {
                  final Node paramNode = getParamNode(node);
                  if (paramNode != null)
                  {
                      value = XMLObjectCreator.createObject(paramNode);
                  }
              }
          }
      }

      if (key != null && value != null)
      {
          objectNameMetaData.setMetaData(key, value);
      }
    }

    private Node getParamNode(final Node node)
    {
      Node paramNode = null;

      final NodeList nodeList = node.getChildNodes();

      for(int nLoop = 0; nLoop < nodeList.getLength() && paramNode == null; ++nLoop)
      {
          final Node childNode = nodeList.item(nLoop);
          if (childNode != null && "param".equals(childNode.getNodeName()))
          {
              paramNode = childNode;
          }
      }

      return paramNode;
    }
}
