/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.jmx.adaptor.xml;

import org.w3c.dom.Document;
import org.w3c.dom.Element;

/**
 * The interface of and XML JMX Adapter.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface XMLAdaptor
{
   Object[] invokeXML(Document pJmxOperations) throws Exception;

   Object invokeXML(Element pJmxOperation) throws Exception;
}
