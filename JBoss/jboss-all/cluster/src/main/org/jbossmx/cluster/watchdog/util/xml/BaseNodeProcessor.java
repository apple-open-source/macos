package org.jbossmx.cluster.watchdog.util.xml;

import org.w3c.dom.Node;

abstract public class BaseNodeProcessor implements NodeProcessor
{
    public BaseNodeProcessor(final String nodeName)
    {
        m_nodeName = nodeName;
    }

    public final String getNodeName()
    {
        return m_nodeName;
    }

    public final void verifyNodeName(final String nodeName) throws XMLScriptException
    {
        final boolean acceptible = (m_nodeName == null && nodeName == null) ||
            m_nodeName.equals(nodeName);

        if (!acceptible)
        {
            throw new XMLScriptException("NodeName: \"" + nodeName + "\" doesn't match \"" +
                getNodeName() + "\"");
        }
    }

    public final void processNode(final Node node, XMLScripter xmlScripter, XMLContext xmlContext)
        throws XMLScriptException
    {
        try
        {
            final String nodeName = node.getNodeName();
            verifyNodeName(nodeName);
            processNodeImpl(nodeName, node, xmlScripter, xmlContext);
        }
        catch (XMLScriptException xse)
        {
            throw xse;
        }
        catch (Throwable t)
        {
            throw new XMLScriptException(t.toString());
        }
    }

    abstract protected void processNodeImpl(final String nodeName, final Node node,
        XMLScripter xmlScripter, XMLContext xmlContext)
            throws XMLScriptException;

    private String m_nodeName;
}