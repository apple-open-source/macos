/**
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins;

import java.lang.reflect.Method;
import java.rmi.RemoteException;
import javax.ejb.EJBObject;
import javax.ejb.EJBException;
import javax.transaction.Transaction;
import javax.transaction.Status;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.InvocationType;
import org.jboss.ejb.Container;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.metadata.EntityMetaData;
import org.jboss.ejb.plugins.lock.Entrancy;
import org.jboss.ejb.plugins.lock.NonReentrantLock;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.CMRInvocation;

/**
 * The role of this interceptor is to check for reentrancy.
 * Per the spec, throw an exception if instance is not marked
 * as reentrant.  We do not check to see if same Tx is
 * accessing object at the same time as we assume that
 * any transactional locks will handle this.
 *
 * <p><b>WARNING: critical code</b>, get approval from senior developers
 *    before changing.
 *
 * @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
 * @version $Revision: 1.1.4.5 $
 */
public class EntityReentranceInterceptor
        extends AbstractInterceptor
{
   protected boolean reentrant = false;

   // Public --------------------------------------------------------

   public void setContainer(Container container)
   {
      super.setContainer(container);
      if (container != null)
      {
         EntityMetaData meta = (EntityMetaData) container.getBeanMetaData();
         reentrant = meta.isReentrant();
      }
   }

   protected boolean isTxExpired(Transaction miTx) throws Exception
   {
      if (miTx != null && miTx.getStatus() == Status.STATUS_MARKED_ROLLBACK)
      {
         return true;
      }
      return false;
   }

   public Object invoke(Invocation mi)
           throws Exception
   {
      // We are going to work with the context a lot
      EntityEnterpriseContext ctx = (EntityEnterpriseContext) mi.getEnterpriseContext();
      if (reentrant || isReentrantMethod(mi))
      {
         return getNext().invoke(mi);
      }

      // Not a reentrant method like getPrimaryKey
      NonReentrantLock methodLock = ctx.getMethodLock();
      Transaction miTx = ctx.getTransaction();
      boolean locked = false;
      try
      {
         while (!locked)
         {
            if (methodLock.attempt(5000, miTx))
            {
               locked = true;
            }
            else
            {
               if (isTxExpired(miTx))
               {
                  log.error("Saw rolled back tx=" + miTx);
                  throw new RuntimeException("Transaction marked for rollback, possibly a timeout");
               }
            }
         }
      }
      catch (NonReentrantLock.ReentranceException re)
      {
         if (mi.getType() == InvocationType.REMOTE)
         {
            throw new RemoteException("Reentrant method call detected: "
                    + container.getBeanMetaData().getEjbName() + " "
                    + ctx.getId().toString());
         }
         else
         {
            throw new EJBException("Reentrant method call detected: "
                    + container.getBeanMetaData().getEjbName() + " "
                    + ctx.getId().toString());
         }
      }
      try
      {
         ctx.lock();
         return getNext().invoke(mi);
      }
      finally
      {
         ctx.unlock();
         methodLock.release();
      }
   }

   // Private ------------------------------------------------------

   private static final Method getEJBHome;
   private static final Method getHandle;
   private static final Method getPrimaryKey;
   private static final Method isIdentical;
   private static final Method remove;

   static
   {
      try
      {
         Class[] noArg = new Class[0];
         getEJBHome = EJBObject.class.getMethod("getEJBHome", noArg);
         getHandle = EJBObject.class.getMethod("getHandle", noArg);
         getPrimaryKey = EJBObject.class.getMethod("getPrimaryKey", noArg);
         isIdentical = EJBObject.class.getMethod("isIdentical", new Class[]{EJBObject.class});
         remove = EJBObject.class.getMethod("remove", noArg);
      }
      catch (Exception e)
      {
         e.printStackTrace();
         throw new ExceptionInInitializerError(e);
      }
   }

   protected boolean isReentrantMethod(Invocation mi)
   {
      // is this a known non-entrant method
      Method m = mi.getMethod();
      if (m != null && (
              m.equals(getEJBHome) ||
              m.equals(getHandle) ||
              m.equals(getPrimaryKey) ||
              m.equals(isIdentical) ||
              m.equals(remove)))
      {
         return true;
      }

      // if this is a non-entrant message to the container let it through
      if (mi instanceof CMRInvocation)
      {
         Entrancy entrancy = ((CMRInvocation) mi).getEntrancy();
         if (entrancy == Entrancy.NON_ENTRANT)
         {
            log.trace("NON_ENTRANT invocation");
            return true;
         }
      }

      return false;
   }

}
