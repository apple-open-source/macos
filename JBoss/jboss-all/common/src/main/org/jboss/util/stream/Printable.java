/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.stream;

import java.io.PrintWriter;
import java.io.PrintStream;

/**
 * A simple interface to allow an object to print itself to a 
 * <code>PrintWriter</code> or <code>PrintStream</code>.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface Printable
{
   /**
    * Print to a PrintWriter.
    *
    * @param writer  PrintWriter to print to.
    */
   void print(PrintWriter writer);

   /**
    * Print to a PrintWriter.
    *
    * @param writer  PrintWriter to print to.
    * @param prefix  Prefix to append to each line in the stream.
    */
   void print(PrintWriter writer, String prefix);

   /**
    * Print to a PrintStream.
    *
    * @param stream  PrintStream to print to.
    */
   void print(PrintStream stream);

   /**
    * Print to a PrintStream.
    *
    * @param stream  PrintStream to print to.
    * @param prefix  Prefix to append to each line in the stream.
    */
   void print(PrintStream stream, String prefix);
}
