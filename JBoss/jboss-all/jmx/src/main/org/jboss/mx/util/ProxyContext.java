/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.util;

import javax.management.MBeanServer;
import javax.management.ObjectName;

/**
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 *   
 */
public interface ProxyContext
{
   void setExceptionHandler(ProxyExceptionHandler handler);
   
   MBeanServer getMBeanServer();
   
   ObjectName getObjectName();
}
      



