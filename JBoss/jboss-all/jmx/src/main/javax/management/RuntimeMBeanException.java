/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Wraps runtime exceptions thrown by MBeans.
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
public class RuntimeMBeanException
   extends JMRuntimeException
{
   // Constants -----------------------------------------------------

   private static final long serialVersionUID = 5274912751982730171L;

   // Attributes ----------------------------------------------------

   /**
    * The wrapped runtime exception.
    */
   private RuntimeException runtimeException = null;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new RuntimeMBeanException from a given runtime exception.
    *
    * @param e the runtime exception to wrap.
    */
   public RuntimeMBeanException(RuntimeException e)
   {
      super();
      this.runtimeException = e;
   }

   /**
    * Construct a new RuntimeMBeanException from a given runtime exception
    * and message.
    *
    * @param e the runtime exception to wrap.
    * @param message the specified message.
    */
   public RuntimeMBeanException(RuntimeException e, String message)
   {
      super(message);
      this.runtimeException = e;
   }

   // Public --------------------------------------------------------

   /**
    * Retrieves the wrapped runtime exception.
    *
    * @return the wrapped runtime exception.
    */
   public RuntimeException getTargetException()
   {
      return runtimeException;
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
      return "RuntimeMBeanException: " + getMessage() + ((runtimeException == null) ? "" : "\nCause: " + runtimeException.toString());
   }

   // Private -------------------------------------------------------
}

