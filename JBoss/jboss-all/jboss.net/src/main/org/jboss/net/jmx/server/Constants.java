/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/

// $Id: Constants.java,v 1.3.4.1 2003/03/28 12:50:47 cgjung Exp $

package org.jboss.net.jmx.server;

import org.apache.log4j.Category;

/**
 * Some Constants for the jmx server package  
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @since 1. October 2001
 * @version $Revision: 1.3.4.1 $
 */

public interface Constants extends org.jboss.net.axis.server.Constants {

   static final Category LOG =
      Category.getInstance(Constants.class.getPackage().getName());
   static final String MBEAN_SERVER_ID_PROPERTY = "MBeanServerId";
   static final String OBJECTNAME_PROPERTY = "ObjectName";
   static final String WRONG_OBJECT_NAME =
      "ObjectName could not be converted to a javax.management.ObjectName.";
   static final String NO_MBEAN_SERVER_FOUND =
      "Could not find the associated MBeanServer.";
   static final String COULD_NOT_CONVERT_PARAMS =
      "Could not convert the parameters to corresponding Java types.";
   static final String CLASS_NOT_FOUND = "Could not find Java class.";
   static final String NO_MBEAN_INSTANCE = "Could not find MBean instance.";
   static final String NO_SUCH_ATTRIBUTE = "Could not find MBean attribute.";
   static final String INVALID_ARGUMENT = "Invalid Argument.";
   static final String MBEAN_EXCEPTION = "Problems while interfacing JMX.";
   static final String BEAN_INFO_IS_NULL =
      "MBeanInfo is null, could not aquire MBean Meta Data!";
   static final String EXCEPTION_OCCURED =
      "Exception occurred in the target MBean method.";
   static final String COULDNT_GEN_WSDL = "Could not generate WSDL document";
   static final String INTROSPECTION_EXCEPTION = "Could not introspect mbean.";
}