/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.naming.test;

import org.jboss.test.JBossTestCase;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NameAlreadyBoundException;
import javax.naming.NameNotFoundException;
import javax.naming.NamingEnumeration;
import javax.naming.NamingException;
import javax.naming.Name;
import java.util.Properties;

/** Simple unit tests for the jndi implementation. Note that there cannot
 * be any security related tests in this file as typically it is not run
 * with the right classpath resources for that.
 */
public class SimpleUnitTestCase extends JBossTestCase
{
   /**
    * Constructor for the SimpleUnitTestCase object
    *
    * @param name  Test name
    */
   public SimpleUnitTestCase(String name)
   {
      super(name);
   }

   /**
    * Tests that the second time you create a subcontext you get an exception.
    *
    * @exception Exception  Description of Exception
    */
   public void testCreateSubcontext() throws Exception
   {
      getLog().debug("+++ testCreateSubcontext");
      InitialContext ctx = getInitialContext();
      ctx.createSubcontext("foo");
      try
      {
         ctx.createSubcontext("foo");
         fail("Second createSubcontext(foo) did NOT fail");
      }
      catch (NameAlreadyBoundException e)
      {
         getLog().debug("Second createSubcontext(foo) failed as expected");
      }
      ctx.createSubcontext("foo/bar");
      ctx.unbind("foo/bar");
      ctx.unbind("foo");
   }

   /** Lookup a name to test basic connectivity and lookup of a known name
    *
    * @throws Exception
    */
   public void testLookup() throws Exception
   {
      getLog().debug("+++ testLookup");
      InitialContext ctx = getInitialContext();
      Object obj = ctx.lookup("");
      getLog().debug("lookup('') = "+obj);
   }

   public void testNameChanges() throws Exception
   {
      getLog().debug("+++ testLookup");
      InitialContext ctx = getInitialContext();
      Name name = ctx.getNameParser("").parse("jnp://localhost/jmx");
      Name copy = (Name) name.clone();
      Object obj = ctx.lookup(name);
      getLog().debug("lookup("+name+") = "+obj);
      assertTrue("name.equals(copy), name="+name, name.equals(copy));
   }

   /** Lookup a name to test basic connectivity and lookup of a known name
    *
    * @throws Exception
    */
   public void testLookupFailures() throws Exception
   {
      getLog().debug("+++ testLookupFailures");
      // Look a name that does not exist
      Properties env = new Properties();
      InitialContext ctx = new InitialContext(env);
      try
      {
         Object obj = ctx.lookup("__bad_name__");
         fail("lookup(__bad_name__) should have thrown an exception");
      }
      catch(NameNotFoundException e)
      {
         getLog().debug("lookup(__bad_name__) failed as expected", e);
      }

      // Do a lookup on an server port that does not exist
      env.setProperty(Context.INITIAL_CONTEXT_FACTORY, "org.jnp.interfaces.NamingContextFactory");
      env.setProperty(Context.PROVIDER_URL, "jnp://localhost:65535/");
      env.setProperty("jnp.disableDiscovery", "true");
      getLog().debug("Creating InitialContext with env="+env);
      try
      {
         ctx = new InitialContext(env);
         Object obj = ctx.lookup("");
         fail("lookup('') should have thrown an exception");
      }
      catch(NamingException e)
      {
         getLog().debug("lookup('') failed as expected", e);
      }
   }

   public void testHaInvoker() throws Exception
   {
      getLog().debug("+++ testHaInvoker");
      Properties env = new Properties();
      env.setProperty(Context.PROVIDER_URL, "jnp://localhost:1100/");
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
   }

   /**
    * Test the creation of HA-JNDI sub-context
    *
    * @throws Exception
    */
   public void testCreateHaJndiSubcontext() throws Exception
   {
      getLog().debug("+++ testCreateSubcontext");
      // Look a name that does not exist
      java.util.Properties env = new java.util.Properties();
      env.setProperty(javax.naming.Context.INITIAL_CONTEXT_FACTORY, "org.jnp.interfaces.NamingContextFactory");
      env.setProperty(javax.naming.Context.PROVIDER_URL, "jnp://localhost:1100/");
      getLog().debug("Creating InitialContext with env="+env);

      InitialContext ctx = new javax.naming.InitialContext(env);
      Object obj = ctx.lookup("");
      getLog().debug("lookup('') against HA-JNDI succedded as expected");

      ctx.createSubcontext("foo");
      try
      {
         ctx.createSubcontext("foo");
         fail("Second createSubcontext(foo) against HA-JNDI did NOT fail");
      }
      catch (javax.naming.NameAlreadyBoundException e)
      {
         getLog().debug("Second createSubcontext(foo) against HA-JNDI failed as expected");
      }

      getLog().debug("binding foo/bar");
      ctx.createSubcontext("foo/bar");

      getLog().debug("unbinding foo/bar");
      ctx.unbind("foo/bar");

      getLog().debug("unbinding foo");
      ctx.unbind("foo");

      try
      {
         obj = ctx.lookup("foo");
         fail("lookup(foo) should have thrown an exception");
      }
      catch(NameNotFoundException e)
      {
         getLog().debug("lookup(foo) failed as expected", e);
      }
   }

   /** Test discovery with the partition name specified
    *
    * @throws Exception
    */
   public void testHaParitionName() throws Exception
   {
      getLog().debug("+++ testHaParitionName");
      Properties env = new Properties();
      env.setProperty(Context.PROVIDER_URL, "jnp://localhost:65535/");
      env.setProperty("jnp.partitionName", "DefaultPartition");
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

      // Now test discovery with a non-existent parition name
      env.setProperty(Context.PROVIDER_URL, "jnp://localhost:65535/");
      env.setProperty("jnp.partitionName", "__NotTheDefaultPartition__");
      try
      {
         ctx = new InitialContext(env);
         getLog().debug("Created InitialContext");
         obj = ctx.lookup("invokers");
         fail("Was able to lookup(invokers): "+obj);
      }
      catch(NamingException e)
      {
         getLog().debug("Partition specific discovery failed as expected", e);
      }
   }

   /** Test naming discovery with an explicit port
    * 
    * @throws Exception
    */ 
   public void testDiscoveryPort() throws Exception
   {
      getLog().debug("+++ testDiscoveryPort");
      Properties env = new Properties();
      env.setProperty(Context.PROVIDER_URL, "jnp://localhost:65535/");
      env.setProperty("jnp.discoveryPort", "1102");
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
   }

   public void testHttpInvoker() throws Exception
   {
      getLog().debug("+++ testHttpInvoker");
      Properties env = new Properties();
      env.setProperty(Context.INITIAL_CONTEXT_FACTORY, "org.jboss.naming.HttpNamingContextFactory");
      env.setProperty(Context.PROVIDER_URL, "http://localhost:8080/invoker/JNDIFactory");
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
   }

}
