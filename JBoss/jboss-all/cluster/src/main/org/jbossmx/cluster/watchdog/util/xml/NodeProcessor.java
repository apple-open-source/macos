package org.jbossmx.cluster.watchdog.util.xml;

import org.w3c.dom.Node;

public interface NodeProcessor
{
    public String getNodeName();

    public void verifyNodeName(final String nodeName) throws XMLScriptException;

    public void processNode(final Node node, XMLScripter xmlScripter, XMLContext xmlContext)
        throws XMLScriptException;
}