/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il;

import java.util.Properties;

import javax.jms.IllegalStateException;
import javax.management.ObjectName;
import javax.naming.InitialContext;
import org.jboss.mq.GenericConnectionFactory;
import org.jboss.mq.SpyConnectionFactory;
import org.jboss.mq.SpyXAConnectionFactory;
import org.jboss.mq.server.JMSServerInterceptor;
import org.jboss.system.ServiceMBeanSupport;
import javax.naming.Context;
import javax.naming.NamingException;

/**
 *  This abstract class handles life cycle managment of the ServeIL. Should be
 *  extended to provide a full implementation.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version    $Revision: 1.13.2.1 $
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 */
public abstract class ServerILJMXService extends ServiceMBeanSupport implements ServerILJMXServiceMBean
{

   private ObjectName jbossMQService;
   private Invoker jmsServer;

   protected String connectionFactoryJNDIRef;
   protected String xaConnectionFactoryJNDIRef;
   protected long pingPeriod = 60000L;
   


   /**
    * Get the value of JBossMQService.
    * @return value of JBossMQService.
    *
    * @jmx:managed-attribute
    */
   public ObjectName getJBossMQService() 
   {
      return jbossMQService;
   }
   
   /**
    * Set the value of JBossMQService.
    * @param v  Value to assign to JBossMQService.
    *
    * @jmx:managed-attribute
    */
   public void setInvoker(ObjectName  jbossMQService) 
   {
      this.jbossMQService = jbossMQService;
   }

   public void startService() throws Exception
   {
      jmsServer = (Invoker)getServer().getAttribute(jbossMQService, "Invoker");;
      if (jmsServer == null) 
      {
         throw new IllegalStateException("Cannot find JBossMQService!");
      } // end of if ()
   }

   public void stopService() throws Exception
   {
      jmsServer = null;
   }
   
   /**
    * @param  newConnectionFactoryJNDIRef  the JNDI reference where the
    *      connection factory should be bound to
    *
    * @jmx:managed-attribute
    */
   public void setConnectionFactoryJNDIRef(java.lang.String newConnectionFactoryJNDIRef)
   {
      connectionFactoryJNDIRef = newConnectionFactoryJNDIRef;
   }

   /**
    * @param  newXaConnectionFactoryJNDIRef  java.lang.String the JNDI reference
    *      where the xa connection factory should be bound to
    *
    * @jmx:managed-attribute
    */
   public void setXAConnectionFactoryJNDIRef(java.lang.String newXaConnectionFactoryJNDIRef)
   {
      xaConnectionFactoryJNDIRef = newXaConnectionFactoryJNDIRef;
   }

   /**
    * @return     The ClientConnectionProperties value
    * @returns    Properties contains all the parameters needed to create a
    *      connection from the client to this IL
    */
   public java.util.Properties getClientConnectionProperties()
   {
      Properties rc = new Properties();
      rc.setProperty(ServerILFactory.PING_PERIOD_KEY, ""+pingPeriod );
      return rc;
   }
   
   /**
    * @return     The ServerIL value
    * @returns    ServerIL An instance of the Server IL, used for
    */
   public abstract ServerIL getServerIL();

   /**
    * @return    java.lang.String the JNDI reference where the connection
    *      factory should be bound to
    *
    * @jmx:managed-attribute
    */
   public java.lang.String getConnectionFactoryJNDIRef()
   {
      return connectionFactoryJNDIRef;
   }

   /**
    * @return    java.lang.String the JNDI reference where the xa connection
    *      factory should be bound to
    *
    * @jmx:managed-attribute
    */
   public java.lang.String getXAConnectionFactoryJNDIRef()
   {
      return xaConnectionFactoryJNDIRef;
   }

   /**
    *  Binds the connection factories for this IL
    *
    * @throws  javax.naming.NamingException  it cannot be unbound
    */
   public void bindJNDIReferences() throws javax.naming.NamingException
   {
      GenericConnectionFactory gcf = new GenericConnectionFactory(getServerIL(), getClientConnectionProperties());
      SpyConnectionFactory scf = new SpyConnectionFactory(gcf);
      SpyXAConnectionFactory sxacf = new SpyXAConnectionFactory(gcf);

      // Get an InitialContext
      InitialContext ctx = new InitialContext();
      rebind( ctx, connectionFactoryJNDIRef, scf);
      rebind( ctx, xaConnectionFactoryJNDIRef, sxacf);

   }
   
   protected void rebind(Context ctx, String name, Object val)
   throws NamingException
   {
      // Bind val to name in ctx, and make sure that all intermediate contexts exist
      javax.naming.Name n = ctx.getNameParser("").parse(name);
      while (n.size() > 1)
      {
         String ctxName = n.get(0);
         try
         {
            ctx = (Context)ctx.lookup(ctxName);
         } catch (javax.naming.NameNotFoundException e)
         {
            ctx = ctx.createSubcontext(ctxName);
         }
         n = n.getSuffix(1);
      }
      
      ctx.rebind(n.get(0), val);
   }
      

   /**
    *  Unbinds the connection factories for this IL
    *
    * @throws  javax.naming.NamingException  it cannot be unbound
    */
   public void unbindJNDIReferences() throws javax.naming.NamingException
   {
      // Get an InitialContext
      InitialContext ctx = new InitialContext();
      ctx.unbind(connectionFactoryJNDIRef);
      ctx.unbind(xaConnectionFactoryJNDIRef);
   }

   /**
    * @return                                Description of the Returned Value
    * @exception  Exception                  Description of Exception
    * @throws  javax.naming.NamingException  if the server is not found
    */
   public Invoker lookupJMSServer()
   {
      return jmsServer;
   }
   
   /**
    * @return    long the period of time in ms to wait between connection pings
    *      factory should be bound to
    *
    * @jmx:managed-attribute
    */
   public long getPingPeriod() {
      return pingPeriod;
   }
   
   /**
    * @param  period long the period of time in ms to wait between connection pings
    *
    * @jmx:managed-attribute
    */
   public void setPingPeriod(long period) {
      pingPeriod = period;
   }

}
