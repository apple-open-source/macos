/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.logging;

/**
 * Defines a "pluggable" login module. In fact, this is only used to split between 
 * log4j and /dev/null. Choice is made in org.jboss.logging.Logger
 *
 * @see org.jboss.logging.Logger
 * @see org.jboss.logging.Log4jLoggerPlugin
 * @see org.jboss.logging.NullLoggerPlugin
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

public interface LoggerPlugin
{
   // must be called first
   //
   public void init (String name);
   
   public boolean isTraceEnabled();
   public void trace(Object message);
   public void trace(Object message, Throwable t);

   public boolean isDebugEnabled();
   public void debug(Object message);
   public void debug(Object message, Throwable t);

   public boolean isInfoEnabled();
   public void info(Object message);
   public void info(Object message, Throwable t);

   public void warn(Object message);
   public void warn(Object message, Throwable t);

   public void error(Object message);
   public void error(Object message, Throwable t);

   public void fatal(Object message);
   public void fatal(Object message, Throwable t);
   
}
