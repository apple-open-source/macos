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

import java.sql.ResultSetMetaData;
import java.sql.SQLException;

/**
 * The RowSetMetaData interface extends ResultSetMetaData with methods that allow a metadata object to be initialized.
 * A RowSetReader may create a RowSetMetaData and pass it to a rowset when new data is read.
 */
public interface RowSetMetaData extends ResultSetMetaData {

  /**
   * Specify whether the is column automatically numbered, thus read-only.
   *
   * @param columnIndex - the first column is 1, the second is 2, ...
   * @param flag - is either true or false.
   * @exception SQLException - if a database-access error occurs.
   */
  public void setAutoIncrement(int columnIndex, boolean flag)
    throws SQLException;

  /**
   * Specify whether the column is case sensitive.
   *
   * @param columnIndex - the first column is 1, the second is 2, ...
   * @param flag - is either true or false.
   * @exception SQLException - if a database-access error occurs.
   */
  public void setCaseSensitive(int columnIndex, boolean flag)
    throws SQLException;

  /**
   * Specify the column's table's catalog name, if any.
   *
   * @param columnIndex - the first column is 1, the second is 2, ...
   * @param string - column's catalog name.
   * @exception SQLException - if a database-access error occurs.
   */
  public void setCatalogName(int columnIndex, String string)
    throws SQLException;

  /**
   * Set the number of columns in the RowSet.
   *
   * @param i - number of columns.
   * @exception SQLException - if a database-access error occurs.
   */
  public void setColumnCount(int i)
    throws SQLException;

  /**
   * Specify the column's normal max width in chars.
   *
   * @param columnIndex - the first column is 1, the second is 2, ...
   * @param size - size of the column
   * @exception SQLException - if a database-access error occurs.
   */
  public void setColumnDisplaySize(int columnIndex, int size)
    throws SQLException;

  /**
   * Specify the suggested column title for use in printouts and displays, if any.
   *
   * @param columnIndex - the first column is 1, the second is 2, ...
   * @param label - the column title
   * @exception SQLException - if a database-access error occurs.
   */
  public void setColumnLabel(int columnIndex, String label)
    throws SQLException;

  /**
   * Specify the column name.
   *
   * @param columnIndex - the first column is 1, the second is 2, ...
   * @param name - the column name
   * @exception SQLException - if a database-access error occurs.
   */
  public void setColumnName(int columnIndex, String name)
    throws SQLException;

  /**
   * Specify the column's SQL type.
   *
   * @param columnIndex - the first column is 1, the second is 2, ...
   * @param sqltype - column's SQL type.
   * @exception SQLException - if a database-access error occurs.
   */
  public void setColumnType(int columnIndex, int sqltype)
    throws SQLException;

  /**
   * Specify the column's data source specific type name, if any.
   *
   * @param columnIndex - the first column is 1, the second is 2, ...
   * @param sqltype - column's SQL type name.
   * @exception SQLException - if a database-access error occurs.
   */
  public void setColumnTypeName(int columnIndex, String typeName)
    throws SQLException;

  /**
   * Specify whether the column is a cash value.
   *
   * @param columnIndex - the first column is 1, the second is 2, ...
   * @param flag - is either true or false.
   * @exception SQLException - if a database-access error occurs.
   */
  public void setCurrency(int columnIndex, boolean flag)
    throws SQLException;

  /**
   * Specify whether the column's value can be set to NULL.
   *
   * @param columnIndex - the first column is 1, the second is 2, ...
   * @param property - is either true or false.
   * @exception SQLException - if a database-access error occurs.
   */
  public void setNullable(int columnIndex, int property)
    throws SQLException;

  /**
   * Specify the column's number of decimal digits.
   *
   * @param columnIndex - the first column is 1, the second is 2, ...
   * @param precision - number of decimal digits.
   * @exception SQLException - if a database-access error occurs.
   */
  public void setPrecision(int columnIndex, int precision)
    throws SQLException;

  /**
   * Specify the column's number of digits to right of the decimal point.
   *
   * @param columnIndex - the first column is 1, the second is 2, ...
   * @param scale - number of digits to right of decimal point.
   * @exception SQLException - if a database-access error occurs.
   */
  public void setScale(int columnIndex, int scale)
    throws SQLException;

  /**
   * Specify the column's table's schema, if any.
   *
   * @param columnIndex - the first column is 1, the second is 2, ...
   * @param schemaName - the schema name
   * @exception SQLException - if a database-access error occurs.
   */
  public void setSchemaName(int columnIndex, String schemaName)
    throws SQLException;

  /**
   * Specify whether the column can be used in a where clause.
   *
   * @param columnIndex - the first column is 1, the second is 2, ...
   * @param flag - is either true or false.
   * @exception SQLException - if a database-access error occurs.
   */
  public void setSearchable(int columnIndex, boolean flag)
    throws SQLException;

  /**
   * Specify whether the column is a signed number.
   *
   * @param columnIndex - the first column is 1, the second is 2, ...
   * @param flag - is either true or false.
   * @exception SQLException - if a database-access error occurs.
   */
  public void setSigned(int columnIndex, boolean flag)
    throws SQLException;

  /**
   * Specify the column's table name, if any.
   *
   * @param columnIndex - the first column is 1, the second is 2, ...
   * @param tableName - column's table name.
   * @exception SQLException - if a database-access error occurs.
   */
  public void setTableName(int columnIndex, String tableName)
    throws SQLException;
}
