/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.security;

import java.lang.reflect.Method;
import java.util.HashMap;
import javax.ejb.EJBContext;

/**
 * An abstract implementation of SecurityProxy that wraps a non-SecurityProxy
 * object. Subclasses of this class are used to create a SecurityProxy given
 * a security delegate that implements methods in the EJB home or remote
 * interface for security checks. This allows custom security classes to be
 * written without using a JBoss specific interface. It also allows the security
 * delegate to follow a natural proxy pattern implementation.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.4.4.2 $
 */
public abstract class AbstractSecurityProxy implements SecurityProxy
{
   /** The HashMap<Method, Method> from the EJB interface methods to the
    * corresponding delegate method
    */
   private HashMap methodMap;
   /** The optional setContext delegate method */
   private Method setContextMethod;
   /** The optional setContext delegate method */
   private Method setBeanMethod;
   /** The optional setContext delegate method */
   protected Object delegate;
   /** Flag which sets whether the method mapping will be performed in a strict
    * fashion. The proxy delegate must provide an implementation of all methods.
    * If set to 'true', a security exception will be thrown during
    * initialisation if a method is found for which the delegate doesn't have
    * a matching method. This defaults to false and is obtained via reflection
    * on the proxy delegate's 'boolean isStrict()' method.
    */
   protected boolean strict = false;

   AbstractSecurityProxy(Object delegate)
   {
      this.delegate = delegate;
      methodMap = new HashMap();
   }

   /**
    * Subclasses implement this method to actually invoke the given home
    * method on the proxy delegate.
    *
    * @param m, the delegate method that was mapped from the ejb home method.
    * @param args, the method invocation arguments.
    * @param delegate, the proxy delegate object associated with the
    *    AbstractSecurityProxy
    * 
    * @see invokeHome(Method, Object[])
    */
   protected abstract void invokeHomeOnDelegate(Method m, Object[] args,
      Object delegate) throws Exception;

   /**
    * Subclasses implement this method to actually invoke the given remote
    * method on the proxy delegate.
    *
    * @param m, the delegate method that was mapped from the ejb remote method.
    * @param args, the method invocation arguments.
    * @param delegate, the proxy delegate object associated with the AbstractSecurityProxy
    * 
    * @see invoke(Method, Object[], Object)
    */
   protected abstract void invokeOnDelegate(Method m, Object[] args, Object delegate)
      throws Exception;

   /**
    *
    * This version invokes init(beanHome, beanRemote, null, null, securityMgr)
    *
    * @see #init(Class, Class, Class, Class, Object)
    * @param beanHome, the class for the EJB home interface
    * @param beanRemote, the class for the EJB remote interface
    * @param securityMgr, The security manager instance assigned to the container.
    * It is not used by this class.
    */
   public void init(Class beanHome, Class beanRemote, Object securityMgr)
      throws InstantiationException
   {
      init(beanHome, beanRemote, null, null, securityMgr);
   }

   /** This method is called by the container SecurityInterceptor to intialize
    * the proxy with the EJB home and remote interface classes that the
    * container is housing. This method creates a mapping from the home and
    * remote classes to the proxy delegate instance. The mapping is based on
    * method name and paramter types. In addition, the proxy delegate is
    * inspected for a setEJBContext(EJBContext) and a setBean(Object) method
    * so that the active EJBContext and EJB instance can be passed to the
    * delegate prior to method invocations.
    *
    * @param beanHome The EJB remote home interface class
    * @param beanRemote The EJB remote interface class
    * @param beanLocalHome The EJB local home interface class
    * @param beanLocal The EJB local interface class
    * @param securityMgr The security manager from the security domain
    * @throws InstantiationException
    */
   public void init(Class beanHome, Class beanRemote,
      Class beanLocalHome, Class beanLocal, Object securityMgr)
      throws InstantiationException
   {
      // Get any methods from the bean home interface
      mapHomeMethods(beanHome);
      // Get any methods from the bean local home interface
      mapHomeMethods(beanLocalHome);
      // Get any methods from the bean remote interface
      mapRemoteMethods(beanRemote);
      // Get any methods from the bean local interface
      mapRemoteMethods(beanLocal);
      // Get the setEJBContext(EJBContext) method
      try
      {
         Class[] parameterTypes = {EJBContext.class};
         setContextMethod = delegate.getClass().getMethod("setEJBContext", parameterTypes);
      }
      catch(Exception ignore)
      {
      }

      // Get the setBean(Object) method
      try
      {
         Class[] parameterTypes = {Object.class};
         setBeanMethod = delegate.getClass().getMethod("setBean", parameterTypes);
      }
      catch(Exception ignore)
      {
      }

      // Check for a boolean isStrict() strict flag accessor
      try
      {
         Class[] parameterTypes = {};
         Object[] args = {};
         Method isStrict = delegate.getClass().getMethod("isStrict", parameterTypes);
         Boolean flag = (Boolean) isStrict.invoke(delegate, args);
         strict = flag.booleanValue();
      }
      catch(Exception ignore)
      {
      }
   }

