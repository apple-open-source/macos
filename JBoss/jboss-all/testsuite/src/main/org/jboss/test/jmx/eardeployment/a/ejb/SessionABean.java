
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jmx.eardeployment.a.ejb; // Generated package name



import java.rmi.RemoteException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.InitialContext;
import org.jboss.logging.Logger;
import org.jboss.test.jmx.eardeployment.b.interfaces.SessionB;
import org.jboss.test.jmx.eardeployment.b.interfaces.SessionBHome;

/**
 * SessionABean.java
 *
 *
 * Created: Thu Feb 21 14:48:18 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 *
 *
 * @ejb:bean   name="SessionA"
 *             jndi-name="eardeployment/SessionA"
 *             local-jndi-name="eardeployment/LocalSessionA"
 *             view-type="both"
 *             type="Stateless"
 *
 */

public class SessionABean implements SessionBean  {

   /**
    * Describe <code>callB</code> method here.
    *
    * @exception RemoteException if an error occurs
    * @ejb:interface-method
    */
   public boolean callB()
   {
      try 
      {
         
         SessionBHome bhome = (SessionBHome)new InitialContext().lookup("eardeployment/SessionB");
         SessionB b = bhome.create();
         b.doNothing();
         return true;
      }
      catch (Exception e)
      {
         Logger.getLogger(getClass()).error("error in callB", e);
         return false;  
      } // end of try-catch
      
   }

   /**
    * Describe <code>doNothing</code> method here.
    *
    * @ejb:interface-method
    */
   public void doNothing()
   {
   }

   /**
    * Describe <code>ejbCreate</code> method here.
    *
    * @ejb:create-method
    */
   public void ejbCreate() 
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
   }

   public void unsetSessionContext()
   {
   }

}

