/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
import java.rmi.RemoteException;
import javax.ejb.EJBContext;
import javax.ejb.EJBLocalObject;
import javax.ejb.EJBObject;
import javax.ejb.SessionContext;
import javax.ejb.SessionBean;
import javax.ejb.EJBException;

/**
 * The enterprise context for stateless session beans.
 *      
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @version $Revision: 1.17 $
 */
public class StatelessSessionEnterpriseContext
   extends EnterpriseContext
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   
   EJBObject ejbObject;
   EJBLocalObject ejbLocalObject;
   SessionContext ctx;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public StatelessSessionEnterpriseContext(Object instance, Container con)
      throws Exception
   {
      super(instance, con);
      ctx = new SessionContextImpl();
      
      ((SessionBean)instance).setSessionContext(ctx);
      
      try
      {
         Method ejbCreate = instance.getClass().getMethod("ejbCreate", new Class[0]);
         ejbCreate.invoke(instance, new Object[0]);
      } catch (InvocationTargetException e) 
      {
         Throwable ex = e.getTargetException();
         if (ex instanceof EJBException)
            throw (Exception)ex;
         else if (ex instanceof RuntimeException)
            throw new EJBException((Exception)ex); // Transform runtime exception into what a bean *should* have thrown
         else if (ex instanceof Exception)
            throw (Exception)ex;
         else
            throw (Error)ex;
      }
   }
   
   // Public --------------------------------------------------------
   
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
   
   public SessionContext getSessionContext() {
      return ctx;
   }

   // EnterpriseContext overrides -----------------------------------
   
   public void discard() throws RemoteException
   {
      ((SessionBean)instance).ejbRemove();
   }
   
   public EJBContext getEJBContext()
   {
      return ctx;
   }
   
   // Package protected ---------------------------------------------
    
   // Protected -----------------------------------------------------
    
   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
   
   protected class SessionContextImpl
      extends EJBContextImpl
      implements SessionContext
   {
      public EJBObject getEJBObject()
      {
         if (((StatelessSessionContainer)con).getProxyFactory()==null)
            throw new IllegalStateException( "No remote interface defined." );
         
         if (ejbObject == null) {
               ejbObject = (EJBObject) ((StatelessSessionContainer)con).getProxyFactory().getStatelessSessionEJBObject(); 
            
         } 	
    
         return ejbObject;
      }

      public EJBLocalObject getEJBLocalObject()
      {
         if (con.getLocalHomeClass()==null)
            throw new IllegalStateException( "No local interface for bean." );
         if (ejbLocalObject == null) {
            ejbLocalObject = ((StatelessSessionContainer)con).getLocalProxyFactory().getStatelessSessionEJBLocalObject(); 
         }
         return ejbLocalObject;
      }
   }
}

