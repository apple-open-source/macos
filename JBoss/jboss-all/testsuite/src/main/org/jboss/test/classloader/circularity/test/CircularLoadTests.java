package org.jboss.test.classloader.circularity.test;

import java.lang.reflect.Method;
import java.lang.reflect.Constructor;
import java.lang.reflect.UndeclaredThrowableException;
import java.net.URL;

import org.apache.log4j.ConsoleAppender;
import org.apache.log4j.Logger;
import org.apache.log4j.PatternLayout;

import org.jboss.mx.loading.UnifiedClassLoader;
import org.jboss.mx.loading.UnifiedClassLoader3;
import org.jboss.mx.loading.UnifiedLoaderRepository3;

import EDU.oswego.cs.dl.util.concurrent.CyclicBarrier;

/** For the <C, UCL> and C^UCL notation used in the comments refer to the
 * paper "Dynamic Class Loading in the Java Virtual Machine", by Sheng
 * Liang and Gilad Bracha. I found a pdf version here:
 * http://www.cs.purdue.edu/homes/jv/smc/pubs/liang-oopsla98.pdf
 * You can get a postscript version from here:
 * http://java.sun.com/people/sl/papers/oopsla98.ps.gz
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.9 $
 */
public class CircularLoadTests
{
   private static Logger log = Logger.getLogger(CircularLoadTests.class);

   private CyclicBarrier setupBarrier = new CyclicBarrier(3);

   public CircularLoadTests()
   {
   }

   /** Test for LinkageError loading constraint violations.
    Given a UnifiedLoaderRepository3 with 3 class loaders:
    UCL0 uses le0.jar contains Base and UserOfBase
    UCL1 uses le1.jar contains Base and Support
    UCL2 this service sar

    1. Load the Base class via UCL2. This has the potential for causing
    <Base, UCL0> and <Base, UCL1> to be defined since both UCL0 and UCL1
    contain this class.
    2. Load the UserOfBase class via UCL0 to define <UserOfBase, UCL0>
    3. Load the Support class via UCL1 to define <Support, UCL1>
    4. Invoke testBase:
    class <UserOfBase, UCL0>
    {
      public void testBase(<Support, UCL1> s)
      {
         Base b = s.getBase(); -> Base^UCL0 = Base^UCL1
      ...
    }
    to trigger the loading constraint that Base as loaded by UCL0 and
    UCL1 must be the same class.
    */
   public void testLinkageError() throws Exception
   {
      log.info("Begin testLinkageError");
      UnifiedLoaderRepository3 repository = new UnifiedLoaderRepository3();
      Class thisClass = getClass();
      UnifiedClassLoader thisUCL = (UnifiedClassLoader) thisClass.getClassLoader();
      URL origURL = thisUCL.getOrigURL();
      log.info("Service origURL="+origURL);
      URL j0 = new URL(origURL, "le0.jar");
      log.info("j0 = "+j0);
      URL j1 = new URL(origURL, "le1.jar");
      log.info("j1 = "+j1);
      final UnifiedClassLoader3 ucl0 = new UnifiedClassLoader3(j0);
      final UnifiedClassLoader3 ucl1 = new UnifiedClassLoader3(j1);
      final UnifiedClassLoader3 ucl2 = new UnifiedClassLoader3(origURL);
      repository.addClassLoader(ucl0);
      repository.addClassLoader(ucl1);
      repository.addClassLoader(ucl2);

      try
      {
         // Load Base
         Class baseClass = ucl2.loadClass("org.jboss.test.classloader.circularity.support.Base");
         log.info("Base.CS: "+baseClass.getClass().getProtectionDomain().getCodeSource());

         // Load and create an instance of the UserOfBase class
         Class userOfBaseClass = ucl0.loadClass("org.jboss.test.classloader.circularity.support.UserOfBase");
         Class[] noSig = {};
         Constructor ctor0 = userOfBaseClass.getConstructor(noSig);
         Object[] noArgs = {};
         Object userOfBase = ctor0.newInstance(noArgs);
         log.info("UserOfBase.CS: "+userOfBase.getClass().getProtectionDomain().getCodeSource());

         // Load and create an instance of the Support class
         Class supportClass = ucl1.loadClass("org.jboss.test.classloader.circularity.support.Support");
         Constructor ctor1 = supportClass.getConstructor(noSig);
         Object support = ctor1.newInstance(noArgs);
         log.info("Support.CS: "+support.getClass().getProtectionDomain().getCodeSource());

         // Now invoke UserOfBase.testBase(Support)
         Class[] sig = {supportClass};
         Method testBase = userOfBaseClass.getMethod("testBase", sig);
         log.info(testBase.toString());
         Object[] args = {support};
         testBase.invoke(userOfBase, args);
      }
      catch(Exception e)
      {
         log.error("Failed", e);
         throw e;
      }
      catch(Throwable e)
      {
         log.error("Failed", e);
         throw new UndeclaredThrowableException(e);
      }
      log.info("End testLinkageError");
   }

