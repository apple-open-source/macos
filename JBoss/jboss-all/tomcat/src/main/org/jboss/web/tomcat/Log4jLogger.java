/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat;

import java.beans.PropertyChangeSupport;
import java.beans.PropertyChangeListener;
import javax.servlet.ServletException;

import org.apache.catalina.Container;
import org.apache.catalina.LifecycleException;
import org.apache.log4j.Category;

import org.jboss.logging.Logger;

/** An adaptor from the org.apache.catalina.Logger to the log4j based logging
 used by JBoss.
 *
 * @author  Scott.Stark@jboss.org
 * @version $Revision: 1.1.1.1 $
 */
public class Log4jLogger implements org.apache.catalina.Logger
{
   /** The Container with which this Logger has been associated. */
   protected Container container = null;
   /** The property change support for this component. */
   protected PropertyChangeSupport support = new PropertyChangeSupport(this);
   /** The log4j Category */
   protected Logger log;
   /** The verbosity level for above which log messages may be filtered.  */
   protected int verbosity = ERROR;
   
   public Log4jLogger()
   {
   }
   public Log4jLogger(Logger log)
   {
      this.log = log;
   }

   /** Allow the log category to be set from the config category attribute
    */
   public void setCategory(String category)
   {
      log = Logger.getLogger(category);
   }

   /**
    * Add a property change listener to this component.
    *
    * @param listener The listener to add
    */
   public void addPropertyChangeListener(PropertyChangeListener listener)
   {
      support.addPropertyChangeListener(listener);
   }
   
   /**
    * Remove a property change listener from this component.
    *
    * @param listener The listener to remove
    */
   public void removePropertyChangeListener(PropertyChangeListener listener)
   {
      support.removePropertyChangeListener(listener);
   }
   
   /**
    * Return the Container with which this Logger has been associated.
    */
   public Container getContainer()
   {
      return container;
   }

   /**
    * Set the Container with which this Logger has been associated.
    *
    * @param container The associated Container
    */
   public void setContainer(Container container)
   {
      Container oldContainer = this.container;
      this.container = container;
      support.firePropertyChange("container", oldContainer, this.container);
   }

   /**
    * Return descriptive information about this Logger implementation and
    * the corresponding version number, in the format
    * <code>&lt;description&gt;/&lt;version&gt;</code>.
    */
   public String getInfo()
   {
      return getClass().getName();
   }
   
   /**
    * Return the verbosity level of this logger.  Messages logged with a
    * higher verbosity than this level will be silently ignored.
    */
   public int getVerbosity()
   {
      return this.verbosity;
   }
   /**
    * Set the verbosity level of this logger.  Messages logged with a
    * higher verbosity than this level will be silently ignored.
    *
    * @param verbosity The new verbosity level
    */
   public void setVerbosity(int verbosity)
   {
      this.verbosity = verbosity;
   }
   public void setVerbosityLevel(String verbosity)
   {
      if ("FATAL".equalsIgnoreCase(verbosity))
         this.verbosity = FATAL;
      else if ("ERROR".equalsIgnoreCase(verbosity))
         this.verbosity = ERROR;
      else if ("WARNING".equalsIgnoreCase(verbosity))
         this.verbosity = WARNING;
      else if ("INFORMATION".equalsIgnoreCase(verbosity))
         this.verbosity = INFORMATION;
      else if ("DEBUG".equalsIgnoreCase(verbosity))
         this.verbosity = DEBUG;
      else
      {
         log.warn("Unknown log level '"+verbosity+"' seen, using DEBUG\n"
            + "Valid values are: FATAL, ERROR, WARNING, INFORMATION or DEBUG");
         this.verbosity = DEBUG;         
      }
   }

   /**
    * Writes the specified message to a servlet log file, usually an event
    * log.  The name and type of the servlet log is specific to the
    * servlet container.  This message will be logged unconditionally.
    *
    * @param message A <code>String</code> specifying the message to be
    * written to the log file
    */
   public void log(String message)
   {
      log.info(message);
   }
   
   /**
    * Writes the specified exception, and message, to a servlet log file.
    * The implementation of this method should call
    * <code>log(msg, exception)</code> instead.  This method is deprecated
    * in the ServletContext interface, but not deprecated here to avoid
    * many useless compiler warnings.  This message will be logged
    * unconditionally.
    *
    * @param exception An <code>Exception</code> to be reported
    * @param message The associated message string
    */
   public void log(Exception exception, String message)
   {
      log.error(message, exception);
   }

   /**
    * Writes the specified message to the servlet log file, usually an event
    * log, if the logger is set to a verbosity level equal to or higher than
    * the specified value for this message.
    *
    * @param message A <code>String</code> specifying the message to be
    * written to the log file
    * @param verbosity Verbosity level of this message
    */
   public void log(String message, int verbosity)
   {
      switch( verbosity )
      {
         case FATAL:
            log.fatal(message);
            break;
         case ERROR:
            log.error(message);
            break;
         case WARNING:
            log.warn(message);
            break;
         case INFORMATION:
            log.info(message);
            break;
         case DEBUG:
            log.debug(message);
            break;
      }
   }

   /**
    * Writes an explanatory message and a stack trace for a given
    * <code>Throwable</code> exception to the servlet log file.  The name
    * and type of the servlet log file is specific to the servlet container,
    * usually an event log.  This message will be logged unconditionally.
    *
    * @param message A <code>String</code> that describes the error or
    * exception
    * @param throwable The <code>Throwable</code> error or exception
    */
   public void log(String message, Throwable throwable)
   {
      Throwable rootCause = null;
      if (throwable instanceof LifecycleException)
         rootCause = ((LifecycleException) throwable).getThrowable();
      else if (throwable instanceof ServletException)
         rootCause = ((ServletException) throwable).getRootCause();
      log.error(message, throwable);
      if( rootCause != null )
      {
         log.error("----- Root Cause -----", rootCause);
      }
   }

   /**
    * Writes the specified message and exception to the servlet log file,
    * usually an event log, if the logger is set to a verbosity level equal
    * to or higher than the specified value for this message.
    *
    * @param message A <code>String</code> that describes the error or
    * exception
    * @param throwable The <code>Throwable</code> error or exception
    * @param verbosity Verbosity level of this message
    */
   public void log(String message, Throwable throwable, int verbosity)
   {
      switch( verbosity )
      {
         case FATAL:
            log.fatal(message, throwable);
            break;
         case ERROR:
            log.error(message, throwable);
            break;
         case WARNING:
            log.warn(message, throwable);
            break;
         case INFORMATION:
            log.info(message, throwable);
            break;
         case DEBUG:
            log.debug(message, throwable);
            break;
      }
   }
   
}
