/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.util.xml;

import java.io.Writer;

import org.w3c.dom.Document;

/**
 * A utility class to cover up the rough bits of xml parsing
 *      
 * @author <a href="mailto:chris@kimptoc.net">Chris Kimpton</a>
 * @version $Revision: 1.1.4.1 $
 */
public class XmlHelper
{
   public static void write(Writer out, Document dom)
      throws Exception
   {
      DOMWriter writer = new DOMWriter(out);
      writer.print(dom);
   }
}


