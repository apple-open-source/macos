/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.io.Serializable;

/**
 * A value expression.<p>
 *
 * Implementations of this interface represent arguments to expressions.
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.3 $
 */
public interface ValueExp
   extends Serializable
{
   /**
    * Apply this value expression to an MBean.
    *
    * @param name the object name of the mbean
    * @return this value expression
    * @exception BadStringOperationException when an invalid string operation
    *            is used during query construction
    * @exception BadBinaryOpValueExpException when an invalid binary operation
    *            is used during query construction
    * @exception BadAttributeValueExpException when an invalid MBean attribute
    *            is used during query construction
    * @exception InvalidApplicationException when trying to apply a subquery
    *            expression to an MBean or an attribute expression to an
    *            MBean of the wrong class.
    */
   public ValueExp apply(ObjectName name)
      throws BadStringOperationException,
             BadBinaryOpValueExpException,
             BadAttributeValueExpException,
             InvalidApplicationException;

   /**
    * Set the MBeanServer for this expression. Only MBeans registered in
    * this server can be used in queries.
    *
    * @param server the MBeanServer
    */
   public void setMBeanServer(MBeanServer server);
}
