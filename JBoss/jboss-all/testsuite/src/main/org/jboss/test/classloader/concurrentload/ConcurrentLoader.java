/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.classloader.concurrentload;

import java.util.HashSet;
import java.util.Timer;
import java.util.TimerTask;
import java.util.Vector;

import org.jboss.logging.Logger;
import org.jboss.system.Service;
import org.jboss.system.ServiceMBeanSupport;


/** A multi-threaded class loading test service.
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2 
 */
public class ConcurrentLoader
       extends ServiceMBeanSupport
       implements ConcurrentLoaderMBean
{

   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   public Object lock = new Object ();

   public final static int MAX_CLASSES = 10;
   public final static int NUMBER_OF_LOADING = 10;
   public final static int NUMBER_OF_THREADS = 20;
   private HashSet classes = new HashSet();
   private Vector ungarbaged = new Vector();
   private Timer newInstanceTimer;
   private int doneCount;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   public ConcurrentLoader ()
   {
   }

   // Public --------------------------------------------------------

   // Z implementation ----------------------------------------------

   // ServiceMBeanSupport overrides ---------------------------------------------------

   protected void createService() throws Exception
   {
      log.debug("Creating "+NUMBER_OF_THREADS+" threads...");
      newInstanceTimer = new Timer(true);
      newInstanceTimer.scheduleAtFixedRate(new NewInstanceTask(), 0, 100);
      doneCount = 0;
      for(int t = 0; t < NUMBER_OF_THREADS; t ++)
      {
         ConcurrentLoader.Loader loader = new ConcurrentLoader.Loader (t);
         loader.start ();
         ungarbaged.add (loader);
      }
      log.info("All threads created");
      Thread.sleep(2000);

      synchronized (lock)
      {
         lock.notifyAll ();
      }
	   log.info("Unlocked all Loader threads");
      synchronized( lock )
      {
         while( doneCount < NUMBER_OF_THREADS )
         {
            lock.wait();
         }
         log.info("Loader doneCount="+doneCount);
      }
      log.info("All Loaders are done");
      newInstanceTimer.cancel();
   }

   protected void stopService() throws Exception
   {
      newInstanceTimer.cancel();
      classes.clear();
      ungarbaged.clear();
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------

   class NewInstanceTask extends TimerTask
   {
      Logger theLog;
      NewInstanceTask()
      {
         this.theLog = ConcurrentLoader.this.getLog();
      }

      /** Create an instance of a class and exit
       */
      public void run()
      {
         int size = classes.size();
         Class[] theClasses = new Class[size];
         classes.toArray(theClasses);
         theLog.info("NewInstanceTask, creating "+size+" instances");
         for(int c = 0; c < theClasses.length; c ++)
         {
            try
            {
               Class clazz = theClasses[c];
               Object obj = clazz.newInstance();
               theLog.debug("Created instance="+obj);
            }
            catch(Throwable t)
            {
               t.printStackTrace();
            }
         }
      }
   }

   class Loader extends Thread
   {
      int classid = 0;
      Logger theLog;

      public Loader (int classid)
      {
         super("ConcurrentLoader - Thread #" + classid);
         this.classid = classid;
         this.theLog = ConcurrentLoader.this.getLog();
      }

      public void run ()
      {
         int modId = classid % MAX_CLASSES;
         String className = this.getClass ().getPackage ().getName () + ".Anyclass" + modId;
         ClassLoader cl = this.getContextClassLoader ();

         synchronized (lock)
         {
            try
            {
               theLog.debug("Thread ready: " + classid);
               lock.wait ();
            }
            catch (Exception e)
            {
               theLog.error("Error during wait", e);
            }
         }
         theLog.debug("loading class... " + className);
         for (int i=0; i<NUMBER_OF_LOADING; i++)
         {
            theLog.debug("loading class with id " + classid + " for the " + i + "th time");
            try
            {
               theLog.debug("before load...");
               long sleep = (long) (1000 * Math.random());
               Thread.sleep(sleep);
               Class clazz = cl.loadClass (className);
               classes.add(clazz);
               Object obj = null; //clazz.newInstance();
               theLog.debug("Class " + className + " loaded, obj="+obj);
            }
            catch (Throwable e)
            {
               theLog.debug("Failed to load class and create instance", e);
            }
         }
         theLog.debug("...Done loading classes. " + classid);
         synchronized( lock )
         {
            doneCount ++;
            lock.notify();
         }
      }
   }

}
