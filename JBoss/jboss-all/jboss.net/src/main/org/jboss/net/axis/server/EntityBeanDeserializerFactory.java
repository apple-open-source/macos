/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: EntityBeanDeserializerFactory.java,v 1.2.4.1 2002/09/12 16:18:04 cgjung Exp $

package org.jboss.net.axis.server;

import org.jboss.net.axis.ParameterizableDeserializerFactory;

import javax.xml.namespace.QName;
import java.util.Hashtable;

/**
 * Factory for server-side Entity Bean Deserialization. 
 * @created 21.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.2.4.1 $
 */

public class EntityBeanDeserializerFactory extends ParameterizableDeserializerFactory {
	
	//
	// Constructors
	//
	
	/** the usual constructor used by axis */
	public EntityBeanDeserializerFactory(Class javaType, QName xmlType) {
	   this(javaType,xmlType,new Hashtable(0));
	}
	
	/** the extended constructor that is parameterized */
	public EntityBeanDeserializerFactory(Class javaType, QName xmlType, Hashtable options) {
	   super(EntityBeanDeserializer.class,javaType,xmlType,options);
	}
		   
}