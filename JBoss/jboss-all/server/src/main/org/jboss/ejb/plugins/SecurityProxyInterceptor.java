/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins;

import java.security.Principal;
import javax.ejb.EJBContext;
import javax.naming.InitialContext;

import org.jboss.ejb.Container;
import org.jboss.ejb.EJBProxyFactoryContainer;
import org.jboss.ejb.EnterpriseContext;
import org.jboss.invocation.Invocation;
import org.jboss.logging.Logger;

import org.jboss.security.AuthenticationManager;
import org.jboss.security.SecurityProxy;
import org.jboss.security.SecurityProxyFactory;

/**
 * The SecurityProxyInterceptor is where the EJB custom security proxy
 * integration is performed. This interceptor is dynamically added to container
 * interceptors when the deployment descriptors specifies a security
 * proxy. It is added just before the container interceptor so that the
 * interceptor has access to the EJB instance and context.
 * 
 * @author <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>.
 * @version $Revision: 1.16.2.2 $
 */
public class SecurityProxyInterceptor
   extends AbstractInterceptor
{
   /** 
    * The JNDI name of the SecurityProxyFactory used to wrap security
    * proxy objects that do not implement the SecurityProxy interface
    */
   public final String SECURITY_PROXY_FACTORY_NAME =
      "java:/SecurityProxyFactory";

   /** Instance logger. */
   protected Logger log = Logger.getLogger(this.getClass());

   protected AuthenticationManager securityManager;

   /**
    * @supplierCardinality 0..1
    * @clientCardinality 1 
    * @supplierQualifier custom security
    */
   protected SecurityProxy securityProxy;

   public SecurityProxyInterceptor()
   {
      super();
   }

   public void setContainer(Container container)
   {
      super.setContainer(container);
      if( container != null )
      {
         securityManager = container.getSecurityManager();
         Object secProxy = container.getSecurityProxy();
         if( secProxy != null )
         {
            // If this is not a SecurityProxy instance then use the default
            // SecurityProxy implementation
            if( (secProxy instanceof SecurityProxy) == false )
            {
               try
               {
                  // Get default SecurityProxyFactory from JNDI at
                  InitialContext iniCtx = new InitialContext();
                  SecurityProxyFactory proxyFactory =
                     (SecurityProxyFactory)iniCtx.lookup(SECURITY_PROXY_FACTORY_NAME);
                  securityProxy = proxyFactory.create(secProxy);
               }
               catch (Exception e)
               {
                  log.error("Failed to initialze DefaultSecurityProxy", e);
               }
            }
            else
            {
               securityProxy = (SecurityProxy) secProxy;
            }

            // Initialize the securityProxy
            try
            {
               EJBProxyFactoryContainer  ic =
                  (EJBProxyFactoryContainer)container;
               Class beanHome = ic.getHomeClass();
               Class beanRemote = ic.getRemoteClass();
               Class beanLocalHome = ic.getLocalHomeClass();
               Class beanLocal = ic.getLocalClass();
               if( beanLocal == null )
               {
                  securityProxy.init(beanHome, beanRemote, securityManager);
               }
               else
               {
                  securityProxy.init(beanHome, beanRemote, beanLocalHome,
                     beanLocal, securityManager);
               }
            }
            catch(Exception e)
            {
               log.error("Failed to initialze SecurityProxy", e);
            }
            if (log.isInfoEnabled())
               log.info("Initialized SecurityProxy=" + securityProxy);
         }
      }
   }

   // Container implementation --------------------------------------
   
   public void start() throws Exception
   {
      super.start();
   }

   public Object invokeHome(Invocation mi) throws Exception
   {
      // Apply any custom security checks
      if( securityProxy != null )
      {
         EJBContext ctx = null;
         EnterpriseContext ectx = (EnterpriseContext)mi.getEnterpriseContext();
         if( ectx != null )
            ctx = ectx.getEJBContext();
         Object[] args = mi.getArguments();
         securityProxy.setEJBContext(ctx);
         try
         {
            securityProxy.invokeHome(mi.getMethod(), args);
         }
         catch(SecurityException e)
         {
            Principal principal = mi.getPrincipal();
            String msg = "SecurityProxy.invokeHome exception, principal=" + principal;
            log.error(msg, e);
            throw e;
         }
      }
      return getNext().invokeHome(mi);
   }
   
   public Object invoke(Invocation mi) throws Exception
   {
      // Apply any custom security checks
      if( securityProxy != null )
      {
         EnterpriseContext ectx = (EnterpriseContext)mi.getEnterpriseContext();
         Object bean = ectx.getInstance();
         EJBContext ctx = ectx.getEJBContext();
         Object[] args = mi.getArguments();
         securityProxy.setEJBContext(ctx);
         try
         {
            securityProxy.invoke(mi.getMethod(), args, bean);
         }
         catch(SecurityException e)
         {
            Principal principal = mi.getPrincipal();
            String msg = "SecurityProxy.invoke exception, principal="+principal;
            log.error(msg, e);
            throw e;
         }
      }
      return getNext().invoke(mi);
   }
}

