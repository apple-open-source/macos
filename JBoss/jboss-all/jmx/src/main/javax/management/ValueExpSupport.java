/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.io.Serializable;

/**
 * An implementation of Value expression. Apply always returns this.
 *
 * <p><b>Revisions:</b>
 * <p><b>20020317 Adrian Brock:</b>
 * <ul>
 * <li>Make queries thread safe
 * </ul>
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.2.6.1 $
 */
/*package*/ class ValueExpSupport
   implements ValueExp
{
   // Constants ---------------------------------------------------

   private static final long serialVersionUID = 5668002406025575052L;

   // Attributes --------------------------------------------------

   // Static ------------------------------------------------------

   // Constructor -------------------------------------------------

   // Public ------------------------------------------------------

   // ValueExp implementation -------------------------------------

   public ValueExp apply(ObjectName name)
      throws BadStringOperationException,
             BadBinaryOpValueExpException,
             BadAttributeValueExpException,
             InvalidApplicationException
   {
      return this;
   }

   public void setMBeanServer(MBeanServer server)
   {
      QueryEval.server.set(server);
   }

   // Object overrides --------------------------------------------

   // Protected ---------------------------------------------------

   // Package -----------------------------------------------------

   /**
    * Get the MBean server
    */
   /*package*/ MBeanServer getMBeanServer()
   {
      return (MBeanServer) QueryEval.server.get();
   }

   // Private -----------------------------------------------------

   // Inner classes -----------------------------------------------
}
