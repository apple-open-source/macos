/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package java.sql;

/**
 * A HACK to allow building under JDK 1.3
 */
public interface Savepoint
{
   int getSavepointId() throws SQLException;

   String getSavepointName() throws SQLException;
}