   /** Test for IllegalAccessError violations due to pkg classes split across jars
    Given a UnifiedLoaderRepository3 with two class loaders:
    UCL0 uses login.jar which contains UserOfLoginInfo
    UCL1 uses usrmgr.jar which contains UserOfUsrMgr

    Both the login.jar and usrmgr.jar reference a cl-util.jar via manifest
    Class-Path entries. The cl-util.jar contains LoginInfo and UsrMgr.
    */
   public void testPackageProtected() throws Exception
   {
      log.info("Begin testPackageProtected");
      UnifiedLoaderRepository3 repository = new UnifiedLoaderRepository3();
      Class thisClass = getClass();
      UnifiedClassLoader thisUCL = (UnifiedClassLoader) thisClass.getClassLoader();
      URL origURL = thisUCL.getOrigURL();
      log.info("Service origURL="+origURL);
      URL j0 = new URL(origURL, "login.jar");
      log.info("j0 = "+j0);
      URL j1 = new URL(origURL, "usrmgr.jar");
      log.info("j1 = "+j1);
      URL util = new URL(origURL, "cl-util.jar");
      log.info("util = "+j1);
      final UnifiedClassLoader3 libs = new UnifiedClassLoader3(origURL);
      final UnifiedClassLoader3 ucl0 = libs;
      final UnifiedClassLoader3 ucl1 = libs;
      repository.addClassLoader(libs);
      libs.addURL(util);
      libs.addURL(j0);
      libs.addURL(j1);

      try
      {
      // Load and create an instance of the UserOfLoginInfo class
      Class c0 = ucl0.loadClass("org.jboss.test.classloader.circularity.support.UserOfLoginInfo");
      Class[] ctorsig0 = {String.class, String.class};
      Constructor ctor0 = c0.getConstructor(ctorsig0);
      Object[] args0 = {"jduke", "theduke"};
      Object o0 = ctor0.newInstance(args0);
      log.info("UserOfLoginInfo.CS: "+o0.getClass().getProtectionDomain().getCodeSource());

      // Load and create an instance of the UserOfUsrMgr class
      Class c1 = ucl1.loadClass("org.jboss.test.classloader.circularity.support.UserOfUsrMgr");
      Class[] ctorsig1 = {String.class, String.class};
      Constructor ctor1 = c1.getConstructor(ctorsig1);
      Object[] args1 = {"jduke", "theduke"};
      Object o1 = ctor1.newInstance(args1);
      log.info("UserOfUsrMgr.CS: "+o1.getClass().getProtectionDomain().getCodeSource());

      // Now invoke UserOfUsrMgr.changePassword(char[] password)
         char[] password = "theduke2".toCharArray();
         Class[] sig = {password.getClass()};
         Method changePassword = c1.getMethod("changePassword", sig);
         log.info(changePassword.toString());
         Object[] args = {password};
         changePassword.invoke(o1, args);
      }
      catch(Exception e)
      {
         log.error("Failed", e);
         throw e;
      }
      log.info("End testPackageProtected");
   }

