/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: AttributeSerializerFactory.java,v 1.1.2.1 2002/09/12 16:18:05 cgjung Exp $

package org.jboss.net.jmx.adaptor;

import java.util.Iterator;

import javax.xml.rpc.JAXRPCException;
import javax.xml.rpc.encoding.Serializer;
import org.apache.axis.encoding.SerializerFactory;

import javax.xml.namespace.QName;

import javax.management.Attribute;

import java.util.Set;

/**
 * Factory for Attribute Serialization.
 * @created 19.04.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 */

public class AttributeSerializerFactory implements SerializerFactory {

	//
	// Attributes
	//
	
	final protected Set mechanisms=new java.util.HashSet(1);
	final protected Class javaType;
	final protected QName xmlType;

	//
	// Constructors
	//
	
	public AttributeSerializerFactory(Class javaType, QName xmlType) throws Exception {
	   if(!(Attribute.class.isAssignableFrom(javaType))) {
	      throw new Exception("Could only build serializers for JMX Attributes.");
	   }
	   mechanisms.add(org.apache.axis.Constants.AXIS_SAX);
	   this.javaType=javaType;
	   this.xmlType=xmlType;
	}
	
	//
	// Public API
	//
	
	public Serializer getSerializerAs(String mechanismType)
        throws JAXRPCException {
		if(!mechanisms.contains(mechanismType)) {
			throw new JAXRPCException("Unsupported mechanism "+mechanismType);
        }
       return new AttributeSerializer(xmlType);    
    }
        

    /**
     * Returns a list of all XML processing mechanism types supported by this DeserializerFactory.
     *
     * @return List of unique identifiers for the supported XML processing mechanism types
     */
    public java.util.Iterator getSupportedMechanismTypes() {
		return mechanisms.iterator();  		
    }

}
