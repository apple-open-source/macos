/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.logging;

import org.apache.log4j.LogManager;
import org.apache.log4j.Level;
import org.apache.log4j.Category;
import org.apache.log4j.Priority;

/**
 * Delegate for org.jboss.logging.Logger logging to log4j. Body of implementation
 * mainly copied from old Logger implementation.
 *
 * @see org.jboss.logging.Logger
 * @see org.jboss.logging.LoggerPlugin
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>3. mai 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 * </p>
 * <p><b>4. february 2003 Dag Liodden:</b>
 * <ul>
 *   <li>Fixed Log4J locationinfo by sending the fully qualified classname of <code>Logger</code> to Log4J</li>
 * </ul>
 * </p>
 */

public class Log4jLoggerPlugin implements LoggerPlugin
{
   
   // Constants -----------------------------------------------------

   /** 
    *  Fully qualified classname for this class so Log4J locationinfo will be
    *  correct
    */
   private static final String FQCN = Logger.class.getName();
   
   // Attributes ----------------------------------------------------
   
   /** The Log4j delegate logger. */
   private transient org.apache.log4j.Logger log;

   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public Log4jLoggerPlugin () { }
   
   public void init (String name)
   {
      log = LogManager.getLogger(name);
   }
   
   // Public --------------------------------------------------------

   public Category getCategory()
   {
      return log;
   }

   /**
    * Exposes the delegate Log4j Logger.
    */
   public org.apache.log4j.Logger getLogger()
   {
      return log;
   }
   
   // LoggerPlugin implementation ----------------------------------------------
   
   public boolean isTraceEnabled()
   {      
      if (log.isEnabledFor(XLevel.TRACE) == false)
         return false;
      return XLevel.TRACE.isGreaterOrEqual(log.getEffectiveLevel());
   }

   /** 
    * Issue a log msg with a level of TRACE.
    * Invokes log.log(XLevel.TRACE, message);
    */
   public void trace(Object message)
   {
      log.log(FQCN, XLevel.TRACE, message, null);
   }

   /** 
    * Issue a log msg and throwable with a level of TRACE.
    * Invokes log.log(XLevel.TRACE, message, t);
    */
   public void trace(Object message, Throwable t)
   {
      log.log(FQCN, XLevel.TRACE, message, t);
   }

   /**
    * Check to see if the TRACE level is enabled for this logger.
    *
    * @return true if a {@link #trace(Object)} method invocation would pass
    * the msg to the configured appenders, false otherwise.
    */
   public boolean isDebugEnabled()
   {
      Level l = Level.DEBUG;
      if (log.isEnabledFor(l) == false)
         return false;
      return l.isGreaterOrEqual(log.getEffectiveLevel());
   }

   /** 
    * Issue a log msg with a level of DEBUG.
    * Invokes log.log(Level.DEBUG, message);
    */
   public void debug(Object message)
   {
      log.log(FQCN, Level.DEBUG, message, null);
   }

   /** 
    * Issue a log msg and throwable with a level of DEBUG.
    * Invokes log.log(Level.DEBUG, message, t);
    */
   public void debug(Object message, Throwable t)
   {
      log.log(FQCN, Level.DEBUG, message, t);
   }

   /** 
    * Check to see if the INFO level is enabled for this logger.
    *
    * @return true if a {@link #info(Object)} method invocation would pass
    * the msg to the configured appenders, false otherwise.
    */
   public boolean isInfoEnabled()
   {
      Level l = Level.INFO;
      if (log.isEnabledFor(l) == false)
         return false;
      return l.isGreaterOrEqual(log.getEffectiveLevel());
   }

   /** 
    * Issue a log msg with a level of INFO.
    * Invokes log.log(Level.INFO, message);
    */
   public void info(Object message)
   {
      log.log(FQCN, Level.INFO, message, null);
   }

   /**
    * Issue a log msg and throwable with a level of INFO.
    * Invokes log.log(Level.INFO, message, t);
    */
   public void info(Object message, Throwable t)
   {
      log.log(FQCN, Level.INFO, message, t);
   }

   /** 
    * Issue a log msg with a level of WARN.
    * Invokes log.log(Level.WARN, message);
    */
   public void warn(Object message)
   {
      log.log(FQCN, Level.WARN, message, null);
   }

   /** 
    * Issue a log msg and throwable with a level of WARN.
    * Invokes log.log(Level.WARN, message, t);
    */
   public void warn(Object message, Throwable t)
   {
      log.log(FQCN, Level.WARN, message, t);
   }

   /** 
    * Issue a log msg with a level of ERROR.
    * Invokes log.log(Level.ERROR, message);
    */
   public void error(Object message)
   {
      log.log(FQCN, Level.ERROR, message, null);
   }

   /** 
    * Issue a log msg and throwable with a level of ERROR.
    * Invokes log.log(Level.ERROR, message, t);
    */
   public void error(Object message, Throwable t)
   {
      log.log(FQCN, Level.ERROR, message, t);
   }

   /** 
    * Issue a log msg with a level of FATAL.
    * Invokes log.log(Level.FATAL, message);
    */
   public void fatal(Object message)
   {
      log.log(FQCN, Level.FATAL, message, null);
   }

   /** 
    * Issue a log msg and throwable with a level of FATAL.
    * Invokes log.log(Level.FATAL, message, t);
    */
   public void fatal(Object message, Throwable t)
   {
      log.log(FQCN, Level.FATAL, message, t);
   }

   /** 
    * Issue a log msg with the given level.
    * Invokes log.log(p, message);
    *
    * @deprecated  Use Level versions.
    */
   public void log(Priority p, Object message)
   {
      log.log(FQCN, p, message, null);
   }

   /** 
    * Issue a log msg with the given priority.
    * Invokes log.log(p, message, t);
    *
    * @deprecated  Use Level versions.
    */
   public void log(Priority p, Object message, Throwable t)
   {
      log.log(FQCN, p, message, t);
   }

   /** 
    * Issue a log msg with the given level.
    * Invokes log.log(l, message);
    */
   public void log(Level l, Object message)
   {
      log.log(FQCN, l, message, null);
   }

   /** 
    * Issue a log msg with the given level.
    * Invokes log.log(l, message, t);
    */
   public void log(Level l, Object message, Throwable t)
   {
      log.log(FQCN, l, message, t);
   }

   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