   /** Given a UnifiedLoaderRepository3 with two class loaders:
    UCL0 uses any0.jar which contains Base, Class0, Class2
    UCL1 uses any1.jar which contains Class0, Class2
    create a thread to load Derived using UCL0.
    */
   public void testDuplicateClass() throws Exception
   {
      log.info("Begin testDuplicateClass");
      UnifiedLoaderRepository3 repository = new UnifiedLoaderRepository3();
      Class thisClass = getClass();
      UnifiedClassLoader thisUCL = (UnifiedClassLoader) thisClass.getClassLoader();
      URL origURL = thisUCL.getOrigURL();
      log.info("Service origURL="+origURL);
      URL j0 = new URL(origURL, "any0.jar");
      log.info("j0 = "+j0);
      URL j1 = new URL(origURL, "any1.jar");
      log.info("j1 = "+j1);
      final UnifiedClassLoader3 ucl0 = new UnifiedClassLoader3(j0);
      final UnifiedClassLoader3 ucl1 = new UnifiedClassLoader3(j1);
      repository.addClassLoader(ucl0);
      repository.addClassLoader(ucl1);

      Class c0 = ucl0.loadClass("org.jboss.test.classloader.circularity.support.Class0");
      log.info("Class0.CS: "+c0.getProtectionDomain().getCodeSource());
      Class c2 = ucl1.loadClass("org.jboss.test.classloader.circularity.support.Class2");
      log.info("Class2.CS: "+c2.getProtectionDomain().getCodeSource());
      Class base = ucl0.loadClass("org.jboss.test.classloader.circularity.support.Base");
      Class[] sig = {};
      Method run = base.getMethod("run", sig);
      Object[] empty = {};
      run.invoke(null, empty);
      log.info("End testDuplicateClass");
   }

   /** Given a UnifiedLoaderRepository3 with three class loaders:
    UCL0 uses j0.jar which contains Class0
    UCL1 uses j2.jar which contains Class2
    Request a class org.jboss.test.classloader.circularity.supportx.Class2
    using T0 assign ownership of UCL0 to T0, and then load Class2 using
    UCL1 and T1 to validate that a load task is not assigned to T0 since
    is should no longer own a UCL with the org.jboss.test.classloader.circularity.support
    package.
    */
   public void testUCLOwner() throws Exception
   {
      UnifiedLoaderRepository3 repository = new UnifiedLoaderRepository3();
      Class thisClass = getClass();
      UnifiedClassLoader thisUCL = (UnifiedClassLoader) thisClass.getClassLoader();
      URL origURL = thisUCL.getOrigURL();
      log.info("Service origURL="+origURL);
      URL j0 = new URL(origURL, "j0.jar");
      log.info("j0 = "+j0);
      URL j1 = new URL(origURL, "j2.jar");
      log.info("j1 = "+j1);
      final UnifiedClassLoader3 ucl0 = new UnifiedClassLoader3(j0);
      final UnifiedClassLoader3 ucl1 = new UnifiedClassLoader3(j1);
      repository.addClassLoader(ucl0);
      repository.addClassLoader(ucl1);

      // Request a class in a package that does not exist
      LoadThread t0 = new LoadThread("org.jboss.test.classloader.circularity.supportx.Class2",
         ucl0, "testUCLOwner.T0");
      t0.start();
      // Join the thread
      t0.join(5000);
      if( t0.loadedClass != null || t0.loadError == null )
      {
         log.error("T0 failed as no class should have been found, loadedClass="+t0.loadedClass);
         throw new IllegalStateException("T0 failed as no class should have been found");
      }

      LoadThread t1 = new LoadThread("org.jboss.test.classloader.circularity.support.Class2",
         ucl1, "testUCLOwner.T1");
      t1.start();
      // Join the thread
      t1.join(5000);
      if( t1.loadedClass == null || t1.loadError != null )
      {
         log.error("T1 failed to load Class2", t1.loadError);
         throw new IllegalStateException("T1 failed to load Class2");
      }
   }

   /** Given a UnifiedLoaderRepository3 with three class loaders:
    UCL0 uses j0.jar which contains Class0
    UCL1 uses j4.jar which contains Derived, but not Base
    create a thread to load Derived using UCL0.
    */
   public void testMissingSuperClass() throws Exception
   {
      UnifiedLoaderRepository3 repository = new UnifiedLoaderRepository3();
      Class thisClass = getClass();
      UnifiedClassLoader thisUCL = (UnifiedClassLoader) thisClass.getClassLoader();
      URL origURL = thisUCL.getOrigURL();
      log.info("Service origURL="+origURL);
      URL j0 = new URL(origURL, "j0.jar");
      log.info("j0 = "+j0);
      URL j3 = new URL(origURL, "j3.jar");
      log.info("j3 = "+j3);
      final UnifiedClassLoader3 ucl0 = new UnifiedClassLoader3(j0);
      final UnifiedClassLoader3 ucl1 = new UnifiedClassLoader3(j3);
      repository.addClassLoader(ucl0);
      repository.addClassLoader(ucl1);

      LoadThread t0 = new LoadThread("org.jboss.test.classloader.circularity.support.Derived",
         ucl0, "testMissingSuperClass.T0");
      t0.start();
      // Join the thread
      t0.join(5000);
      if( t0.loadedClass != null || t0.loadError == null )
      {
         log.error("T0 failed as no class should have been found");
         throw new IllegalStateException("T0 failed as no class should have been found");
      }
      log.debug("Load of Derivied failed as expected", t0.loadError);
   }

