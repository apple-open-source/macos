/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.naming.test;

import java.util.Hashtable;
import javax.naming.RefAddr;
import javax.naming.StringRefAddr;
import javax.naming.Reference;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.naming.spi.InitialContextFactoryBuilder;
import javax.naming.spi.InitialContextFactory;
import javax.naming.spi.NamingManager;

import junit.extensions.TestSetup;
import junit.framework.Test;

import org.jboss.naming.ENCFactory;
import org.jnp.server.Main;
import org.jnp.interfaces.NamingContext;

/** Create a naming server instance in setUp and destroy it in tearDown.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class NamingServerSetup extends TestSetup
   implements InitialContextFactoryBuilder
{
   private Main namingServer;

   public NamingServerSetup(Test test)
   {
      super(test);
   }

   public InitialContextFactory createInitialContextFactory(Hashtable environment)
      throws NamingException
   {
      return new InVMInitialContextFactory();
   }

   protected void setUp() throws Exception
   {
      super.setUp();
      namingServer = new Main();
      namingServer.setPort(10099);
      namingServer.start();

      NamingManager.setInitialContextFactoryBuilder(this);
      /* Bind an ObjectFactory to "java:comp" so that "java:comp/env" lookups
         produce a unique context for each thread contexxt ClassLoader that
         performs the lookup.
      */
      InitialContext iniCtx = new InitialContext();
      ClassLoader topLoader = Thread.currentThread().getContextClassLoader();
      ENCFactory.setTopClassLoader(topLoader);
      RefAddr refAddr = new StringRefAddr("nns", "ENC");
      Reference envRef = new Reference("javax.naming.Context", refAddr, ENCFactory.class.getName(), null);
      Context ctx = (Context)iniCtx.lookup("java:");
      ctx.rebind("comp", envRef);
   }

   protected void tearDown() throws Exception
   {
      namingServer.stop();
      super.tearDown();
   }

   static class InVMInitialContextFactory implements InitialContextFactory
   {
      public Context getInitialContext(Hashtable env)
         throws NamingException
      {
         Hashtable env2 = (Hashtable) env.clone();
         env2.remove(Context.PROVIDER_URL);
         return new NamingContext(env2, null, null);
      }
   }
}
