/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

import java.security.Principal;

import java.rmi.RemoteException;

import javax.ejb.EJBContext;
import javax.ejb.EJBHome;
import javax.ejb.EJBLocalHome;
import javax.ejb.EJBObject;
import javax.ejb.MessageDrivenContext;
import javax.ejb.MessageDrivenBean;
import javax.ejb.SessionContext;
import javax.ejb.EJBException;

import org.jboss.metadata.MetaData;
import org.jboss.metadata.MessageDrivenMetaData;

/**
 * Context for message driven beans.
 * 
 * @version <tt>$Revision: 1.16 $</tt>
 * @author <a href="mailto:peter.antman@tim.se">Peter Antman</a>.
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class MessageDrivenEnterpriseContext
   extends EnterpriseContext
{
   private MessageDrivenContext ctx;

   /**
    * Construct a <tt>MessageDrivenEnterpriseContext</tt>.
    *
    * <p>Sets the MDB context and calls ejbCreate().
    *
    * @param instance   An instance of MessageDrivenBean
    * @param con        The container for this MDB.
    *
    * @throws Exception    EJBException, Error or Exception.  If RuntimeException
    *                      was thrown by ejbCreate it will be turned into an
    *                      EJBException.
    */
   public MessageDrivenEnterpriseContext(Object instance, Container con)
      throws Exception
   {
      super(instance, con);
      
      ctx = new MessageDrivenContextImpl();
      ((MessageDrivenBean)instance).setMessageDrivenContext(ctx);

      try
      {
         Method ejbCreate = instance.getClass().getMethod("ejbCreate", new Class[0]);
         ejbCreate.invoke(instance, new Object[0]);
      }
      catch (InvocationTargetException e)
      {
         Throwable t = e.getTargetException();
         
         if (t instanceof RuntimeException) {
            if (t instanceof EJBException) {
               throw (EJBException)t;
            }
            else {
               // Transform runtime exception into what a bean *should* have thrown
               throw new EJBException((RuntimeException)t);
            }
         }
         else if (t instanceof Exception) {
            throw (Exception)t;
         }
         else if (t instanceof Error) {
            throw (Error)t;
         }
         else {
            throw new org.jboss.util.NestedError("Unexpected Throwable", t);
         }
      }
   }

   public MessageDrivenContext getMessageDrivenContext()
   {
      return ctx;
   }

   // EnterpriseContext overrides -----------------------------------

   /**
    * Calls ejbRemove() on the MDB instance.
    */
   public void discard() throws RemoteException
   {
      ((MessageDrivenBean)instance).ejbRemove();
   }

   public EJBContext getEJBContext()
   {
      return ctx;
   }

   /**
    * The EJBContext for MDBs.
    */
   protected class MessageDrivenContextImpl
      extends EJBContextImpl
      implements MessageDrivenContext
   {
      /**
       * Not allowed for MDB.
       *
       * @throws IllegalStateException  Always
       */
      public EJBHome getEJBHome()
      {
         throw new IllegalStateException
            ("MDB must not call getEJBHome (EJB 2.0 15.4.3)");
      }

      /**
       * Not allowed for MDB.
       *
       * @throws IllegalStateException  Always
       */
      public EJBLocalHome getEJBLocalHome()
      {
         throw new IllegalStateException
            ("MDB must not call getEJBLocalHome (EJB 2.0 15.4.3)");
      }

      /**
       * Not allowed for MDB.
       *
       * @throws IllegalStateException  Always
       */
      public boolean isCallerInRole(String id)
      {
         throw new IllegalStateException
            ("MDB must not call isCallerInRole (EJB 2.0 15.4.3)");
      }

      /**
       * Not allowed for MDB.
       *
       * @throws IllegalStateException  Always
       */
      public Principal getCallerPrincipal()
      {
         throw new IllegalStateException
            ("MDB must not call getCallerPrincipal (EJB 2.0 15.4.3)");
      }

      /** Helper to check if the tx type is TX_REQUIRED. */
      private boolean isTxRequired()
      {
         MessageDrivenMetaData md = (MessageDrivenMetaData)con.getBeanMetaData();
         return md.getMethodTransactionType() == MetaData.TX_REQUIRED;
      }
      
      /**
       * If transaction type is not Container or there is no transaction
       * then throw an exception.
       *
       * @throws IllegalStateException   If transaction type is not Container,
       *                                 or no transaction.
       */
      public boolean getRollbackOnly()
      {
         if (!isContainerManagedTx()) {
            throw new IllegalStateException
               ("Bean managed MDB are not allowed getRollbackOnly (EJB 2.0 - 15.4.3)");
         }

         //
         // jason: I think this is lame... but the spec says this is how it is.
         //        I think it would be better to silently ignore... or not so silently
         //        but still continue.
         //
         
         if (!isTxRequired()) {
            throw new IllegalStateException
               ("getRollbackOnly must only be called in the context of a transaction (EJB 2.0 - 15.5.1)");
         }

         return super.getRollbackOnly();
      }

      /**
       * If transaction type is not Container or there is no transaction
       * then throw an exception.
       *
       * @throws IllegalStateException   If transaction type is not Container,
       *                                 or no transaction.
       */
      public void setRollbackOnly()
      {
         if (!isContainerManagedTx()) {
            throw new IllegalStateException
               ("Bean managed MDB are not allowed setRollbackOnly (EJB 2.0 - 15.4.3)");
         }

         if (!isTxRequired()) {
            throw new IllegalStateException
               ("setRollbackOnly must only be called in the context of a transaction (EJB 2.0 - 15.5.1)");
         }

         super.setRollbackOnly();
      }
   }
}
