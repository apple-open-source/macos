package org.jboss.resource.connectionmanager;

import java.io.PrintStream;
import java.io.PrintWriter;

import javax.transaction.xa.XAException;

import org.jboss.util.NestedThrowable;



/**
 * JBossLocalXAException.java
 *
 *
 * Created: Sun Feb  9 16:25:06 2003
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class JBossLocalXAException
   extends XAException
   implements NestedThrowable
{

   private final Throwable t;

   public JBossLocalXAException()
   {
      super();
      t = null;
   }

   public JBossLocalXAException(int errcode)
   {
      super(errcode);
      t = null;
   }

   public JBossLocalXAException(String message)
   {
      super(message);
      t = null;
   }

   public JBossLocalXAException(String message, Throwable t)
   {
      super(message);
      this.t = t;
   }


   // Implementation of org.jboss.util.NestedThrowable

   public Throwable getNested()
   {
      return t;
   }

   public Throwable getCause()
   {
      return t;
   }

   /**
    * Returns the composite throwable message.
    *
    * @return  The composite throwable message.
    */
   public String getMessage() {
      return NestedThrowable.Util.getMessage(super.getMessage(), t);
   }

   /**
    * Prints the composite message and the embedded stack trace to the
    * specified print stream.
    *
    * @param stream  Stream to print to.
    */
   public void printStackTrace(final PrintStream stream)
   {
      if (t == null || NestedThrowable.PARENT_TRACE_ENABLED)
      {
         super.printStackTrace(stream);
      }
      NestedThrowable.Util.print(t, stream);
   }

   /**
    * Prints the composite message and the embedded stack trace to the
    * specified print writer.
    *
    * @param writer  Writer to print to.
    */
   public void printStackTrace(final PrintWriter writer)
   {
      if (t == null || NestedThrowable.PARENT_TRACE_ENABLED)
      {
         super.printStackTrace(writer);
      }
      NestedThrowable.Util.print(t, writer);
   }

   /**
    * Prints the composite message and the embedded stack trace to
    * <tt>System.err</tt>.
    */
   public void printStackTrace()
   {
      printStackTrace(System.err);
   }

}// JBossLocalXAException
