/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.tm;

import java.io.PrintStream;
import java.io.PrintWriter;
import javax.ejb.TransactionRolledbackLocalException;
import javax.transaction.RollbackException;
import org.jboss.util.NestedThrowable;

/**
 * JBossTransactionRolledbackLocalException.java
 *
 * Created: Sun Feb  9 22:45:03 2003
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */
public class JBossTransactionRolledbackLocalException
   extends TransactionRolledbackLocalException
   implements NestedThrowable
{
   public JBossTransactionRolledbackLocalException()
   {
      super();
   }

   public JBossTransactionRolledbackLocalException(final Exception e)
   {
      super(null, e);
   }

   public JBossTransactionRolledbackLocalException(final String message)
   {
      super(message);
   }

   public JBossTransactionRolledbackLocalException(final String message, final Exception e)
   {
      super(message, e);
   }

   // Implementation of org.jboss.util.NestedThrowable

   public Throwable getNested()
   {
      return getCausedByException();
   }

   public Throwable getCause()
   {
      return getCausedByException();
   }

   /**
    * Returns the composite throwable message.
    *
    * @return  The composite throwable message.
    */
   public String getMessage() {
      return NestedThrowable.Util.getMessage(super.getMessage(), getCausedByException());
   }

   /**
    * Prints the composite message and the embedded stack trace to the
    * specified print stream.
    *
    * @param stream  Stream to print to.
    */
   public void printStackTrace(final PrintStream stream)
   {
      if (getCausedByException() == null || NestedThrowable.PARENT_TRACE_ENABLED)
      {
         super.printStackTrace(stream);
      }
      NestedThrowable.Util.print(getCausedByException(), stream);
   }

   /**
    * Prints the composite message and the embedded stack trace to the
    * specified print writer.
    *
    * @param writer  Writer to print to.
    */
   public void printStackTrace(final PrintWriter writer)
   {
      if (getCausedByException() == null || NestedThrowable.PARENT_TRACE_ENABLED)
      {
         super.printStackTrace(writer);
      }
      NestedThrowable.Util.print(getCausedByException(), writer);
   }

   /**
    * Prints the composite message and the embedded stack trace to
    * <tt>System.err</tt>.
    */
   public void printStackTrace()
   {
      printStackTrace(System.err);
   }

}// JBossTransactionRolledbackLocalException
