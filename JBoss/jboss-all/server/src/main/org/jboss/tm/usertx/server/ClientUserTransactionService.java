/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.tm.usertx.server;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.UndeclaredThrowableException;
import java.util.Map;
import java.util.HashMap;
import java.util.Collections;

import javax.management.ObjectName;
import javax.naming.InitialContext;
import javax.naming.Context;

import org.jboss.system.ServiceMBeanSupport;
import org.jboss.tm.usertx.client.ClientUserTransaction;
import org.jboss.tm.usertx.interfaces.UserTransactionSessionFactory;
import org.jboss.tm.usertx.interfaces.UserTransactionSession;
import org.jboss.invocation.Invocation;
import org.jboss.invocation.MarshalledInvocation;

/**
 *  This is a JMX service handling the serverside of UserTransaction
 *  usage for standalone clients.
 *
 * @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.6.2.4 $
 */
public class ClientUserTransactionService
      extends ServiceMBeanSupport
      implements ClientUserTransactionServiceMBean
{
   // Constants -----------------------------------------------------
   /** The location where the javax.transaction.UserTransaction is bound on the server */
   public static String JNDI_NAME = "UserTransaction";

   // Attributes ----------------------------------------------------
   /** The UserTransactionSession and UserTransactionSessionFactory method hashes */
   private Map marshalledInvocationMapping = new HashMap();
   /** The proxy factory service used for the UserTransactionSession */
   private ObjectName txProxyName;
   /** The stateless proxy used for all UserTransactionSessions */
   private Object txProxy;

   /** Set the name of the proxy factory service used for the 
    * UserTransactionSession
    * @param proxyName
    */ 
   public void setTxProxyName(ObjectName proxyName)
   {
      this.txProxyName = proxyName;      
   }

   /** Expose UserTransactionSession and UserTransactionSessionFactory
    * interfaces via JMX to invokers.
    *
    * @jmx:managed-operation
    *
    * @param invocation    A pointer to the invocation object
    * @return              Return value of method invocation.
    *
    * @throws Exception    Failed to invoke method.
    */
   public Object invoke(Invocation invocation) throws Exception
   {
      // Set the method hash to Method mapping
      if (invocation instanceof MarshalledInvocation)
      {
         MarshalledInvocation mi = (MarshalledInvocation) invocation;
         mi.setMethodMap(marshalledInvocationMapping);
      }
      // Invoke the method via reflection
      Method method = invocation.getMethod();
      Object[] args = invocation.getArguments();
      Object value = null;
      try
      {
         if( UserTransactionSessionFactory.class.isAssignableFrom(method.getDeclaringClass()) )
         {
            // Just return the UserTransactionSession proxy as its stateless
            value = txProxy;
         }
         else if( method.getName().equals("begin") )
         {
            // Begin a new transaction
            Integer timeout = (Integer) args[0];
            UserTransactionSession session = UserTransactionSessionImpl.getInstance();
            value = session.begin(timeout.intValue());
         }
         else if( method.getName().equals("destroy"))
         {
            /* We do nothing as the tx will timeout and the tx map is shared
            across all sessions as we have no association with the txs
            a given client has started.
            */
         }
         else
         {
            UserTransactionSession session = UserTransactionSessionImpl.getInstance();
            value = method.invoke(session, args);
         }
      }
      catch(InvocationTargetException e)
      {
         Throwable t = e.getTargetException();
         if( t instanceof Exception )
            throw (Exception) t;
         else
            throw new UndeclaredThrowableException(t, method.toString());
      }

      return value;
   }

   // ServiceMBeanSupport overrides ---------------------------------
   
   protected void startService()
         throws Exception
   {
      Context ctx = new InitialContext();
      // Bind the in VM UserTransaction interface
      ctx.bind(JNDI_NAME, ClientUserTransaction.getSingleton());

      // Get the UserTransactionSession proxy
      txProxy = getServer().getAttribute(txProxyName, "Proxy");

      // Build the UserTransactionSession interface method map
      HashMap tmpMap = new HashMap(13);
      Method[] methods = UserTransactionSession.class.getMethods();
      for(int m = 0; m < methods.length; m ++)
      {
         Method method = methods[m];
         Long hash = new Long(MarshalledInvocation.calculateHash(method));
         tmpMap.put(hash, method);
      }
      // Add the UserTransactionSessionFactory interface method map
      methods = UserTransactionSessionFactory.class.getMethods();
      for(int m = 0; m < methods.length; m ++)
      {
         Method method = methods[m];
         Long hash = new Long(MarshalledInvocation.calculateHash(method));
         tmpMap.put(hash, method);
      }
      marshalledInvocationMapping = Collections.unmodifiableMap(tmpMap);
   }

   protected void stopService()
   {
      try
      {
         Context ctx = new InitialContext();
         ctx.unbind(JNDI_NAME);
      }
      catch (Exception e)
      {
         log.warn("Failed to unbind "+JNDI_NAME, e);
      }
   }

}
