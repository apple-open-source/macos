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

import org.jboss.invocation.Invocation;
import org.jboss.invocation.InvocationType;
import org.jboss.ejb.Container;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.metadata.EntityMetaData;
import org.jboss.ejb.plugins.lock.Entrancy;

/**
 * The role of this interceptor is to check for reentrancy.
 * Per the spec, throw an exception if instance is not marked
 * as reentrant.
 *
 * <p><b>WARNING: critical code</b>, get approval from senior developers
 *    before changing.
 *
 * @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
 * @version $Revision: 1.1.4.3 $
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
         EntityMetaData meta = (EntityMetaData)container.getBeanMetaData();
         reentrant = meta.isReentrant();
      }
   }
 
   public Object invoke(Invocation mi)
      throws Exception
   {
      // We are going to work with the context a lot
      EntityEnterpriseContext ctx = (EntityEnterpriseContext)mi.getEnterpriseContext();
      if (isReentrantMethod(mi))
      {
         return getNext().invoke(mi);
      }

      // Not a reentrant method like getPrimaryKey

      synchronized(ctx)
      {
         if (!reentrant && ctx.isLocked())
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
         ctx.lock();
      }
      try
      {
         return getNext().invoke(mi); 
      }
      finally
      {
         synchronized (ctx)
         {
            ctx.unlock();
         }
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
         isIdentical = EJBObject.class.getMethod("isIdentical", new Class[] {EJBObject.class});
         remove = EJBObject.class.getMethod("remove", noArg);
      }
      catch (Exception e) {
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
      Entrancy entrancy = (Entrancy)mi.getValue(Entrancy.ENTRANCY_KEY);
      if(entrancy == Entrancy.NON_ENTRANT)
      {
         log.trace("NON_ENTRANT invocation");
         return true;
      }
  
      return false;
   }

}
