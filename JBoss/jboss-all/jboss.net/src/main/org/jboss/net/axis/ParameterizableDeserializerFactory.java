/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: ParameterizableDeserializerFactory.java,v 1.1.4.1 2002/09/12 16:18:03 cgjung Exp $

package org.jboss.net.axis;

import org.apache.axis.encoding.ser.BaseDeserializerFactory;

import javax.xml.rpc.encoding.DeserializerFactory;
import javax.xml.namespace.QName;

import java.util.Map;

/**
 * Deserializer Factory that may be parameterized with additional
 * options.
 * <br>
 * <h3>Change History</h3>
 * <ul>
 * </ul>
 * @created 06.04.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.4.1 $
 */

public class ParameterizableDeserializerFactory extends BaseDeserializerFactory {

	// 
	// Attributes
	// 
	
	protected Map options;
	
	//
	// Constructors
	//
		
	/** regular constructor */
	public ParameterizableDeserializerFactory(Class deserializerType, Class javaType, QName xmlType) {
	   super(deserializerType, xmlType,javaType);
	}
	
	/** the extended constructor that is parameterized */
	public ParameterizableDeserializerFactory(Class deserializerType, Class javaType, QName xmlType, Map options) {
	   super(deserializerType,xmlType,javaType);
	   this.options=options;
	}

	//
	// public API
	//

	/** return options */
	protected Map getOptions() {
	   return options;
	}
	
	/** set options */
	protected void setOptions(Map options) {
	   this.options=options;
	}
		
	/** return a new deserializer that we equip with the right options */
    public javax.xml.rpc.encoding.Deserializer getDeserializerAs(String mechanismType) {
       javax.xml.rpc.encoding.Deserializer deser=super.getDeserializerAs(mechanismType);
       if(deser instanceof ParameterizableDeserializer) {
          ((ParameterizableDeserializer) deser).setOptions(options);
       }
       return deser;
    }
	   
}