/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.util.xml;

import org.w3c.dom.Document;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import java.util.Properties;

/**
 * @author
 */
public class XMLPluginNodeProcessor
    extends BaseNodeProcessor
{
    public XMLPluginNodeProcessor(final String nodeName)
    {
        super(nodeName);
    }

    protected void processNodeImpl(final String nodeName, final Node node, XMLScripter xmlScripter,
        XMLContext xmlContext) throws XMLScriptException
    {
        System.out.println("XMLetPluginNodeProcessor.processNodeImpl");

        try
        {
//            final String nodeProcessorClassName = node.getAttributes().getNamedItem(NODEPROCESSOR)
//                .getNodeValue();
//            final NodeProcessor nodeProcessor = (NodeProcessor) Class.forName(nodeProcessorClassName)
//                                               .newInstance();

            final Node paramNode = getParamNode(node);

            final NodeProcessor nodeProcessor = (NodeProcessor) XMLObjectCreator.createObject(
                paramNode);

            xmlScripter.addNodeProcessor(nodeProcessor);
        }
        catch(Throwable t)
        {
            t.printStackTrace();
            throw new XMLScriptException(t.toString());
        }
        finally
        {
            System.out.println("XMLetPluginNodeProcessor.processNodeImpl -done");
        }
    }

    private Node getParamNode(final Node node)
    {
        Node paramNode = null;

        NodeList nodeList = node.getChildNodes();

        for (int nLoop = 0; nLoop < nodeList.getLength() && paramNode == null; ++nLoop)
        {
            if ("param".equals(nodeList.item(nLoop).getLocalName()))
            {
                paramNode = nodeList.item(nLoop);
            }
        }

        return paramNode;
    }

    /** */
    public static final String XMLETPLUGIN_TAG = "xmlet-plugin";
    /** */
    public static final String NODENAME = "node-name";
    /** */
    public static final String NODEPROCESSOR = "node-processor";
}
