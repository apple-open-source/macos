/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb;

import java.io.IOException;
import java.io.Serializable;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;

import java.rmi.RemoteException;

import javax.ejb.EJBContext;
import javax.ejb.EJBLocalObject;
import javax.ejb.EJBObject;
import javax.ejb.EJBLocalObject;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;


/**
 * The enterprise context for stateful session beans.
 *
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:docodan@mvcsoft.com">Daniel OConnor</a>
 * @version $Revision: 1.21 $
 */
public class StatefulSessionEnterpriseContext
   extends EnterpriseContext
   implements Serializable
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------
   private EJBObject ejbObject;
   private EJBLocalObject ejbLocalObject;
   private SessionContext ctx;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   public StatefulSessionEnterpriseContext(Object instance, Container con)
      throws RemoteException
   {
      super(instance, con);
      ctx = new StatefulSessionContextImpl();
      ((SessionBean)instance).setSessionContext(ctx);
   }

   // Public --------------------------------------------------------

   public void discard() throws RemoteException
   {
      // Do nothing
   }

   public EJBContext getEJBContext()
   {
      return ctx;
   }

   /**
    * During activation of stateful session beans we replace the instance
    * by the one read from the file.
    */
   public void setInstance(Object instance)
   {
      this.instance = instance;
      try
      {
         ((SessionBean)instance).setSessionContext(ctx);
      }
      catch (Exception x)
      {
         log.error("Failed to setSessionContext", x);
      }
   }

   public void setEJBObject(EJBObject eo) {
      ejbObject = eo;
   }

   public EJBObject getEJBObject() {
      return ejbObject;
   }

   public void setEJBLocalObject(EJBLocalObject eo) {
      ejbLocalObject = eo;
   }

   public EJBLocalObject getEJBLocalObject() {
      return ejbLocalObject;
   }
    
   public SessionContext getSessionContext()
   {
      return ctx;
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   private void writeObject(ObjectOutputStream out)
      throws IOException, ClassNotFoundException
   {
      // No state
   }
    
   private void readObject(ObjectInputStream in)
      throws IOException, ClassNotFoundException
   {
      // No state
   }

   // Inner classes -------------------------------------------------

   protected class StatefulSessionContextImpl
      extends EJBContextImpl
      implements SessionContext
   {
      public EJBObject getEJBObject()
      {
         if (((StatefulSessionContainer)con).getProxyFactory()==null)
            throw new IllegalStateException( "No remote interface defined." );

         if (ejbObject == null) {
               ejbObject = (EJBObject) ((StatefulSessionContainer)con).getProxyFactory().getStatefulSessionEJBObject(id);
         }  

         return ejbObject;
      }

      public EJBLocalObject getEJBLocalObject()
      {
         if (con.getLocalHomeClass()==null)
            throw new IllegalStateException( "No local interface for bean." );
         if (ejbLocalObject == null)
         {
            ejbLocalObject = ((StatefulSessionContainer)con).getLocalProxyFactory().getStatefulSessionEJBLocalObject(id);
         }
         return ejbLocalObject;
      }

      public Object getPrimaryKey()
      {
         return id;
      }
   }
}
