/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.io.Serializable;

/**
 * A query expression.<p>
 *
 * An implementation of this interface can be used in
 * a query. Multiple query expressions can be used together to form
 * a more complex query.
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.2 $
 */
public interface QueryExp 
   extends Serializable
{
   // Constants ---------------------------------------------------

   // Public ------------------------------------------------------

   /**
    * Apply this query expression to an MBean.
    *
    * @param name the object name of the mbean
    * @return true or false as the result of the query expression.
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
   public boolean apply(ObjectName name)
      throws BadStringOperationException,
             BadBinaryOpValueExpException,
             BadAttributeValueExpException,
             InvalidApplicationException;

   /**
    * Set the MBeanServer for this query. Only MBeans registered in
    * this server can be used in queries.
    *
    * @param server the MBeanServer
    */
   public void setMBeanServer(MBeanServer server);
}
