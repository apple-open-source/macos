/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Thrown by the MBeanServer when an exception occurs using the
 * java.lang.reflect package to invoke methods on MBeans.
 *
 * @author <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.4.6.1 $
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
public class ReflectionException
   extends JMException
{
   // Constants -----------------------------------------------------

   private static final long serialVersionUID = 9170809325636915553L;

   // Attributes ----------------------------------------------------

   /**
    * The wrapped exception.
    */
   private Exception exception = null;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new ReflectionException from a given exception.
    *
    * @param e the exception to wrap.
    */
   public ReflectionException(Exception e)
   {
      super();
      this.exception = e;
   }

   /**
    * Construct a new ReflectionException from a given exception and message.
    *
    * @param e the exception to wrap.
    * @param message the specified message.
    */
   public ReflectionException(Exception e, String message)
   {
      super(message);
      this.exception = e;
   }

   // Public --------------------------------------------------------

   /**
    * Retrieves the wrapped exception.
    *
    * @return the wrapped exception.
    */
   public Exception getTargetException()
   {
      return exception;
   }

   // JMException overrides -----------------------------------------
   /**
    * Returns a string representation of this exception. The returned string
    * contains this exception name, message and a string representation of the
    * target exception if it has been set.
    *
    * @return string representation of this exception
    */
   public String toString()
   {
      return "ReflectionException: " + getMessage() + ((exception == null) ? "" : "\nCause: " + exception.toString());
   }
   
}

