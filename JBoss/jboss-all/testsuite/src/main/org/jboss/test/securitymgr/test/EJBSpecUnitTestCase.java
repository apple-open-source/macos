/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.securitymgr.test;

import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import javax.naming.InitialContext;

import org.jboss.test.securitymgr.interfaces.IOSession;
import org.jboss.test.securitymgr.interfaces.IOSessionHome;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.test.JBossTestCase;

/** Tests of the programming restrictions defined by the EJB spec. The JBoss
server must be running under a security manager. The securitymgr-ejb.jar
should be granted only the following permission:

grant securitymgr-ejb.jar {
   permission java.util.PropertyPermission "*", "read";
   permission java.lang.RuntimePermission "queuePrintJob";
   permission java.net.SocketPermission "*", "connect";
 };

@author Scott.Stark@jboss.org
@version $Revision: 1.3 $
 */
public class EJBSpecUnitTestCase
   extends JBossTestCase
{

   public EJBSpecUnitTestCase(String name)
   {
      super(name);
   }

   /** Test that a bean cannot access the filesystem using java.io.File
    */
   public void testFileIO() throws Exception
   {
      log.debug("+++ testFileIO()");
      IOSession bean = getIOSession();

      try
      {
         // This should fail because the bean calls File.exists()
         bean.read("nofile.txt");
         doFail("Was able to call IOSession.read");
      }
      catch(Exception e)
      {
         log.debug("IOSession.read failed as expected", e);
      }

      try
      {
         // This should fail because the bean calls File.exists()
         bean.write("nofile.txt");
         doFail("Was able to call IOSession.write");
      }
      catch(Exception e)
      {
         log.debug("IOSession.write failed as expected", e);
      }
      bean.remove();
   }

   public void testSockets() throws Exception
   {
      log.debug("+++ testSockets()");
      IOSession bean = getIOSession();
      try
      {
         bean.listen(0);
         doFail("Was able to call IOSession.listen");
      }
      catch(Exception e)
      {
         log.debug("IOSession.listen failed as expected", e);
      }

      final ServerSocket tmp = new ServerSocket(0);
      log.debug("Created ServerSocket: "+tmp);
      Thread t = new Thread("Acceptor")
      {
         public void run()
         {
            try
            {
               Socket s = tmp.accept();
               log.debug("Accepted Socket: "+s);
               s.close();
               log.debug("ServerSocket thread exiting");
            }
            catch(IOException e)
            {
            }
         }
      };
      int port = tmp.getLocalPort();
      t.start();
      bean.connect("localhost", port);
      tmp.close();
      bean.remove();
   }

   public void testClassLoaders() throws Exception
   {
      log.debug("+++ testClassLoaders()");
      IOSession bean = getIOSession();
      try
      {
         bean.createClassLoader();
         doFail("Was able to call IOSession.createClassLoader");
      }
      catch(Exception e)
      {
         log.debug("IOSession.createClassLoader failed as expected", e);
      }

      try
      {
         bean.getContextClassLoader();
         //doFail("Was able to call IOSession.getContextClassLoader");
         log.debug("Was able to call IOSession.getContextClassLoader");
      }
      catch(Exception e)
      {
         log.debug("IOSession.getContextClassLoader failed as expected", e);
      }

      try
      {
         bean.setContextClassLoader();
         doFail("Was able to call IOSession.setContextClassLoader");
      }
      catch(Exception e)
      {
         log.debug("IOSession.setContextClassLoader failed as expected", e);
      }
      bean.remove();
   }

   public void testReflection() throws Exception
   {
      log.debug("+++ testReflection()");
      IOSession bean = getIOSession();
      try
      {
         bean.useReflection();
         doFail("Was able to call IOSession.useReflection");
      }
      catch(Exception e)
      {
         log.debug("IOSession.useReflection failed as expected", e);
      }
      bean.remove();
   }

   public void testThreadAccess() throws Exception
   {
      log.debug("+++ testThreadAccess()");
      IOSession bean = getIOSession();
      try
      {
         // This test will fail because the calling thread it not in the root thread group
         bean.renameThread();
         doFail("Was able to call IOSession.renameThread");
      }
      catch(Exception e)
      {
         log.debug("IOSession.renameThread failed as expected", e);
      }
      bean.remove();
   }

   public void testSystemAccess() throws Exception
   {
      log.debug("+++ testSystemAccess()");
      IOSession bean = getIOSession();
      try
      {
         bean.createSecurityMgr();
         doFail("Was able to call IOSession.createSecurityMgr");
      }
      catch(Exception e)
      {
         log.debug("IOSession.createSecurityMgr failed as expected", e);
      }

      try
      {
         bean.createSecurityMgr();
         doFail("Was able to call IOSession.changeSystemOut");
      }
      catch(Exception e)
      {
         log.debug("IOSession.changeSystemOut failed as expected", e);
      }

      try
      {
         bean.changeSystemErr();
         doFail("Was able to call IOSession.changeSystemErr");
      }
      catch(Exception e)
      {
         log.debug("IOSession.changeSystemErr failed as expected", e);
      }

      try
      {
         bean.loadLibrary();
         doFail("Was able to call IOSession.loadLibrary");
      }
      catch(Exception e)
      {
         log.debug("IOSession.loadLibrary failed as expected", e);
      }

      try
      {
         bean.systemExit(1);
         doFail("Was able to call IOSession.systemExit");
      }
      catch(Exception e)
      {
         log.debug("IOSession.systemExit failed as expected", e);
      }
      bean.remove();
   }

   /**
    * Setup the test suite.
    */
   public static Test suite() throws Exception
   {
      return getDeploySetup(EJBSpecUnitTestCase.class, "securitymgr-ejb.jar");
   }

   private IOSession getIOSession() throws Exception
   {
      Object obj = getInitialContext().lookup("secmgr.IOSessionHome");
      IOSessionHome home = (IOSessionHome) obj;
      log.debug("Found secmgr.IOSessionHome");
      IOSession bean = home.create();
      log.debug("Created IOSession");
      return bean;
   }

   private void doFail(String msg)
   {
      log.error(msg);
      fail(msg);
   }
   private void doFail(String msg, Throwable t)
   {
      log.error(msg, t);
      fail(msg);
   }
}
