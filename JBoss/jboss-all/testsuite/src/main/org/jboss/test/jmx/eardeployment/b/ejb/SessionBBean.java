
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jmx.eardeployment.b.ejb; // Generated package name



import java.rmi.RemoteException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.InitialContext;
import org.jboss.logging.Logger;
import org.jboss.test.jmx.eardeployment.a.interfaces.SessionA;
import org.jboss.test.jmx.eardeployment.a.interfaces.SessionAHome;

/**
 * SessionBBean.java
 *
 *
 * Created: Thu Feb 21 14:50:22 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 *
 *
 * @ejb:bean   name="SessionB"
 *             jndi-name="eardeployment/SessionB"
 *             local-jndi-name="eardeployment/LocalSessionB"
 *             view-type="both"
 *             type="Stateless"
 *
 */

public class SessionBBean implements SessionBean  {

   /**
    * Describe <code>callA</code> method here.
    *
    * @ejb:interface-method
    */
   public boolean callA()
   {
      try
      {
         SessionAHome ahome = (SessionAHome)new InitialContext().lookup("eardeployment/SessionA");
         SessionA a = ahome.create();
         a.doNothing();
         return true;
      }
      catch (Exception e)
      {
         Logger.getLogger(getClass()).error("error in callA", e);
         return false;  
      }
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


   public void ejbActivate() throws RemoteException
   {
   }

   public void ejbPassivate() throws RemoteException
   {
   }

   public void ejbRemove() throws RemoteException
   {
   }

   public void setSessionContext(SessionContext ctx) throws RemoteException
   {
   }

   public void unsetSessionContext() throws RemoteException
   {
   }

}

