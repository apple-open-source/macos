/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.naming.test;

import java.util.Properties;
import java.security.Principal;
import java.lang.reflect.UndeclaredThrowableException;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.naming.NamingEnumeration;
import javax.security.auth.login.LoginContext;

import org.jboss.test.JBossTestCase;
import org.jboss.test.util.AppCallbackHandler;
import org.jboss.security.SecurityAssociation;

/** Tests of secured access to the JNDI naming service. This testsuite will
 * be run with the standard security resources available via the classpath.
 */
public class SecurityUnitTestCase extends JBossTestCase
{
   /**
    * Constructor for the SecurityUnitTestCase object
    *
    * @param name  Test name
    */
   public SecurityUnitTestCase(String name)
   {
      super(name);
   }

   /** Test access to the security http InitialContext without a login
    *
    * @throws Exception
    */
   public void testSecureHttpInvokerFailure() throws Exception
   {
      getLog().debug("+++ testSecureHttpInvokerFailure");
      Properties env = new Properties();
      env.setProperty(Context.INITIAL_CONTEXT_FACTORY, "org.jboss.naming.HttpNamingContextFactory");

      // Test the secured JNDI factory
      env.setProperty(Context.PROVIDER_URL, "http://localhost:8080/invoker/restricted/JNDIFactory");
      getLog().debug("Creating InitialContext with env="+env);

      // Try without a login to ensure the lookup fails
      try
      {
         getLog().debug("Testing without valid login");
         InitialContext ctx1 = new InitialContext(env);
         getLog().debug("Created InitialContext");
         Object obj1 = ctx1.lookup("invokers");
         getLog().debug("lookup(invokers) : "+obj1);
         fail("Should not have been able to lookup(invokers)");
      }
      catch(Exception e)
      {
         getLog().debug("Lookup failed as expected", e);
      }

   }

   /** Test access to the JNDI naming service over a restricted http URL
    */
   public void testSecureHttpInvoker() throws Exception
   {
      getLog().debug("+++ testSecureHttpInvoker");
      Properties env = new Properties();
      env.setProperty(Context.INITIAL_CONTEXT_FACTORY, "org.jboss.naming.HttpNamingContextFactory");

      // Specify the login conf file location
      String authConf = super.getResourceURL("security/auth.conf");
      getLog().debug("Using auth.conf: "+authConf);
      System.setProperty("java.security.auth.login.config", authConf);
      AppCallbackHandler handler = new AppCallbackHandler("invoker", "invoker".toCharArray());
      LoginContext lc = new LoginContext("testSecureHttpInvoker", handler);
      lc.login();

      // Test the secured JNDI factory
      env.setProperty(Context.PROVIDER_URL, "http://localhost:8080/invoker/restricted/JNDIFactory");
      getLog().debug("Creating InitialContext with env="+env);
      InitialContext ctx = new InitialContext(env);
      getLog().debug("Created InitialContext");
      Object obj = ctx.lookup("invokers");
      getLog().debug("lookup(invokers) : "+obj);
      Context invokersCtx = (Context) obj;
      NamingEnumeration list = invokersCtx.list("");
      while( list.hasMore() )
      {
         Object entry = list.next();
         getLog().debug(" + "+entry);
      }
      ctx.close();
      lc.logout();

      Principal p = SecurityAssociation.getPrincipal();
      assertTrue("SecurityAssociation.getPrincipal is null", p == null);

      /* This is now failing because we don't appear to have anyway to flush
      the java.net.Authenticator cache. Need to figure out how this can
      be done or switch a better http client library.
      */
      // Try without a login to ensure the lookup fails
      testSecureHttpInvokerFailure();
   }

   /** Test access of the readonly context without a login
    *
    * @throws Exception
    */
   public void testHttpReadonlyLookup() throws Exception
   {
      getLog().debug("+++ testHttpReadonlyLookup");
      /* Try without a login to ensure that a lookup against "readonly" works.
       *First create the readonly context using the standard JNDI factory
      */
      InitialContext bootCtx = new InitialContext();
      try
      {
         bootCtx.unbind("readonly");
      }
      catch(NamingException ignore)
      {
      }
      Context readonly = bootCtx.createSubcontext("readonly");
      readonly.bind("data", "somedata");

      Properties env = new Properties();
      env.setProperty(Context.INITIAL_CONTEXT_FACTORY, "org.jboss.naming.HttpNamingContextFactory");
      env.setProperty(Context.PROVIDER_URL, "http://localhost:8080/invoker/ReadOnlyJNDIFactory");
      getLog().debug("Creating InitialContext with env="+env);
      InitialContext ctx = new InitialContext(env);
      Object data = ctx.lookup("readonly/data");
      getLog().debug("lookup(readonly/data) : "+data);
      try
      {
         // Try to bind into the readonly context
         ctx.bind("readonly/mydata", "otherdata");
         fail("Was able to bind into the readonly context");
      }
      catch(UndeclaredThrowableException e)
      {
         getLog().debug("Invalid exception", e);
         fail("UndeclaredThrowableException thrown");
      }
      catch(Exception e)
      {
         getLog().debug("Bind failed as expected", e);
      }

      try
      {
         // Try to access a context other then under readonly
         ctx.lookup("invokers");
         fail("Was able to lookup(invokers)");
      }
      catch(UndeclaredThrowableException e)
      {
         getLog().debug("Invalid exception", e);
         fail("UndeclaredThrowableException thrown");
      }
      catch(Exception e)
      {
         getLog().debug("lookup(invokers) failed as expected", e);
      }
   }

