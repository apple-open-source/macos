/*
 * JUnitEJB
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.util.ejb;

import junit.framework.AssertionFailedError;

/**
 * RemoteAssertionFailedError is the client-side view of an assertion
 * failed error on the server.  
 *
 * All throwables caught on the server are wrapped with a RemoteTestException
 * and rethrown.  On the client side the exception is caught, and if the 
 * server side exception is an instance of AssertionFailedError, it is
 * wrapped with an instance of this class and rethrown. That makes the 
 * exception an instance of AssertionFailedError so it is reconized as 
 * a failure and not an Error.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1.2.1 $
 */
public class RemoteAssertionFailedError extends AssertionFailedError
{
   private AssertionFailedError remoteAssertionFailedError;
   private String remoteStackTrace;

   /**
    * Constructs a remote assertion failed error based on the specified
    * AssertionFailedError and remote stack trace.
    * @param e the AssertionFailedError that was thrown on the server side
    * @param remoteStackTrace the stack trace of the assertion failed error
    * 		exactly as it appeared on the server side
    */
   public RemoteAssertionFailedError(
      AssertionFailedError e,
      String remoteStackTrace)
   {

      remoteAssertionFailedError = e;
      this.remoteStackTrace = remoteStackTrace;
   }

   /**
    * Gets the message exactly as it appeared on server side.
    * @return the message exactly as it appeared on server side
    */
   public String getMessage()
   {
      return remoteAssertionFailedError.getMessage();
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
    * Gets the assertion failed error object from the server side.
    * Note: the stack trace of this object is not available because
    * 	exceptions don't seralize the stack trace. Use 
    *		getRemoteStackTrace to get the stack trace as it appeared 
    * 	on the server.
    * @retun the assertion failed error object from the server side.
    */
   public AssertionFailedError getRemoteAssertionFailedError()
   {
      return remoteAssertionFailedError;
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
