/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.logging.appender;

/** 
 * An extention of the default Log4j RollingFileAppender which
 * will make the directory structure for the set log file. 
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class RollingFileAppender
   extends org.apache.log4j.RollingFileAppender
{
   public void setFile(final String filename)
   {
      FileAppender.Helper.makePath(filename);
      super.setFile(filename);
   }
}
