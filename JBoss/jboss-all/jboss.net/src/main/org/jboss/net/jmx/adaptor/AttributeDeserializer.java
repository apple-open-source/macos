/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: AttributeDeserializer.java,v 1.1.2.1 2002/09/12 16:18:05 cgjung Exp $

package org.jboss.net.jmx.adaptor;

import org.apache.axis.encoding.DeserializerImpl;
import org.apache.axis.encoding.DeserializationContext;
import org.apache.axis.encoding.Target;
import org.apache.axis.message.SOAPHandler;
import org.apache.axis.encoding.Deserializer;
import org.apache.axis.encoding.TypeMapping;

import javax.xml.namespace.QName;

import org.xml.sax.SAXException;
import org.xml.sax.Attributes;


import javax.management.Attribute;

/**
 * Deserializer that turns well-formed XML-elements back
 * into JMX Attributes.
 * <br>
 * <ul>
 * </ul>
 * @created 19.04.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 */

public class AttributeDeserializer extends DeserializerImpl {

	//
	// Attributes
	//

	protected String attributeName;
	protected Object attributeValue;
	protected QName xmlType;

	//
	// Constructors
	//

	public AttributeDeserializer(QName xmlType) {
		this.xmlType = xmlType;
	}

	/** we can already defer the attribute name */
	public void onStartElement(
		String namespace,
		String localName,
		String qName,
		Attributes attributes,
		DeserializationContext context)
		throws SAXException {
		attributeName = attributes.getValue("", "name");
	}

	/** dispatch to the deserializer for the value element */
	public SOAPHandler onStartChild(
		String namespace,
		String localName,
		String prefix,
		Attributes attributes,
		DeserializationContext context)
		throws SAXException {
		if (localName.equals("value") && namespace.equals("")) {
			QName qn = context.getTypeFromAttributes(namespace, localName, attributes);
			// get the deserializer
			Deserializer dSer = context.getDeserializerForType(qn);
			// If no deserializer, use the base DeserializerImpl.
			// There may not be enough information yet to choose the
			// specific deserializer.
			if (dSer == null) {
				dSer = new DeserializerImpl();
				// determine a default type for this child element
				TypeMapping tm = context.getTypeMapping();
				dSer.setDefaultType(tm.getTypeQName(Object.class));
				dSer.registerValueTarget(new AttributeValueTarget());
			}
			return (SOAPHandler) dSer;
		} else {
			return null;
		}
	}

	/**
	 * Append any characters to the value.  This method is defined by 
	 * Deserializer.
	 */
	public void onEndElement(
		String namespace,
		String localName,
		DeserializationContext context)
		throws SAXException {
		if (isNil) {
			value = null;
			return;
		}

		value = new Attribute(attributeName, attributeValue);
	}

	//
	// Inner classes
	//

	protected class AttributeValueTarget implements Target {
		public void set(Object value) throws SAXException {
			attributeValue = value;
		}
	}

}