/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.test;

import org.jboss.invocation.Invocation;
import org.jboss.ha.framework.interfaces.GenericClusteringException;

/**
 * Used for testing clustering: allows to explicitely makes a call to node fail
 * This will mimic a dead server.
 *
 * @see org.jboss.ha.framework.test.ExplicitFailoverClientInterceptor
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.4.2 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>8 avril 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class ExplicitFailoverServerInterceptor extends org.jboss.ejb.plugins.AbstractInterceptor
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   protected org.jboss.ejb.Container container;

   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public ExplicitFailoverServerInterceptor ()
   {
   }
   
   // Public --------------------------------------------------------
   
   public void setContainer(org.jboss.ejb.Container container)
   {
      this.container = container;
   }
	
   public org.jboss.ejb.Container getContainer()
   {
      return container;
   }
	
   // Z implementation ----------------------------------------------
   
   // AbstractInterceptor overrides ---------------------------------------------------
   
   public Object invokeHome(Invocation mi)
      throws Exception
   {
      checkFailoverNeed (mi);
      
     return super.invokeHome (mi);
   }
   
   public Object invoke(Invocation mi)
      throws Exception
   {
      checkFailoverNeed (mi);
      
      return super.invoke (mi);
   }
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   protected void checkFailoverNeed (Invocation mi) 
      throws GenericClusteringException 
   {
      Object data = mi.getValue ("DO_FAIL_DURING_NEXT_CALL");
      
      if (data != null &&
          data instanceof java.lang.Boolean &&
          data.equals (java.lang.Boolean.TRUE))
      {
         // we now determine if we have already failed
         //
         Object alreadyDone = mi.getValue ("FAILOVER_COUNTER");
         
         if (alreadyDone != null &&
             alreadyDone instanceof java.lang.Integer &&
             ((java.lang.Integer)alreadyDone).intValue () == 0)
         {
            // we do fail
            //
            this.log.debug ("WE FAILOVER IN SERVER INTERCEPTOR (explicit failover asked by client interceptor)!");
            throw new GenericClusteringException 
               (GenericClusteringException.COMPLETED_NO, "Test failover from server interceptor", false);
         }
      }
   }
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
