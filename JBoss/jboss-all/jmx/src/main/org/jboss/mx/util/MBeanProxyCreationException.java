/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.util;

import javax.management.JMException;

/**
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 *   
 */
public class MBeanProxyCreationException
   extends JMException
{
   // Constructors --------------------------------------------------
   public MBeanProxyCreationException() 
   {
      super();
   }
   
   public MBeanProxyCreationException(String msg)
   {
      super(msg);
   }
}
      



