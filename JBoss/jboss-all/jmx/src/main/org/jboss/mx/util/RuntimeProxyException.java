/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.util;

import javax.management.JMRuntimeException;

/**
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $  
 */
public class RuntimeProxyException
   extends JMRuntimeException
{
   // Constructors --------------------------------------------------
   public RuntimeProxyException() 
   {
      super();
   }
   
   public RuntimeProxyException(String msg)
   {
      super(msg);
   }
}
      



