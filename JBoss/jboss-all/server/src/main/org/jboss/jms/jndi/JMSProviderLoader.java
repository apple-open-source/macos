/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.jms.jndi;

import javax.management.ObjectName;
import javax.management.MBeanServer;
import javax.management.MalformedObjectNameException;

import javax.naming.Context;
import javax.naming.Name;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.naming.NameNotFoundException;

import org.jboss.deployment.DeploymentException;
import org.jboss.system.ServiceMBeanSupport;

/**
 * A JMX service to load a JMSProviderAdapter and register it.
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 * 
 * <p>Created: Wed Nov 29 14:07:07 2000
 *
 * <p>6/22/01 - hchirino - The queue/topic jndi references are now configed via JMX
 *
 * @author  <a href="mailto:cojonudo14@hotmail.com">Hiram Chirino</a>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @version $Revision: 1.13.2.1 $
 */
public class JMSProviderLoader
   extends ServiceMBeanSupport
   implements JMSProviderLoaderMBean
{
   /** The provider adapter which we are loading. */
   protected JMSProviderAdapter providerAdapter;

   /** The provider url. */
   protected String url;

   /** The provider name. */
   protected String providerName;

   /** The provider adapter classname. */
   protected String providerAdapterClass;

   /** The queue factory jndi name. */
   protected String queueFactoryRef;

   /** The topic factory jndi name. */   
   protected String topicFactoryRef;

   /** The JNDI name to bind the adapter to. */
   protected String jndiName;

   /**
    * @jmx:managed-attribute
    */
   public void setProviderName(String name)
   {
      this.providerName = name;
   }      
   
   /**
    * @jmx:managed-attribute
    */
   public String getProviderName()
   {
      return providerName;
   }      
   
   /**
    * @jmx:managed-attribute
    */
   public void setProviderAdapterClass(String clazz)
   {
      providerAdapterClass = clazz;
   }      
   
   /**
    * @jmx:managed-attribute
    */
   public String getProviderAdapterClass()
   {
      return providerAdapterClass;
   }      
   
   /**
    * @jmx:managed-attribute
    */
   public void setProviderUrl(final String url)
   {
      this.url = url;
   }      

   /**
    * @jmx:managed-attribute
    */
   public String getProviderUrl()
   {
      return url;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setAdapterJNDIName(final String name)
   {
      this.jndiName = name;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getAdapterJNDIName()
   {
      return jndiName;
   }
   
   /**
    * @jmx:managed-attribute
    */
   public void setQueueFactoryRef(final String newQueueFactoryRef) {
      queueFactoryRef = newQueueFactoryRef;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setTopicFactoryRef(final String newTopicFactoryRef) {
      topicFactoryRef = newTopicFactoryRef;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getQueueFactoryRef() {
      return queueFactoryRef;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getTopicFactoryRef() {
      return topicFactoryRef;
   }
   

   ///////////////////////////////////////////////////////////////////////////
   //                    ServiceMBeanSupport Overrides                      //
   ///////////////////////////////////////////////////////////////////////////

   public String getName()
   {
      return providerName;
   }
   
   protected void startService() throws Exception
   {
      // validate the configuration
      if (queueFactoryRef == null)
         throw new DeploymentException
            ("missing required attribute: QueueFactoryRef");

      if (topicFactoryRef == null)
         throw new DeploymentException
            ("missing required attribute: TopicFactoryRef");

      Class cls = Thread.currentThread().getContextClassLoader().loadClass(providerAdapterClass);
      providerAdapter = (JMSProviderAdapter)cls.newInstance();
      providerAdapter.setName(providerName);
      providerAdapter.setProviderUrl(url);
      providerAdapter.setQueueFactoryRef(queueFactoryRef);
      providerAdapter.setTopicFactoryRef(topicFactoryRef);

      InitialContext context = new InitialContext();
      try {
         // Bind in JNDI
         if (jndiName == null) {
            String name = providerAdapter.getName();
            jndiName = "java:/" + name;
         }
         bind(context, jndiName, providerAdapter);
         log.info("Bound adapter to " + jndiName);
      }
      finally {
         context.close();
      }
   }      

   protected void stopService() throws Exception
   {
      InitialContext context = new InitialContext();
      
      try {
         // Unbind from JNDI
         String name = providerAdapter.getName();
         String jndiname = "java:/" + name;
         context.unbind(jndiname);
         if (log.isInfoEnabled())
            log.info("unbound adapter " + name + " from " + jndiname);
         
         //source.close();
         //log.log("XA Connection pool "+name+" shut down");
      }
      finally {
         context.close();
      }
   }      

   private void bind(Context ctx, String name, Object val)
      throws NamingException
   {
      if (log.isDebugEnabled())
         log.debug("attempting to bind " + val + " to " + name);

      // Bind val to name in ctx, and make sure that all
      // intermediate contexts exist
      Name n = ctx.getNameParser("").parse(name);
      while (n.size() > 1)
      {
         String ctxName = n.get(0);
         try
         {
            ctx = (Context)ctx.lookup(ctxName);
         } catch (NameNotFoundException e)
         {
            ctx = ctx.createSubcontext(ctxName);
         }
         n = n.getSuffix(1);
      }

      ctx.bind(n.get(0), val);
   }      
}
