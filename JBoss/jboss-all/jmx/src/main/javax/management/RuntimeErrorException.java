/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;


/**
 * Thrown when a java.lang.error occurs.
 *
 * @author <a href="mailto:juha@jboss.org">Juha Lindfors</a>
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.3.6.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>20020313 Juha Lindfors:</b>
 * <ul>
 * <li> Overriding toString() to print out the root exception </li>
 * </ul>
 * <p><b>20020711 Adrian Brock:</b>
 * <ul>
 * <li> Serialization </li>
 * </ul>
 */
public class RuntimeErrorException
   extends JMRuntimeException
{
   // Constants -----------------------------------------------------

   private static final long serialVersionUID = 704338937753949796L;

   // Attributes ----------------------------------------------------

   /**
    * The wrapped error.
    */
   private Error error = null;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new RuntimeErrorException from a given error.
    *
    * @param e the error to wrap.
    */
   public RuntimeErrorException(Error e)
   {
      super();
      this.error = e;
   }

   /**
    * Construct a new RuntimeErrorException from a given error and message.
    *
    * @param e the error to wrap.
    * @param message the specified message.
    */
   public RuntimeErrorException(Error e, String message)
   {
      super(message);
      this.error = e;
   }

   // Public --------------------------------------------------------

   /**
    * Retrieves the wrapped error.
    *
    * @return the wrapped error.
    */
   public java.lang.Error getTargetError()
   {
      return error;
   }

   // JMRuntimeException overrides ----------------------------------
   /**
    * Returns a string representation of this exception. The returned string
    * contains this exception name, message and a string representation of the
    * target exception if it has been set.
    *
    * @return string representation of this exception
    */
   public String toString()
   {
      return "RuntimeErrorException: " + getMessage() + ((error == null) ? "" : "\nCause: " + error.toString());
   }

   // Private -------------------------------------------------------
}
