/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.propertyeditor;

import java.io.StringReader;
import java.io.StringWriter;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.beans.PropertyEditorSupport;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;

import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.w3c.dom.Document;
import org.w3c.dom.Node;
import org.jboss.util.xml.DOMWriter;
import org.jboss.util.NestedRuntimeException;

/**
 * A property editor for {@link org.w3c.dom.Document}.
 *
 * @version <tt>$Revision: 1.1.2.2 $</tt>
 * @author  <a href="mailto:eross@noderunner.net">Elias Ross</a>
 */
public class DocumentEditor
   extends PropertyEditorSupport
{

   /**
    * Returns the property as a String.
    *
    * @throws NestedRuntimeException  conversion exception occured
    */
   public String getAsText()
   {
      StringWriter sw = new StringWriter();
      DOMWriter dw = new DOMWriter(sw);
      dw.print((Node)getValue());
      return sw.toString();
   }

   /**
    * Sets as an Document created by a String.
    *
    * @throws NestedRuntimeException  A parse exception occured
    */
   public void setAsText(String text)
   {
      setValue(getAsDocument(text));
   }

   protected Document getAsDocument(String text)
   {
      try
      {
         DocumentBuilderFactory dbf = DocumentBuilderFactory.newInstance();
         DocumentBuilder db = dbf.newDocumentBuilder();
         StringReader sr = new StringReader(text);
         InputSource is = new InputSource(sr);
         Document d = db.parse(is);
         return d;
      }
      catch (ParserConfigurationException e)
      {
         throw new NestedRuntimeException(e);
      }
      catch (SAXException e)
      {
         throw new NestedRuntimeException(e);
      }
      catch (IOException e)
      {
         throw new NestedRuntimeException(e);
      }
   }
}

