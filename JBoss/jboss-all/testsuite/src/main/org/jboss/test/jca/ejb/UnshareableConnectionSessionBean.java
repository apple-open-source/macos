/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.jca.ejb;

import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.InitialContext;

import org.jboss.test.jca.interfaces.UnshareableConnectionStatefulLocal;
import org.jboss.test.jca.interfaces.UnshareableConnectionStatefulLocalHome;

/**
 * A stateless session bean that invokes a stateful session with an unshareable resource
 *
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 */
public class UnshareableConnectionSessionBean
   implements SessionBean
{
   private SessionContext ctx;

   public void runTest()
   {
      try
      {
         UnshareableConnectionStatefulLocalHome home = (UnshareableConnectionStatefulLocalHome) new InitialContext().lookup("local/UnshareableStateful");
         UnshareableConnectionStatefulLocal stateful = home.create();
         stateful.runTestPart1();
         stateful.runTestPart2();
         stateful.remove();
      }
      catch (Exception e)
      {
         throw new EJBException(e.toString());
      }
   }

   public void ejbCreate()
      throws CreateException
   {
   }

   public void ejbActivate()
   {
    }

   public void ejbPassivate()
   {
   }

   public void ejbRemove()
   {
   }

   public void setSessionContext(SessionContext ctx)
   {
      this.ctx = ctx;
   }

   public void unsetSessionContext()
   {
   }
}

