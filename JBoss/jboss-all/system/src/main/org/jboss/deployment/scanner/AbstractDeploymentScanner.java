/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.deployment.scanner;

import javax.management.ObjectName;

import org.jboss.deployment.Deployer;
import org.jboss.logging.Logger;
import org.jboss.system.MissingAttributeException;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.util.MuBoolean;
import org.jboss.util.MuLong;
import org.jboss.util.NullArgumentException;
import org.jboss.mx.util.MBeanProxyExt;
import org.jboss.mx.util.MBeanProxyInstance;

/**
 * An abstract support class for implementing a deployment scanner.
 *
 * <p>Provides the implementation of period-based scanning, as well
 *    as Deployer integration.
 *
 * <p>Sub-classes only need to implement {@link DeploymentScanner#scan}.
 *
 * @version <tt>$Revision: 1.9.2.4 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author Scott.Stark@jboss.org
 */
public abstract class AbstractDeploymentScanner
   extends ServiceMBeanSupport
   implements DeploymentScanner, DeploymentScannerMBean
{
   /** The scan period in milliseconds */
   protected MuLong scanPeriod = new MuLong(5000);

   /** True if period based scanning is enabled. */
   protected MuBoolean scanEnabled = new MuBoolean(true);

   /** A proxy to the deployer we are using. */
   protected Deployer deployer;

   /** The scanner thread. */
   protected ScannerThread scannerThread;

   /** HACK: Shutdown hook to get around problems with system service shutdown ordering. */
   private Thread shutdownHook;
   

   /////////////////////////////////////////////////////////////////////////
   //                           DeploymentScanner                         //
   /////////////////////////////////////////////////////////////////////////

   public void setDeployer(final ObjectName deployerName)
   {
      if (deployerName == null)
         throw new NullArgumentException("deployerName");

      deployer = (Deployer)
         MBeanProxyExt.create(Deployer.class, deployerName, server);
   }

   public ObjectName getDeployer()
   {
      return ((MBeanProxyInstance)deployer).getMBeanProxyObjectName();
   }

   /**
    * Period must be >= 0.
    */
   public void setScanPeriod(final long period)
   {
      if (period < 0)
         throw new IllegalArgumentException("ScanPeriod must be >= 0; have: " + period);

      this.scanPeriod.set(period);
   }

   public long getScanPeriod()
   {
      return scanPeriod.longValue();
   }

   public void setScanEnabled(final boolean flag)
   {
      this.scanEnabled.set(flag);
   }

   public boolean isScanEnabled()
   {
      return scanEnabled.get();
   }

   /** This is here to work around a bug in the IBM vm that causes an
    * AbstractMethodError to be thrown when the ScannerThread calls scan.
    * @throws Exception
    */
   public abstract void scan() throws Exception;

   /////////////////////////////////////////////////////////////////////////
   //                           Scanner Thread                            //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Should use Timer/TimerTask instead?  This has some issues with
    * interaction with ScanEnabled attribute.  ScanEnabled works only
    * when starting/stopping.
    */
   public class ScannerThread
      extends Thread
   {
      /** We get our own logger. */
      protected Logger log = Logger.getLogger(ScannerThread.class);

      /** True if the scan loop should run. */
      protected boolean enabled;

      /** True if we are shutting down. */
      protected boolean shuttingDown;

      /** Lock/notify object. */
      protected Object lock = new Object();

      public ScannerThread(boolean enabled)
      {
         super("ScannerThread");

         this.enabled = enabled;
      }

      public void setEnabled(boolean enabled)
      {
         this.enabled = enabled;

         synchronized (lock)
         {
            lock.notifyAll();
         }

         if (log.isDebugEnabled())
         {
            log.debug("Notified that enabled: " + enabled);
         }
      }

      public void shutdown()
      {
         enabled = false;
         shuttingDown = true;

         synchronized (lock)
         {
            lock.notifyAll();
         }

         if (log.isDebugEnabled())
         {
            log.debug("Notified to shutdown");
         }

         // jason: shall we also interrupt this thread?
      }
    
      public void run()
      {
         log.info("Running");

         while (!shuttingDown)
         {

            // If we are not enabled, then wait
            if (!enabled)
            {
               try
               {
                  log.debug("Disabled, waiting for notification");
                  synchronized (lock)
                  {
                     lock.wait();
                  }
               }
               catch (InterruptedException ignore) {}
            }

            loop();
         }

         log.info("Shutdown");
      }

      public void doScan()
      {
         // Scan for new/removed/changed/whatever
         try {
            scan();
         }
         catch (Exception e) {
            log.error("Scanning failed; continuing", e);
         }
      }
      
      protected void loop()
      {
         while (enabled)
         {
            doScan();

            // Sleep for scan period
            try
            {
               log.trace("Sleeping...");
               Thread.sleep(scanPeriod.longValue());
            }
            catch (InterruptedException ignore) {}
         }
      }
   }


   /////////////////////////////////////////////////////////////////////////
   //                     Service/ServiceMBeanSupport                     //
   /////////////////////////////////////////////////////////////////////////

   protected void createService() throws Exception
   {
      if (deployer == null)
         throw new MissingAttributeException("Deployer");

      // setup + start scanner thread
      scannerThread = new ScannerThread(false);
      scannerThread.setDaemon(true);
      scannerThread.start();
      log.debug("Scanner thread started");

      // HACK
      // 
      // install a shutdown hook, as the current system service shutdown
      // mechanism will not call this until all other services have stopped.
      // we need to know soon, so we can stop scanning to try to avoid
      // starting new services when shutting down

      final ScannerThread _scannerThread = scannerThread;
      shutdownHook = new Thread("DeploymentScanner Shutdown Hook")
         {
            ScannerThread scannerThread = _scannerThread;
            
            public void run()
            {
               scannerThread.shutdown();
            }
         };
      
      try
      {
         Runtime.getRuntime().addShutdownHook(shutdownHook);
      }
      catch (Exception e)
      {
         log.warn("Failed to add shutdown hook", e);
      }
   }

   protected void startService() throws Exception 
   {
      synchronized( scannerThread )
      {
         // scan before we enable the thread, so JBoss version shows up afterwards
         scannerThread.doScan();

         // enable scanner thread if we are enabled
         scannerThread.setEnabled(scanEnabled.get());
      }
   }
   
   protected void stopService() throws Exception 
   {
      // disable scanner thread
      if( scannerThread != null )
         scannerThread.setEnabled(false);
   }

   protected void destroyService() throws Exception 
   {
      // drop our ref to deployer, so scan will fail
      deployer = null;

      // shutdown scanner thread
      if( scannerThread != null )
      {
         synchronized( scannerThread )
         {
            scannerThread.shutdown();
         }
      }

      // HACK
      // 
      // remove the shutdown hook, we don't need it anymore
      try
      {
         Runtime.getRuntime().removeShutdownHook(shutdownHook);
      }
      catch (Exception ignore)
      {
      } // who cares really

      // help gc
      shutdownHook = null;
      scannerThread = null;
   }
}
