/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.logging.util;

import java.io.IOException;
import java.io.PrintWriter;
import java.io.Writer;

import org.apache.log4j.Logger;
import org.apache.log4j.Level;

/**
 *  A subclass of PrintWriter that redirects its output to a log4j Logger. <p>
 *
 *  This class is used to have something to give api methods that require a
 *  PrintWriter for logging. JBoss-owned classes of this nature generally ignore
 *  the PrintWriter and do their own log4j logging.
 *
 * @author     <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 *      .
 * @created    August 19, 2001
 * @version    $$
 */
public class LoggerWriter
   extends PrintWriter
{
   private Logger logger;
   private Level level;
   private boolean  inWrite;
   private boolean  issuedWarning;

   /**
    *  Redirect logging to the indicated logger using Level.INFO
    *
    * @param  logger  Description of Parameter
    */
   public LoggerWriter( final Logger logger ) {
      this( logger, Level.INFO );
   }

   /**
    *  Redirect logging to the indicated logger using the given level. The
    *  ps is simply passed to super but is not used.
    *
    * @param  logger  Description of Parameter
    * @param  level  Description of Parameter
    */
   public LoggerWriter( final Logger logger,
         final Level level ) {
      super( new InternalLoggerWriter( logger, level ), true );
   }

   /**
    * @created    August 19, 2001
    */
   static class InternalLoggerWriter extends Writer {
      private Logger logger;
      private Level level;
      private boolean closed;

      public InternalLoggerWriter( final Logger logger, final Level level ) {
         lock = logger;
         //synchronize on this logger
         this.logger = logger;
         this.level = level;
      }

      public void write( char[] cbuf, int off, int len )
         throws IOException {
         if ( closed ) {
            throw new IOException( "Called write on closed Writer" );
         }
         // Remove the end of line chars
         while ( len > 0 && ( cbuf[len - 1] == '\n' || cbuf[len - 1] == '\r' ) ) {
            len--;
         }
         if ( len > 0 ) {
            logger.log( level, String.copyValueOf( cbuf, off, len ) );
         }
      }


      public void flush()
         throws IOException {
         if ( closed ) {
            throw new IOException( "Called flush on closed Writer" );
         }
      }

      public void close() {
         closed = true;
      }
   }

}
