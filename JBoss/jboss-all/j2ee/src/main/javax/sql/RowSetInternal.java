/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 * 2001/04/08: kjenks: Initial author
 * 2001/06/14: jpedersen: Updated javadoc, removed abstract from methods
 */
package javax.sql;

import java.sql.*;

/**
 * A rowset object presents itself to a reader or writer as an instance of RowSetInternal.
 * The RowSetInternal interface contains additional methods that let the reader or writer access
 * and modify the internal state of the rowset.
 */
public interface RowSetInternal {

  /**
   * Get the Connection passed to the rowset.
   *
   * @return the Connection passed to the rowset, or null if none
   * @exception SQLException - if a database-access error occurs.
   */
  public Connection getConnection()
    throws SQLException;

  /**
   * Returns a result set containing the original value of the rowset. The cursor is positioned before the
   * first row in the result set. Only rows contained in the result set returned by getOriginal() are said to
   * have an original value.
   *
   * @return the original value of the rowset
   * @exception SQLException - if a database-access error occurs.
   */
  public ResultSet getOriginal()
    throws SQLException;

  /**
   * Returns a result set containing the original value of the current row. If the current row has no original
   * value an empty result set is returned. If there is no current row a SQLException is thrown.
   *
   * @return the original value of the row
   * @exception SQLException - if a database-access error occurs.
   */
  public ResultSet getOriginalRow()
    throws SQLException;

  /**
   * Get the parameters that were set on the rowset.
   *
   * @return an array of parameters
   * @exception SQLException - if a database-access error occurs.
   */
  public Object[] getParams()
    throws SQLException;

  /**
   * Set the rowset's metadata.
   *
   * @param rowSetMetaData - metadata object
   * @exception SQLException - if a database-access error occurs.
   */
  public void setMetaData(RowSetMetaData rowSetMetaData)
    throws SQLException;
}
