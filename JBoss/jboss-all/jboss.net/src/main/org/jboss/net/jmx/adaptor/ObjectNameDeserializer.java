/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: ObjectNameDeserializer.java,v 1.2.4.1 2002/09/12 16:18:05 cgjung Exp $

package org.jboss.net.jmx.adaptor;

import org.apache.axis.encoding.DeserializerImpl;
import org.apache.axis.encoding.DeserializationContext;

import javax.xml.namespace.QName;

import org.xml.sax.SAXException;

import javax.management.ObjectName;

/**
 * Deserializer that turns string-based XML-elements back
 * into JMX objectnames.
 * @created 2. Oktober 2001, 14:09
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.2.4.1 $
 */

public class ObjectNameDeserializer extends DeserializerImpl {

   //
   // Attributes
   //

   StringBuffer val=new StringBuffer();
   protected QName xmlType;

   //
   // Constructors
   //

   public ObjectNameDeserializer(QName xmlType){
      this.xmlType = xmlType;
   }

    /**
     * Append any characters received to the value.  This method is defined 
     * by Deserializer.
     */
    public void characters(char [] chars, int start, int end)
        throws SAXException
    {
        val.append(chars, start, end);
    }

    /**
     * Append any characters to the value.  This method is defined by 
     * Deserializer.
     */
    public void onEndElement(String namespace, String localName,
                           DeserializationContext context)
        throws SAXException
    {
        if (isNil) {
            value = null;
            return;
        }
        
        try {
            value = new ObjectName(val.toString());
        } catch (Exception e) {
            throw new SAXException(e);
        }
        
    }

}