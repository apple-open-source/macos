/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.ejb;

/**
 * The EJBException exception is thrown by an enterprise Bean instance to its
 * container to report that the invoked business method or callback method
 * could not be completed because of an unexpected error (e.g. the instance
 * failed to open a database connection).
 */
public class EJBException extends RuntimeException
{
   private Exception causeException;
   
   /**
    * Constructs an EJBException with no detail message.
    */
   public EJBException()
   {
      super();
      causeException = null;
   }
   
   /**
    * Constructs an EJBException that embeds the originally thrown exception.
    *
    * @param ex - The originally thrown exception.
    */
   public EJBException(Exception ex)
   {
      super();
      causeException = ex;
   }
   
   /**
    * Constructs an EJBException with the specified detailed message.
    *
    * @param messagee - The detailed message.
    */
   public EJBException(String message)
   {
      super(message);
      causeException = null;
   }
   
   /**
    * Constructs an EJBException with the specified message and the
    * originally throw exception.
    *
    * @param message - The detailed message.
    * @param ex - The originally thrown exception.
    */
   public EJBException(String message, Exception ex)
   {
      super(message);
      causeException = ex;
   }
   
   /**
    * Obtain the exception that caused the EJBException being thrown.
    *
    * @return The originally thrown exception.
    */
   public Exception getCausedByException()
   {
      return causeException;
   }
   
   /**
    * Obtain the detailed message for this exception and the embedded
    * exception if there is one.
    *
    * @return The detailed message and the originally thrown exception.
    */
   public String getMessage()
   {
      StringBuffer s = new StringBuffer();
      s.append( super.getMessage() );
      if( causeException != null )
      {
         s.append( "; CausedByException is:\n\t" );
         s.append( causeException.getMessage() );
      }
      return s.toString();
   }
   
   /**
    * Print composite stack trace to System.err
    */
   public void printStackTrace()
   {
      super.printStackTrace();
      if( causeException != null )
      {
         causeException.printStackTrace();
      }
   }
   
   /**
    * Print composite stack trace to PrintStream ps
    *
    * @param ps - The print stream.
    */
   public void printStackTrace( java.io.PrintStream ps )
   {
      super.printStackTrace(ps);
      if( causeException != null )
      {
         causeException.printStackTrace(ps);
      }
   }
   
   /**
    * Print composite stack trace to PrintWriter pw
    *
    * @param pw - The print writer.
    */
   public void printStackTrace( java.io.PrintWriter pw )
   {
      super.printStackTrace(pw);
      if( causeException != null )
      {
         causeException.printStackTrace(pw);
      }
   }
}

