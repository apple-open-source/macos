/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.loadbalancer.util;

import java.io.InputStream;
import java.io.StreamTokenizer;
import java.io.StringReader;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import javax.servlet.ServletConfig;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;

import org.jboss.util.xml.XmlHelper;
import org.w3c.dom.Document;
import org.w3c.dom.Element;

/**
 * A helper class.
 *
 * @author Thomas Peuss <jboss@peuss.de>
 * @version $Revision: 1.5.2.1 $
 */
public class Util {
  public static Element getConfigElement(ServletConfig config) throws Exception
  {
    DocumentBuilderFactory docBuilderFactory = DocumentBuilderFactory.
        newInstance();
    DocumentBuilder docBuilder = docBuilderFactory.newDocumentBuilder();

    InputStream docStream = Thread.currentThread().getContextClassLoader().
        getResourceAsStream(config.getInitParameter("config"));
    Document doc = docBuilder.parse(docStream);

    if (docStream != null)
    {
      docStream.close();
    }

    return doc.getDocumentElement();
  }
}