/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jnp.client;

import java.net.*;
import java.io.*;

import java.io.IOException;
import java.net.MalformedURLException;
import java.rmi.NotBoundException;
import java.rmi.RemoteException;
import java.util.Properties;

import javax.naming.Binding;
import javax.naming.InitialContext;
import javax.naming.Name;
import javax.naming.LinkRef;
import javax.naming.Context;
import javax.naming.NamingEnumeration;
import javax.naming.NameClassPair;
import javax.naming.NameParser;
import javax.naming.NamingException;
import javax.naming.NameNotFoundException;
import javax.naming.Reference;
import javax.naming.StringRefAddr;

/**
 * This is a test client of the NamingServer. It calls the server
 * in various ways to test the functionality.
 *
 * <p>The JNDI configuration is provided in the jndi.properties file.
 *
 * @see org.jnp.interfaces.NamingContext
 * 
 * @author oberg
 * @author Scott_Stark@displayscape.com
 * @version $Revision: 1.7 $
 */
public class Main
   implements Runnable
{
    org.jnp.server.Main remoteServer;

   /**
    *   Start the test
    *
    * @param   args  
    * @exception   Exception  
    */
    public static void main(String[] args)
      throws Exception
    {
       System.setProperty("java.naming.factory.initial", "org.jnp.interfaces.NamingContextFactory");
       System.setProperty("java.naming.factory.url.pkgs", "org.jnp.interfaces");
       System.setProperty("java.naming.provider.url", "localhost");
       System.setErr(System.out);
       new Main().run();
    }

   // Constructors --------------------------------------------------

   // Public --------------------------------------------------------

   public void printName(String name)
      throws NamingException
   {
      Context ctx = (Context)new InitialContext().lookup("");
      Name n = ctx.getNameParser("").parse(name);
      System.out.println("'"+name+"'.size = "+n.size());
      for (int i = 0; i < n.size(); i++)
         System.out.println("  ["+i+"]"+n.get(i));
   }
   
   /**
    *   Show a JNDI context tree on system out.
    *
    * @param   ctx  
    * @exception   NamingException  
    */
   public void showTree(Context ctx)
      throws NamingException
   {
      showTree(ctx, Integer.MAX_VALUE);
   }
   public void showTree(Context ctx, int maxDepth)
      throws NamingException
   {
      System.out.println("----------------------------");
      showTree("/", ctx, 0, maxDepth);
      System.out.println("----------------------------");
   }

   // Runnable implementation ---------------------------------------

   /**
    *   Run the tests
    *
    */
   public void run()
   {
      try
      {
         printName("jnp://localhost/");
         printName("jnp://localhost:1099/");
         printName("jnp://localhost:1099/root");
         printName("jnp://localhost");
         printName("jnp:/localhost/");
         printName("jnp:localhost/");
         
         // Locate naming service (environment/setup is provided through the jndi.properties file)
         InitialContext iniCtx = new InitialContext(); 
         Context ctx = iniCtx;

         // Lookup the java: context
         Context java = (Context) iniCtx.lookup("java:");
         System.out.println("java: "+java);

         // Create subcontext
         Context test = ctx.createSubcontext("test");
         System.out.println("test created:"+test);
         
         // Create objects
         Object hello1 = "Hello1";
         System.out.println(hello1);
         Object hello2 = "Hello2";
         System.out.println(hello2);
         
         // Bind object
         ctx.bind("/test/server", hello1);
         System.out.println("test/server bound");
         
         // Bind object
         test.bind("server2", hello2);
         System.out.println("test/server2 bound");

         // Lookup object
         Object server = ctx.lookup("test/server2");
         System.out.println("test/server2 lookup:"+server);
         server = ctx.lookup("jnp://localhost/test/server2");
         System.out.println("jnp://localhost/test/server2 lookup:"+server);
         
         // Lookup object
         test = (Context)ctx.lookup("test");
         Object server2 = test.lookup("server");
         
         // Rebind object
         iniCtx.rebind("test/server2", hello2);
         System.out.println("test/server2 rebound");

         showTree(ctx);
         
         // Rename object using absolute and relative names
         test.rename("/test/server2", "server3");
         System.out.println("test/server2 renamed to test/server3");

         // Lookup object
         try
         {
            test.lookup("server2");
         } catch (NameNotFoundException e)
         {
            System.out.println("Server2 was not found (which is OK)");
         }
         
         Object server3 = test.lookup("server3");
         System.out.println("Server3:"+server3);

         // Print tree
         showTree(ctx);
         
         // Try URL context factory
         ctx = (Context) iniCtx.lookup("jnp://localhost/");
         System.out.println("Looked up URL context");
         
         showTree(ctx);

         // Try complete URL
         System.out.println("Looked up using URL: " +iniCtx.lookup("jnp://localhost:1099/test/server3"));

         // Bind using complete URL
         iniCtx.bind("jnp://localhost/helloserver",hello2);
         System.out.println("Bound helloserver");
         
         // Rename using URL
         iniCtx.rename("helloserver","jnp://localhost/test/helloserver");
         System.out.println("Renamed helloserver to test/helloserver");
         
         // Bind to root using absolute and relative names

         test.bind("/helloserver2",test.lookup("helloserver"));
         System.out.println("Bound test/helloserver to /helloserver2");
            
         // Create LinkRef
         test.bind("/helloserver3", new LinkRef("/test/server3"));
         test.bind("helloserver4", new LinkRef("server3"));
         System.out.println("test/helloserver3="+ctx.lookup("helloserver3"));
         
         // Create LinkRef to context
         ctx.createSubcontext("test2");
         ctx.bind("test2/helloworld", ctx.lookup("test/server3"));
         test.bind("test2link", new LinkRef("/test2"));
         System.out.println("test2/helloworld="+ctx.lookup("test2/helloworld"));
         System.out.println("test/test2link/helloworld="+ctx.lookup("test/test2link/helloworld"));
         
         // Show root context using listBindings
         System.out.println();
         System.out.println("Show root bindings");
         ctx = iniCtx;
         NamingEnumeration enum = ctx.listBindings("");
         while (enum.hasMoreElements())
         {
            Binding b = (Binding)enum.next();
            System.out.println(b);
         }
         
         showTree(ctx);

         // Test a URL Reference to a filesystem context
         StringRefAddr addr = new StringRefAddr("URL", "file:/tmp");
         Reference fsRef = new Reference("javax.naming.Context", addr);
         ctx.bind("external", fsRef);
         Context tmpfs = (Context) ctx.lookup("external");
         System.out.println("+++ tmp filesystem context:");
         showTree(tmpfs, 2);

         // Create an initial context that is rooted at /test
         Properties env = new Properties();
         env.setProperty(Context.INITIAL_CONTEXT_FACTORY, "org.jnp.interfaces.NamingContextFactory");
         env.setProperty(Context.URL_PKG_PREFIXES, "org.jnp.interfaces");
         env.setProperty(Context.PROVIDER_URL, "jnp://localhost/test");
         System.out.println("+++ Test jnp URL passed as PROVIDER_URL");
         ctx = new InitialContext(env);
         server = ctx.lookup("server");
         System.out.println("+ PROVIDER_URL=jnp://localhost/test lookup(server):"+server);
         env.setProperty(Context.PROVIDER_URL, "jnp://localhost:1099/test");
         ctx = new InitialContext(env);
         server = ctx.lookup("server");
         System.out.println("+ PROVIDER_URL=jnp://localhost:1099/test lookup(server):"+server);
         env.setProperty(Context.PROVIDER_URL, "jnp://localhost");
         ctx = new InitialContext(env);
         server = ctx.lookup("test/server");
         System.out.println("+ PROVIDER_URL=jnp://localhost lookup(test/server):"+server);
         env.setProperty(Context.PROVIDER_URL, "jnp://localhost:1099/");
         ctx = new InitialContext(env);
         server = ctx.lookup("test/server");
         System.out.println("+ PROVIDER_URL=jnp://localhost:1099/ lookup(test/server):"+server);

         // Test accessing a remote by accessing a non-default local server
         runRemoteServer();
         System.out.println("+++ Started second jnp server on port 10099");
         test = (Context) iniCtx.lookup("test");
         showTree(test);

         env = new Properties();
         env.setProperty(Context.INITIAL_CONTEXT_FACTORY, "org.jnp.interfaces.NamingContextFactory");
         env.setProperty(Context.URL_PKG_PREFIXES, "org.jnp.interfaces");
         ctx = (Context) new InitialContext(env).lookup("jnp://localhost:10099/");
         System.out.println(ctx.getEnvironment());
         // Create subcontext
         test = ctx.createSubcontext("test2");
         System.out.println("10099 test2 created:"+test);
         System.out.println("10099 test2.env:"+test.getEnvironment());
         test.bind("external", new LinkRef("jnp://localhost:1099/test"));
         Context external = (Context) new InitialContext(env).lookup("jnp://localhost:10099/test2/external");
         System.out.println("jnp://localhost:10099/test2 = "+external);
         System.out.println("jnp://localhost:10099/test2.env = "+external.getEnvironment());
         remoteServer.stop();
      }
      catch (Exception e)
      {
         e.printStackTrace(System.err);
      } 
   }

   private void runRemoteServer() throws Exception
   {
       remoteServer = new org.jnp.server.Main();
       remoteServer.setPort(10099);
       remoteServer.start();
   }

    /**
    *   Print the contents of a JNDI context recursively
    *
    * @param   indent  indentation string
    * @param   ctx  the JNDI context
    * @exception   NamingException  thrown if any problems occur
    */
    private void showTree(String indent, Context ctx, int depth, int maxDepth)
        throws NamingException
    {
        if( depth == maxDepth )
            return;
        NamingEnumeration enum = ctx.list("");
        while (enum.hasMoreElements())
        {
            NameClassPair ncp = (NameClassPair)enum.next();
            System.out.println(indent+ncp);
            if (ncp.getClassName().indexOf("Context") != -1)
               showTree(indent+ncp.getName()+"/", (Context)ctx.lookup(ncp.getName()), depth+1, maxDepth);
        }
    }

   // Inner classes -------------------------------------------------
}
