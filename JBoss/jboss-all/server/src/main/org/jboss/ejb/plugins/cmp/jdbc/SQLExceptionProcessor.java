/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.ejb.plugins.cmp.jdbc;

import org.jboss.system.ServiceMBeanSupport;

import java.sql.SQLException;

/**
 * Default SQLExceptionProcessor.
 *
 * @author <a href="mailto:jeremy@boynes.com">Jeremy Boynes</a>
 *
 * @jmx.mbean
 */
public final class SQLExceptionProcessor extends ServiceMBeanSupport implements SQLExceptionProcessorMBean
{
   private static final String DUPLICATE_CODE = "23000";

   /**
    * Return true if the exception indicates that an operation failed due to a
    * unique constraint violation. This could be from any unique constraint
    * not just the primary key.
    *
    * @param e the SQLException to process
    * @return true if it was caused by a unique constraint violation
    * @jmx.managed-operation
    */
   public boolean isDuplicateKey(SQLException e)
   {
      return DUPLICATE_CODE.equals(e.getSQLState());
   }
}