   /** Test access of the readonly context without a login
    *
    * @throws Exception
    */
   public void testHttpReadonlyContextLookup() throws Exception
   {
      getLog().debug("+++ testHttpReadonlyContextLookup");
      /* Deploy a customized naming service with a NamingContext proxy
      replacement interceptor
      */
      deploy("naming-readonly.sar");

      /* Try without a login to ensure that a lookup against "readonly" works.
      First create the readonly context using a non-readonly JNDI factory
      */
      Properties env = new Properties();
      env.setProperty(Context.INITIAL_CONTEXT_FACTORY,
         "org.jboss.test.naming.test.BootstrapNamingContextFactory");
      env.setProperty(Context.PROVIDER_URL, "jnp://localhost:1099");
      env.setProperty("bootstrap-binding", "naming/Naming");
      getLog().debug("Creating bootstrap InitialContext with env="+env);
      InitialContext bootCtx = new InitialContext(env);
      try
      {
         bootCtx.unbind("readonly");
      }
      catch(NamingException ignore)
      {
      }
      getLog().debug("Creating readonly context");
      bootCtx.createSubcontext("readonly");
      bootCtx.bind("readonly/data", "somedata");

      // Test access through the readonly proxy
      env.setProperty("bootstrap-binding", "naming/ReadOnlyNaming");
      getLog().debug("Creating InitialContext with env="+env);
      InitialContext ctx = new InitialContext(env);
      Object data = ctx.lookup("readonly/data");
      getLog().debug("lookup(readonly/data) : "+data);
      // Lookup the readonly context to see that the readonly proxy is seen
      Object robinding = ctx.lookup("readonly");
      getLog().debug("Looked up readonly: "+robinding);
      Context roctx = (Context) robinding;
      data = roctx.lookup("data");
      getLog().debug("Looked up data: "+data);
      assertTrue("lookup(data) == somedata: "+data, "somedata".equals(data));
      try
      {
         // Try to bind into the readonly context
         roctx.bind("mydata", "otherdata");
         fail("Was able to bind into the readonly context");
      }
      catch(UndeclaredThrowableException e)
      {
         getLog().debug("Invalid exception", e);
         fail("UndeclaredThrowableException thrown");
      }
      catch(Exception e)
      {
         getLog().debug("Bind failed as expected", e);
      }

      try
      {
         // Try to access a context other then under readonly
         ctx.lookup("invokers");
         fail("Was able to lookup(invokers)");
      }
      catch(UndeclaredThrowableException e)
      {
         getLog().debug("Invalid exception", e);
         fail("UndeclaredThrowableException thrown");
      }
      catch(Exception e)
      {
         getLog().debug("lookup(invokers) failed as expected", e);
      }
      undeploy("naming-readonly.sar");
   }

   /** Test an initial context factory that does a JAAS login to validate the
    * credentials passed in
    */
   public void testLoginInitialContext() throws Exception
   {
      getLog().debug("+++ testLoginInitialContext");
      Properties env = new Properties();
      // Try with a login that should succeed
      env.setProperty(Context.INITIAL_CONTEXT_FACTORY, "org.jboss.security.jndi.LoginInitialContextFactory");
      env.setProperty(Context.PROVIDER_URL, "jnp://localhost:1099/");
      env.setProperty(Context.SECURITY_CREDENTIALS, "theduke");
      env.setProperty(Context.SECURITY_PRINCIPAL, "jduke");
      env.setProperty(Context.SECURITY_PROTOCOL, "testLoginInitialContext");

      // Specify the login conf file location
      String authConf = super.getResourceURL("security/auth.conf");
      System.setProperty("java.security.auth.login.config", authConf);

      getLog().debug("Creating InitialContext with env="+env);
      InitialContext ctx = new InitialContext(env);
      getLog().debug("Created InitialContext");
      Object obj = ctx.lookup("invokers");
      getLog().debug("lookup(invokers) : "+obj);
      Context invokersCtx = (Context) obj;
      NamingEnumeration list = invokersCtx.list("");
      while( list.hasMore() )
      {
         Object entry = list.next();
         getLog().debug(" + "+entry);
      }
      ctx.close();

      // Try with a login that should fail
      env.setProperty(Context.SECURITY_CREDENTIALS, "badpass");
      try
      {
         getLog().debug("Creating InitialContext with env="+env);
         ctx = new InitialContext(env);
         fail("Was able to create InitialContext with badpass");
      }
      catch(NamingException e)
      {
         getLog().debug("InitialContext failed as expected with exception", e);
      }
   }
}
