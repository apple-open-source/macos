/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.interceptor;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
import java.util.Map;
import java.util.HashMap;

import javax.management.MBeanInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanParameterInfo;
import javax.management.ReflectionException;
import javax.management.MBeanException;
import javax.management.RuntimeErrorException;
import javax.management.RuntimeOperationsException;

import org.jboss.mx.server.MBeanInvoker;

/** The interceptor that dispatches the attribute accesses and operations
 * to the ModelMBean resource.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.2.8.6 $
 *
 */
public class ModelMBeanInterceptor
   extends AbstractInterceptor
{

   // Attributes ----------------------------------------------------
   private Map methodMap = new HashMap();

   // Constructors --------------------------------------------------
   public ModelMBeanInterceptor(MBeanInfo info, MBeanInvoker invoker)
      throws ReflectionException
   {
      super(info, invoker);

      MBeanOperationInfo[] operations = info.getOperations();
      Object resource = invoker.getResource();

      StringBuffer paramBuffer = new StringBuffer();
      for (int i = 0; i < operations.length; ++i)
      {
         String name = operations[i].getName();
         try
         {
            MBeanParameterInfo[] params = operations[i].getSignature();
            paramBuffer.setLength(0);
            for (int j = 0; j < params.length; ++j)
            {
               paramBuffer.append(params[j].getType());
            }
            Class resourceClass = resource.getClass();
            ClassLoader resourceLoader = resourceClass.getClassLoader();
            Class[] opSig = StandardMBeanInterceptor.getSignatureAsClassArray(params, resourceLoader);
            Method opMethod =  resourceClass.getMethod(name, opSig);
            methodMap.put(name + paramBuffer.toString(), opMethod);
         }
         catch (ClassNotFoundException e)
         {
            throw new ReflectionException(e, "Unable to load operation " + name
               + " parameter type: " + e.getMessage());
         }
         catch (NoSuchMethodException e)
         {
            throw new ReflectionException(e, "Unable to find operation "
               + name + "("+paramBuffer+")");
         }
      }
   }

   // Public ------------------------------------------------------------

   // Interceptor overrides ----------------------------------------------
   public Object invoke(Invocation invocation) throws InvocationException
   {
      try
      {
         Object resource = invocation.getResource();
         if( resource == null )
         {
            String msg = "No resource found in: "+ invocation.getName();
            new InvocationException(new NullPointerException(msg), msg);
         }
         String opSig = invocation.getOperationWithSignature();
         Method m = (Method)methodMap.get(opSig);
         return m.invoke(resource, invocation.getArgs());
      }
      catch (IllegalAccessException e)
      {
         throw new InvocationException(e, "Illegal access to method "
            + invocation.getName());
      }
      catch (IllegalArgumentException e)
      {
         throw new InvocationException(e, "Illegal operation arguments in "
            + invocation.getName() + ": " + e.getMessage());
      }
      catch (InvocationTargetException e)
      {
         // Handle a declared exception
         Throwable targetEx = e.getTargetException();
         String msg = "Operation "+invocation.getName() + " on MBean "
            + info.getClassName() + " has thrown an exception: "
            + targetEx.getMessage();
         // Simply throw the MBeanServer.invoke declared exceptions
         if( targetEx instanceof MBeanException || targetEx instanceof ReflectionException )
            throw new InvocationException(targetEx, msg);
         // If its not an Error wrap it in a ReflectionException
         else if ( targetEx instanceof Exception )
         {
            ReflectionException re = new ReflectionException((Exception) targetEx);
            throw new InvocationException(re, msg);
         }
         // Wrap Errors it in a RuntimeErrorException
         else
         {
            RuntimeErrorException ree = new RuntimeErrorException((Error) targetEx);
            MBeanException me = new MBeanException(ree);
            throw new InvocationException(me, msg);
         }
      }
      catch (NullPointerException e)
      {
         String msg = "Operation " + invocation.getName() +
               " is not a declared management operation.";
         NullPointerException npe = new NullPointerException(msg);
         RuntimeOperationsException roe = new RuntimeOperationsException(npe, msg);
         throw new InvocationException(roe, msg);
      }
   }

}
