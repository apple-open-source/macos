/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.logging;

/**
 * LoggerPlugin implementation producing no output at all. Used for client
 * side logging when no log4j.jar is available on the classpath.
 *
 * @see org.jboss.logging.Logger
 * @see org.jboss.logging.LoggerPlugin
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>30 mai 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class NullLoggerPlugin implements LoggerPlugin
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public NullLoggerPlugin () { }
   
   public void init (String name)
   { /* don't care */ }
   
   // Public --------------------------------------------------------
   
   public boolean isTraceEnabled () { return false; }
   public void trace (Object message) { }   
   public void trace (Object message, Throwable t) { }
   
   public boolean isDebugEnabled () { return false; }
   public void debug (Object message) { }   
   public void debug (Object message, Throwable t) { }
   
   public boolean isInfoEnabled () { return false; }
   public void info (Object message) { }   
   public void info (Object message, Throwable t) { }
   
   public void error (Object message) { }   
   public void error (Object message, Throwable t) { }
   
   public void fatal (Object message) { }   
   public void fatal (Object message, Throwable t) { }
   
   public void warn (Object message) { }   
   public void warn (Object message, Throwable t) { }
   
   // Z implementation ----------------------------------------------
   
   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
