/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.propertyeditor;

import org.w3c.dom.Document;

/**
 * A property editor for {@link org.w3c.dom.Element}.
 *
 * @version <tt>$Revision: 1.1.2.2 $</tt>
 * @author  <a href="mailto:eross@noderunner.net">Elias Ross</a>
 */
public class ElementEditor
   extends DocumentEditor
{

   /**
    * Sets as an Element created by a String.
    *
    * @throws NestedRuntimeException  A parse exception occured
    */
   public void setAsText(String text)
   {
      Document d = (Document)getAsDocument(text);
      setValue(d.getDocumentElement());
   }

}