   /** Given a UnifiedLoaderRepository3 with three class loaders:
    UCL0 uses j0.jar which contains Class0
    UCL1 uses j1.jar which contains Class1
    UCL2 uses j2.jar which contains Class2

    creates 3 threads:
    T0 uses UCL0 to load Class2
    T1 uses UCL1 to load Class0
    T2 uses UCL2 to load Class1
    */
   public void testLoading() throws Exception
   {
      UnifiedLoaderRepository3 repository = new UnifiedLoaderRepository3();
      Class thisClass = getClass();
      UnifiedClassLoader thisUCL = (UnifiedClassLoader) thisClass.getClassLoader();
      URL origURL = thisUCL.getOrigURL();
      log.info("Service origURL="+origURL);
      URL j0 = new URL(origURL, "j0.jar");
      log.info("j0 = "+j0);
      URL j1 = new URL(origURL, "j1.jar");
      log.info("j1 = "+j1);
      URL j2 = new URL(origURL, "j2.jar");
      log.info("j2 = "+j2);
      final UnifiedLoader ucl0 = new UnifiedLoader(j0);
      final UnifiedLoader ucl1 = new UnifiedLoader(j1);
      final UnifiedLoader ucl2 = new UnifiedLoader(j2);
      repository.addClassLoader(ucl0);
      repository.addClassLoader(ucl1);
      repository.addClassLoader(ucl2);

      LoadThread t0 = new LoadThread("org.jboss.test.classloader.circularity.support.Class2",
         ucl0, "testLoading.T0");
      LoadThread t1 = new LoadThread("org.jboss.test.classloader.circularity.support.Class0",
         ucl1, "testLoading.T1");
      LoadThread t2 = new LoadThread("org.jboss.test.classloader.circularity.support.Class1",
         ucl2, "testLoading.T2");
      t0.start();
      t1.start();
      t2.start();
      // Join the threads
      boolean ok = true;
      t0.join(5000);
      if( t0.loadedClass == null || t0.loadError != null )
      {
         log.error("T0 failed", t0.loadError);
         ok = false;
      }
      t1.join(5000);
      if( t1.loadedClass == null || t1.loadError != null )
      {
         log.error("T1 failed", t1.loadError);
         ok = false;
      }
      t2.join(5000);
      if( t2.loadedClass == null || t2.loadError != null )
      {
         log.error("T2 failed", t2.loadError);
         ok = false;
      }
      if( ok == false )
         throw new IllegalStateException("Failed to load Class0..Class2");
   }

   static class LoadThread extends Thread
   {
      String classname;
      ClassLoader loader;
      Class loadedClass;
      Throwable loadError;

      LoadThread(String classname, ClassLoader loader, String name)
      {
         super(name);
         this.classname = classname;
         this.loader = loader;
      }

      public void run()
      {
         try
         {
            loadedClass = loader.loadClass(classname);
         }
         catch(Throwable t)
         {
            loadError = t;
         }
      }
   }

   public class UnifiedLoader extends UnifiedClassLoader3
   {
      private boolean enteredBarrier;
      public UnifiedLoader(URL url)
      {
         super(url);
      }

      public synchronized Class loadClass(String name, boolean resolve)
         throws ClassNotFoundException
      {
         try
         {
            // Get the UCLs all locked up before starting
            if( enteredBarrier == false )
               setupBarrier.barrier();
            enteredBarrier = true; 
         }
         catch(InterruptedException e)
         {
            throw new ClassNotFoundException("Failed due to InterruptedException");
         }
         return super.loadClass(name, false);
      }

   }
}
