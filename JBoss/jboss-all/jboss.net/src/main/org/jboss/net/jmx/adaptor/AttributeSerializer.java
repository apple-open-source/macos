/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: AttributeSerializer.java,v 1.1.2.3 2003/06/27 15:01:03 acoliver2 Exp $

package org.jboss.net.jmx.adaptor;

import org.apache.axis.Constants;
import org.apache.axis.encoding.Serializer;
import org.apache.axis.encoding.SerializationContext;
import org.apache.axis.encoding.XMLType;
import org.apache.axis.wsdl.fromJava.Types;

import org.xml.sax.Attributes;
import org.w3c.dom.Element;
import org.xml.sax.helpers.AttributesImpl;

import javax.xml.namespace.QName;

import javax.management.Attribute;

import java.io.IOException;

/**
 * Serializer specialized to turn JMX-Attributes into
 * corresponding XML-types.
 * @since 09.04.02
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.3 $
 */

public class AttributeSerializer implements Serializer {

   /** this is the fully-qualified type that we serialize into */
   protected QName xmlType;

   // 
   // Constructors
   //

   public AttributeSerializer(QName xmlType) {
      this.xmlType = xmlType;
   }

   //
   // Public API
   //

   /** 
    *  turns a JMX objectname into a string-based xml element 
    *  @param name the name of the element that carries our type
    *  @param attributes the attributes of the element that carries our type
    *  @param value the objectname to serialize
    *  @param context the serialization context we live into
    */
   public void serialize(
      QName name,
      Attributes attributes,
      Object value,
      SerializationContext context)
      throws IOException {

      // do some initialisation of attributes
      AttributesImpl attrs;
      if (attributes != null)
         attrs = new AttributesImpl(attributes);
      else
         attrs = new AttributesImpl();

      // next we utter the attribute name as an attribute
      QName qname = new QName("", "name");
      attrs.addAttribute(
         qname.getNamespaceURI(),
         qname.getLocalPart(),
         context.qName2String(qname),
         "CDATA",
         ((Attribute) value).getName());

      // start the attribute tag
      context.startElement(name, attrs);

      // next we utter an embedded value object of any-type with the
      // attributes content
      qname = new QName("", "value");
      Object attrValue = ((Attribute) value).getValue();
      // lets hope that Object is mapped semantically to xsd:any??	
      context.serialize(qname, null, attrValue);
      // end the attribute tag
      context.endElement();
   }

   /** we use sax approach */
   public String getMechanismType() {
      return Constants.AXIS_SAX;
   }

   /**
    * Return XML schema for the specified type.
    * The Attribute type has a string-based name attribute and a
    */

   public Element writeSchema(Class clazz, Types types) throws Exception {
      // ComplexType representation of SimpleType bean class
      Element complexType = types.createElement("complexType");
      types.writeSchemaElement(xmlType, complexType);
      complexType.setAttribute("name", xmlType.getLocalPart());
      Element nameAttribute =
         types.createAttributeElement(
            "name",
	    String.class,
            XMLType.XSD_STRING,
            false,
            complexType.getOwnerDocument());
      complexType.appendChild(nameAttribute);
      Element complexContent = types.createElement("complexContent");
      complexType.appendChild(complexContent);
      Element all = types.createElement("sequence");
      complexContent.appendChild(all);
      Element valueElement =
         types.createElement(
            "value",
            types.getNamespaces().getCreatePrefix(
               XMLType.XSD_ANYTYPE.getNamespaceURI())
               + ":"
               + XMLType.XSD_ANYTYPE.getLocalPart(),
            true,
            false,
            all.getOwnerDocument());
      all.appendChild(valueElement);
      return complexType;
   }

}