   /** Called by the SecurityProxyInterceptor prior to a method invocation
    * to set the context for the call.
    *
    * @param ctx the bean's EJBContext
    */
   public void setEJBContext(EJBContext ctx)
   {
      if(setContextMethod != null)
      {
         Object[] args = {ctx};
         try
         {
            setContextMethod.invoke(delegate, args);
         }
         catch(Exception e)
         {
            e.printStackTrace();
         }
      }
   }

   /** Called by the SecurityProxyInterceptor to allow the proxy delegate to
    * perform a security check of the indicated home interface method.
    *
    * @param m, the EJB home interface method
    * @param args, the method arguments
    */
   public void invokeHome(final Method m, Object[] args)
      throws Exception
   {
      Method delegateMethod = (Method)methodMap.get(m);
      if( delegateMethod != null )
         invokeHomeOnDelegate(delegateMethod, args, delegate);
   }

   /**
    * Called by the SecurityProxyInterceptor to allow the proxy delegate to perform
    * a security check of the indicated remote interface method.
    * @param m, the EJB remote interface method
    * @param args, the method arguments
    * @param bean, the EJB bean instance
    */
   public void invoke(final Method m, final Object[] args, final Object bean)
      throws Exception
   {
      Method delegateMethod = (Method)methodMap.get(m);
      if( delegateMethod != null )
      {
         if( setBeanMethod != null )
         {
            Object[] bargs = {bean};
            try
            {
               setBeanMethod.invoke(delegate, bargs);
            }
            catch(Exception e)
            {
               e.printStackTrace();
               throw new SecurityException("Failed to set bean on proxy" + e.getMessage());
            }
         }
         invokeOnDelegate(delegateMethod, args, delegate);
      }
   }

   /** Performs a mapping from the methods declared in the beanHome class to
    * the proxy delegate class. This allows the methods to be either named
    * the same as the home interface method "create(...)" or as the bean
    * class method "ejbCreate(...)". This handles both local home and
    * remote home interface methods.
    */
   protected void mapHomeMethods(Class beanHome)
   {
      if( beanHome == null )
         return;

      Class delegateClass = delegate.getClass();
      Method[] methods = beanHome.getMethods();
      for(int m = 0; m < methods.length; m++)
      {
         // Check for ejbCreate... methods
         Method hm = methods[m];
         Class[] parameterTypes = hm.getParameterTypes();
         String name = hm.getName();
         name = "ejb" + Character.toUpperCase(name.charAt(0)) + name.substring(1);
         try
         {
            Method match = delegateClass.getMethod(name, parameterTypes);
            methodMap.put(hm, match);
         }
         catch(NoSuchMethodException e)
         {
            // Try for the home interface name without the ejb prefix
            name = hm.getName();
            try
            {
               Method match = delegateClass.getMethod(name, parameterTypes);
               methodMap.put(hm, match);
            }
            catch(NoSuchMethodException e2)
            {
               if( strict )
               {
                  String msg = "Missing home method:" + hm + " in delegate";
                  throw new SecurityException(msg);
               }
            }
         }
      }
   }

   /** Performs a mapping from the methods declared in the beanRemote class to
    * the proxy delegate class. This handles both local and remote interface
    * methods.
    */
   protected void mapRemoteMethods(Class beanRemote)
   {
      if( beanRemote == null )
         return;

      Class delegateClass = delegate.getClass();
      Method[] methods = beanRemote.getMethods();
      for(int m = 0; m < methods.length; m++)
      {
         Method rm = methods[m];
         Class[] parameterTypes = rm.getParameterTypes();
         String name = rm.getName();
         try
         {
            Method match = delegateClass.getMethod(name, parameterTypes);
            methodMap.put(rm, match);
         }
         catch(NoSuchMethodException e)
         {
            if( strict )
            {
               String msg = "Missing method:" + rm + " in delegate";
               throw new SecurityException(msg);
            }
         }
      }
   }
}
