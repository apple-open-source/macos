/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.varia.process;

import java.util.Properties;
import java.util.Iterator;

import java.io.File;
import java.io.InputStream;
import java.io.Reader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.BufferedReader;

import org.apache.log4j.Level;
import org.apache.log4j.Logger;

import org.jboss.system.ServiceMBeanSupport;

import org.jboss.util.NullArgumentException;

/**
 * A service to manage a child process.
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 *
 * @version <tt>$Revision: 1.2 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class ChildProcessService
   extends ServiceMBeanSupport
   implements ChildProcessServiceMBean
{
   /** The command line of the process to execute. */
   protected String commandLine;

   /** The environment for the process. */
   protected Properties env;

   /** The working directory of the process. */
   protected File workingDir;

   /** The child, we are so proud. */
   protected Process childProcess;

   /**
    * The name of the logger adapter for the child process' streams.
    */
   protected String loggerAdapterName = this.getClass().getName();
   
   /**
    * The input adapter, which takes the process' STDOUT and
    * turns them into logger calls.
    */
   protected ReaderLoggerAdapter inputAdapter;

   /**
    * The input adapter, which takes the process' STDERR and
    * turns them into logger calls.
    */
   protected ReaderLoggerAdapter errorAdapter;
   
   /**
    * @jmx:managed-attribute
    */
   public void setCommandLine(final String commandLine)
   {
      if (commandLine == null)
         throw new NullArgumentException("commandLine");
      
      this.commandLine = commandLine;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getCommandLine()
   {
      return commandLine;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setEnvironment(final Properties env)
   {
      this.env = env;
   }

   /**
    * @jmx:managed-attribute
    */
   public Properties getEnvironment()
   {
      return env;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setWorkingDirectory(final File dir)
   {
      // only check if not null
      if (dir != null) {
         if (dir.exists()) {
            if (!dir.isDirectory()) {
               throw new IllegalArgumentException
                  ("Directory argument does not point to a directory: " + dir);
            }
         }
      }

      this.workingDir = dir;
   }

   /**
    * @jmx:managed-attribute
    */
   public File getWorkingDirectory()
   {
      return workingDir;
   }

   /**
    * @jmx:managed-attribute
    */
   public Integer getExitValue()
   {
      if (childProcess != null) {
         return new Integer(childProcess.exitValue());
      }

      return null;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setLoggerAdapterName(final String name)
   {
      this.loggerAdapterName = name;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getLoggerAdapterName()
   {
      return loggerAdapterName;
   }
   
   protected String[] makeEnvArray(final Properties props)
   {
      if (props == null)
         return new String[0];
      
      String[] envArray = new String[props.keySet().size()];

      Iterator iter = props.keySet().iterator();
      int i = 0;
      while (iter.hasNext()) {
         String name = (String)iter.next();
         envArray[i++] = name + "=" + props.getProperty(name);
      }

      return envArray;
   }

   ///////////////////////////////////////////////////////////////////////////
   //                   Reader/InputStream Logger Adapter                   //
   ///////////////////////////////////////////////////////////////////////////

   protected static class ReaderLoggerAdapter
      implements Runnable
   {
      protected BufferedReader reader;
      protected boolean shutdown;
      protected Logger log;
      protected Level level;
      
      public ReaderLoggerAdapter(final Reader reader, final Logger log, final Level level)
      {
         if (reader instanceof BufferedReader) {
            this.reader = (BufferedReader)reader;
         }
         else {
            this.reader = new BufferedReader(reader);
         }

         this.log = log;
         this.level = level;
      }

      public ReaderLoggerAdapter(final InputStream input, final Logger log, final Level level)
      {
         this(new InputStreamReader(input), log, level);
      }

      public void shutdown()
      {
         shutdown = true;
      }

      public void run()
      {
         while (!shutdown) {
            try {
               String data = reader.readLine();
               if (data == null) {
                  try {
                     Thread.sleep(1000);
                  }
                  catch (InterruptedException ignore) {}
               }
               else {
                  log.log(level, data);
               }
            }
            catch (IOException e) {
               log.error("Failed to read data from reader", e);
            }
         }
      }
   }
   
   
   ///////////////////////////////////////////////////////////////////////////
   //                    ServiceMBeanSupport Overrides                      //
   ///////////////////////////////////////////////////////////////////////////

   protected void startService() throws Exception
   {
      Runtime rt = Runtime.getRuntime();

      childProcess = rt.exec(commandLine, makeEnvArray(env), workingDir);
      log.info("Spawned child process: " + commandLine);

      // hook up the processes output streams to logging
      Logger logger = Logger.getLogger(loggerAdapterName);
      
      InputStream input = childProcess.getInputStream();
      inputAdapter = new ReaderLoggerAdapter(input, logger, Level.INFO);
      new Thread(inputAdapter).start();
      
      InputStream error = childProcess.getErrorStream();
      errorAdapter = new ReaderLoggerAdapter(error, logger, Level.ERROR);
      new Thread(errorAdapter).start();
   }

   protected void stopService() throws Exception
   {
      childProcess.destroy();

      log.debug("Child process destroyed; waiting for process to exit");
      childProcess.waitFor();

      log.info("Child exited with code: " + getExitValue());

      inputAdapter.shutdown();
      errorAdapter.shutdown();
      
      childProcess = null;
      inputAdapter = null;
      errorAdapter = null;
   }
}
