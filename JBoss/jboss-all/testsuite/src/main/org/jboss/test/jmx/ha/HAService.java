package org.jboss.test.jmx.ha;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.UndeclaredThrowableException;
import java.security.Principal;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.MarshalledInvocation;
import org.jboss.security.SecurityAssociation;
import org.jboss.system.Registry;
import org.jboss.system.ServiceMBeanSupport;

public class HAService
   extends ServiceMBeanSupport
   implements HAServiceRemote, HAServiceMBean
{
   private int count = 0;

   private Map marshalledInvocationMapping;

   public void startService()
      throws Exception
   {
      // Calulate method hashes for remote invocation
      Method[] methods = HAServiceRemote.class.getMethods();
      HashMap tmpMap = new HashMap(methods.length);
      for(int m = 0; m < methods.length; m ++)
      {
         Method method = methods[m];
         Long hash = new Long(MarshalledInvocation.calculateHash(method));
         tmpMap.put(hash, method);
      }
      marshalledInvocationMapping = Collections.unmodifiableMap(tmpMap);

      // Place our ObjectName hash into the Registry so invokers can resolve it
      Registry.bind(new Integer(serviceName.hashCode()), serviceName);
   }

   public void stopService()
      throws Exception
   {
      // No longer available to the invokers
      Registry.unbind(new Integer(serviceName.hashCode()));
   }

   /** 
    * Expose the client mapping
    */
   public Map getMethodMap()
   {
      return marshalledInvocationMapping;
   }

   /** 
    * This is the "remote" entry point
    */
   public Object invoke(Invocation invocation)
      throws Exception
   {
      // Invoked remotely, inject method resolution
      if (invocation instanceof MarshalledInvocation)
      {
         MarshalledInvocation mi = (MarshalledInvocation) invocation;
         mi.setMethodMap(marshalledInvocationMapping);
      }
      Method method = invocation.getMethod();
      Object[] args = invocation.getArguments();

      // Setup any security context (only useful if something checks it, this impl doesn't)
      Principal principal = invocation.getPrincipal();
      Object credential = invocation.getCredential();
      SecurityAssociation.setPrincipal(principal);
      SecurityAssociation.setCredential(credential);

      // Dispatch the invocation
      try
      {
         return method.invoke(this, args);
      }
      catch(InvocationTargetException e)
      {
         Throwable t = e.getTargetException();
         if( t instanceof Exception )
            throw (Exception) t;
         else
            throw new UndeclaredThrowableException(t, method.toString());
      }
      finally
      {
         // Clear the security context
         SecurityAssociation.clear();
      }
   }

   // Implementation of remote methods

   public String hello()
   {
      return "Hello";
   }

}
