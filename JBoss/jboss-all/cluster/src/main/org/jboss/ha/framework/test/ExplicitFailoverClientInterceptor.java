/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.test;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.PayloadKey;

/**
 * Used for testing clustering: allows to explicitely makes a call to node fail
 * This will mimic a dead server.
 *
 * @see org.jboss.ha.framework.test.ExplicitFailoverServerInterceptor
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.2.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>8 avril 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class ExplicitFailoverClientInterceptor extends org.jboss.proxy.Interceptor
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public ExplicitFailoverClientInterceptor ()
   {
   }
   
   // Public --------------------------------------------------------
   
   // Z implementation ----------------------------------------------
   
   // Interceptor overrides ---------------------------------------------------
   
   public Object invoke (Invocation mi) throws Throwable
   {
      Object failover = System.getProperty ("JBossCluster-DoFail");
      boolean doFail = false;
      
      if (failover != null && 
          failover instanceof java.lang.String)
      {
         String strFailover = (java.lang.String)failover;
         if (strFailover.equalsIgnoreCase ("true"))
         {
            doFail = true;
         }
         else if (strFailover.equalsIgnoreCase ("once"))
         {
            doFail = true;
            System.setProperty ("JBossCluster-DoFail", "false");
         }          
      }
      
      if (doFail)
      {
         mi.setValue ("DO_FAIL_DURING_NEXT_CALL", Boolean.TRUE, PayloadKey.AS_IS);
         System.out.println("SYSTEM : We fail during next call!!!");
      }
      else
         mi.setValue ("DO_FAIL_DURING_NEXT_CALL", Boolean.FALSE, PayloadKey.AS_IS);
         

      return getNext().invoke(mi);
   }
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
