
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
import java.io.Serializable;
import javax.transaction.RollbackException;
import javax.transaction.TransactionRolledbackException;
import org.jboss.util.NestedThrowable;



/**
 * JBossTransactionRolledbackException.java
 *
 *
 * Created: Sun Feb  9 22:45:03 2003
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class JBossTransactionRolledbackException
   extends TransactionRolledbackException
   implements NestedThrowable, Serializable
{

   public JBossTransactionRolledbackException()
   {
      super();
   }

   public JBossTransactionRolledbackException(final String message)
   {
      super(message);
   }


   public JBossTransactionRolledbackException(final Throwable t)
   {
      super();
      this.detail = t;
   }

   public JBossTransactionRolledbackException(final String message, final Throwable t)
   {
      super(message);
      this.detail = t;
   }


   // Implementation of org.jboss.util.NestedThrowable

   public Throwable getNested()
   {
      return detail;
   }

   public Throwable getCause()
   {
      return detail;
   }

   /**
    * Returns the composite throwable message.
    *
    * @return  The composite throwable message.
    */
   public String getMessage()
   {
      return NestedThrowable.Util.getMessage(super.getMessage(), detail);
   }

   /**
    * Prints the composite message and the embedded stack trace to the
    * specified print stream.
    *
    * @param stream  Stream to print to.
    */
   public void printStackTrace(final PrintStream stream)
   {
      if (detail == null || NestedThrowable.PARENT_TRACE_ENABLED)
      {
         super.printStackTrace(stream);
      }
      NestedThrowable.Util.print(detail, stream);
   }

   /**
    * Prints the composite message and the embedded stack trace to the
    * specified print writer.
    *
    * @param writer  Writer to print to.
    */
   public void printStackTrace(final PrintWriter writer)
   {
      if (detail == null || NestedThrowable.PARENT_TRACE_ENABLED)
      {
         super.printStackTrace(writer);
      }
      NestedThrowable.Util.print(detail, writer);
   }

   /**
    * Prints the composite message and the embedded stack trace to
    * <tt>System.err</tt>.
    */
   public void printStackTrace()
   {
      printStackTrace(System.err);
   }

}// JBossTransactionRolledbackException
