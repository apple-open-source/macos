/*
 * JUnitEJB
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.util.ejb;

import java.io.PrintStream;
import java.io.PrintWriter;
import java.io.StringWriter;

/**
 * RemoteTestException is the client-side view of a throwable on the server.  
 *
 * All throwables caught on the server are wrapped with a RemoteTestException
 * and rethrown. On the client side the exception is caught, and if the 
 * server side exception is an instance of AssertionFailedError, it is
 * wrapped with a RemoteAssertionFailedError and rethrown. That makes the 
 * exception an instance of AssertionFailedError so it is reconized as 
 * a failure and not an Error.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1.2.1 $
 */
public class RemoteTestException extends Exception
{
   private Throwable remoteThrowable;
   private String remoteStackTrace;

   /**
    * Constructs a remote test exception that wrapps the the specified
    * throwable.
    * @param e the Throwable that was thrown on the server side
    */
   public RemoteTestException(Throwable e)
   {
      remoteThrowable = e;

      StringWriter stringWriter = new StringWriter();
      PrintWriter writer = new PrintWriter(stringWriter);
      e.printStackTrace(writer);
      StringBuffer buffer = stringWriter.getBuffer();
      remoteStackTrace = buffer.toString();
   }

   /**
    * Gets the message exactly as it appeared on server side.
    * @return the message exactly as it appeared on server side
    */
   public String getMessage()
   {
      return remoteThrowable.getMessage();
   }

   /**
    * Prints the stack trace exactly as it appeared on the server side.
    * @param ps the PrintStream on which the stack trace is printed
    */
   public void printStackTrace(java.io.PrintStream ps)
   {
      ps.print(remoteStackTrace);
   }

   /**
    * Prints the stack trace exactly as it appeared on the server side.
    */
   public void printStackTrace()
   {
      printStackTrace(System.err);
   }

   /**
    * Prints the stack trace exactly as it appeared on the server side.
    * @param pw the PrintWriter on which the stack trace is printed
    */
   public void printStackTrace(java.io.PrintWriter pw)
   {
      pw.print(remoteStackTrace);
   }

   /**
    * Gets the throwable object from the server side.
    * Note: the stack trace of this object is not available because
    * 	exceptions don't seralize the stack trace. Use 
    *		getRemoteStackTrace to get the stack trace as it appeared 
    * 	on the server.
    * @return the Throwable object from the server side.
    */
   public Throwable getRemoteThrowable()
   {
      return remoteThrowable;
   }

   /**
    * Gets the stack trace exactly as it appeared on the server side.
    * @return the stack trace exactly as it appeared on the server side
    */
   public String getRemoteStackTrace()
   {
      return remoteStackTrace;
   }
}
