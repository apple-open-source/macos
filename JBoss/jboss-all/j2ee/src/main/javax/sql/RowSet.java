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
import java.io.InputStream;
import java.io.Reader;
import java.math.BigDecimal;
import java.util.Calendar;
import java.util.Map;

/**
 * <p>The RowSet interface adds support to the JDBC API for the JavaBeans(TM) component model. A rowset can
 * be used as a JavaBean in a visual Bean development environment. A RowSet can be created and configured at
 * design time and executed at runtime. The RowSet interface provides a set of JavaBeans properties that allow
 * a RowSet instance to be configured to connect to a JDBC data source and read some data from the data source.
 * A group of setXXX() methods provide a way to pass input parameters to a rowset. The RowSet interface supports
 * JavaBeans events, allowing other components in an application to be notified when an important event on a rowset
 * occurs, such as a change in its value.</p>
 *
 * <p>The RowSet interface is unique in that it is intended to be implemented using the rest of the JDBC(TM) API.
 * In other words, a RowSet implementation is a layer of software that executes "on top" of a JDBC driver.
 * Implementations of the RowSet interface can be provided by anyone, including JDBC driver vendors who want to provide
 * a RowSet implementation as part of their JDBC products.</p>
 *
 * <p>Rowsets are easy to use. The RowSet interface extends the standard java.sql.ResultSet interface. The RowSetMetaData
 * interface extends the java.sql.ResultSetMetaData interface. Thus, developers familiar with the JDBC API will have to learn
 * a minimal number of new APIs to use rowsets. In addition, third-party software tools that work with JDBC ResultSets will
 * also easily be made to work with rowsets.</p>
 */
public interface RowSet extends ResultSet {

  /**
   * RowSet listener registration. Listeners are notified when an event occurs.
   *
   * @param rowSetListener - an event listener
   */
  public void addRowSetListener(RowSetListener rowSetListener);

  /**
   * In general, parameter values remain in force for repeated use of a RowSet. Setting a parameter value
   * automatically clears its previous value. However, in some cases it is useful to immediately release the
   * resources used by the current parameter values; this can be done by calling clearParameters.
   *
   * @exception SQLException - if a database-access error occurs.
   */
  public void clearParameters()
    throws SQLException;

  /**
   * Fills the rowset with data. Execute() may use the following properties: url, data source name, user name,
   * password, transaction isolation, and type map to create a connection for reading data. Execute may use the
   * following properties to create a statement to execute a command: command, read only, maximum field size, maximum rows,
   * escape processing, and query timeout. If the required properties have not been set, an exception is thrown.
   * If successful, the current contents of the rowset are discarded and the rowset's metadata is also (re)set.
   * If there are outstanding updates, they are ignored.
   *
   * @exception SQLException - if a database-access error occurs.
   */
  public void execute()
    throws SQLException;

  /**
   * Get the rowset's command property. The command property contains a command string that can be executed to fill
   * the rowset with data. The default value is null.
   *
   * @return the command string, may be null
   */
  public String getCommand();

  /**
   * The JNDI name that identifies a JDBC data source. Users should set either the url or data source name properties.
   * The most recent property set is used to get a connection.
   *
   * @return a data source name
   */
  public String getDataSourceName();

  /**
   * If escape scanning is on (the default), the driver will do escape substitution before sending the SQL to the database.
   *
   * @return true if enabled; false if disabled
   * @exception SQLException - if a database-access error occurs.
   */
  public boolean getEscapeProcessing()
    throws SQLException;

  /**
   * The maxFieldSize limit (in bytes) is the maximum amount of data returned for any column value; it only applies
   * to BINARY, VARBINARY, LONGVARBINARY, CHAR, VARCHAR, and LONGVARCHAR columns. If the limit is exceeded, the excess data
   * is silently discarded.
   *
   * @return the current max column size limit; zero means unlimited
   * @exception SQLException - if a database-access error occurs.
   */
  public int getMaxFieldSize()
    throws SQLException;

  /**
   * The maxRows limit is the maximum number of rows that a RowSet can contain. If the limit is exceeded, the excess
   * rows are silently dropped.
   *
   * @return the current max row limit; zero means unlimited
   * @exception SQLException - if a database-access error occurs.
   */
  public int getMaxRows()
    throws SQLException;

  /**
   * The password used to create a database connection. The password property is set at runtime before calling execute().
   * It is not usually part of the serialized state of a rowset object.
   *
   * @return a password
   */
  public String getPassword();

  /**
   * The queryTimeout limit is the number of seconds the driver will wait for a Statement to execute.
   * If the limit is exceeded, a SQLException is thrown.
   *
   * @return the current query timeout limit in seconds; zero means unlimited
   * @exception SQLException - if a database-access error occurs.
   */
  public int getQueryTimeout()
    throws SQLException;

  /**
   * The transaction isolation property contains the JDBC transaction isolation level used.
   *
   * @return the transaction isolation level
   */
  public int getTransactionIsolation();

  /**
   * Get the type-map object associated with this rowset. By default, the map returned is empty.
   *
   * @return a map object
   * @exception SQLException - if a database-access error occurs.
   */
  public Map getTypeMap()
    throws SQLException;

  /**
   * Get the url used to create a JDBC connection. The default value is null.
   *
   * @return a string url
   * @exception SQLException - if a database-access error occurs.
   */
  public String getUrl()
    throws SQLException;

  /**
   * The username used to create a database connection. The username property is set at runtime before calling execute().
   * It is not usually part of the serialized state of a rowset object.
   *
   * @return a user name
   */
  public String getUsername();

  /**
   * A rowset may be read-only. Attempts to update a read-only rowset will result in an SQLException being thrown.
   * Rowsets are updateable, by default, if updates are possible.
   *
   * @return true if not updatable, false otherwise
   */
  public boolean isReadOnly();

  /**
   * RowSet listener deregistration.
   *
   * @param rowSetListener - an event listener
   */
  public void removeRowSetListener(RowSetListener rowSetListener);

  /**
   * Set an Array parameter.
   *
   * @param i - the first parameter is 1, the second is 2, ...
   * @param array - an object representing an SQL array
   * @exception SQLException - if a database-access error occurs.
   */
  public void setArray(int i, Array array)
    throws SQLException;

  /**
   * <p>When a very large ASCII value is input to a LONGVARCHAR parameter, it may be more practical to send it via
   * a java.io.InputStream. JDBC will read the data from the stream as needed, until it reaches end-of-file.</p>
   *
   * <p><b>Note:</b> This stream object can either be a standard Java stream object or your own subclass that implements
   * the standard interface.</p>
   *
   * @param i - the first parameter is 1, the second is 2, ...
   * @param inputStream - the java input stream which contains the ASCII parameter value
   * @param j - the number of bytes in the stream
   * @exception SQLException - if a database-access error occurs.
   */
  public void setAsciiStream(int i, InputStream inputStream, int j)
    throws SQLException;

  /**
   * Set a parameter to a java.lang.BigDecimal value.
   *
   * @param i - the first parameter is 1, the second is 2, ...
   * @param bigDecimal - the parameter value
   * @exception SQLException - if a database-access error occurs.
   */
  public void setBigDecimal(int i, BigDecimal bigDecimal)
    throws SQLException;

  /**
   * <p>When a very large binary value is input to a LONGVARBINARY parameter, it may be more practical to send it
   * via a java.io.InputStream. JDBC will read the data from the stream as needed, until it reaches end-of-file.</p>
   *
   * <p><b>Note:</b> This stream object can either be a standard Java stream object or your own subclass that implements
   * the standard interface.</p>
   *
   * @param i - the first parameter is 1, the second is 2, ...
   * @param inputStream - the java input stream which contains the binary parameter value
   * @param j - the number of bytes in the stream
   * @exception SQLException - if a database-access error occurs.
   */
  public void setBinaryStream(int i, InputStream inputStream, int j)
    throws SQLException;

  /**
   * Set a BLOB parameter.
   *
   * @param i - the first parameter is 1, the second is 2, ...
   * @param blob - an object representing a BLOB
   * @exception SQLException - if a database-access error occurs.
   */
  public void setBlob(int i, Blob blob)
    throws SQLException;

  /**
   * Set a parameter to a Java boolean value.
   *
   * @param i - the first parameter is 1, the second is 2, ...
   * @param flag - the parameter value
   * @exception SQLException - if a database-access error occurs.
   */
  public void setBoolean(int i, boolean flag)
    throws SQLException;

  /**
   * Set a parameter to a Java byte value.
   *
   * @param i - the first parameter is 1, the second is 2, ...
   * @param b - the parameter value
   * @exception SQLException - if a database-access error occurs.
   */
  public void setByte(int i, byte b)
    throws SQLException;

  /**
   * Set a parameter to a Java array of bytes.
   *
   * @param i - the first parameter is 1, the second is 2, ...
   * @param ab - the parameter value
   * @exception SQLException - if a database-access error occurs.
   */
  public void setBytes(int i, byte ab[])
    throws SQLException;

  /**
   * <p>When a very large UNICODE value is input to a LONGVARCHAR parameter, it may be more practical to send it via a
   * java.io.Reader. JDBC will read the data from the stream as needed, until it reaches end-of-file.</p>
   *
   * <p><b>Note:</b> This stream object can either be a standard Java stream object or your own subclass that implements
   * the standard interface.</p>
   *
   * @param i - the first parameter is 1, the second is 2, ...
   * @param reader - the java reader which contains the UNICODE data
   * @param j - the number of characters in the stream
   * @exception SQLException - if a database-access error occurs.
   */
  public void setCharacterStream(int i, Reader reader, int j)
    throws SQLException;

  /**
   * Set a CLOB parameter.
   *
   * @param i - the first parameter is 1, the second is 2, ...
   * @param clob - an object representing a CLOB
   * @exception SQLException - if a database-access error occurs.
   */
  public void setClob(int i, Clob clob)
    throws SQLException;

  /**
   * Set the rowset's command property. This property is optional. The command property may not be needed
   * when a rowset is produced by a data source that doesn't support commands, such as a spreadsheet.
   *
   * @param string - a command string, may be null
   * @exception SQLException - if a database-access error occurs.
   */
  public void setCommand(String string)
    throws SQLException;

  /**
   * Set the rowset concurrency.
   *
   * @param i - a value from ResultSet.CONCUR_XXX
   * @exception SQLException - if a database-access error occurs.
   */
  public void setConcurrency(int i)
    throws SQLException;

  /**
   * Set the data source name.
   *
   * @param string - a data source name
   * @exception SQLException - if a database-access error occurs.
   */
  public void setDataSourceName(String string)
    throws SQLException;

  /**
   * Set a parameter to a java.sql.Date value.
   *
   * @param i - the first parameter is 1, the second is 2, ...
   * @param date - the parameter value
   * @exception SQLException - if a database-access error occurs.
   */
  public void setDate(int i, Date date)
    throws SQLException;

  /**
   * Set a parameter to a java.sql.Date value. The driver converts this to a SQL DATE value when
   * it sends it to the database.
   *
   * @param i - the first parameter is 1, the second is 2, ...
   * @param date - the parameter value
   * @param calendar - the calendar used
   * @exception SQLException - if a database-access error occurs.
   */
  public void setDate(int i, Date date, Calendar calendar)
    throws SQLException;

  /**
   * Set a parameter to a Java double value.
   *
   * @param i - the first parameter is 1, the second is 2, ...
   * @param d - the parameter value
   * @exception SQLException - if a database-access error occurs.
   */
  public void setDouble(int i, double d)
    throws SQLException;

  /**
   * If escape scanning is on (the default), the driver will do escape substitution before sending the SQL to the database.
   *
   * @param flag - true to enable; false to disable
   * @exception SQLException - if a database-access error occurs.
   */
  public void setEscapeProcessing(boolean flag)
    throws SQLException;

  /**
   * Set a parameter to a Java float value. The driver converts this to a SQL FLOAT value when it sends it to the database.
   *
   * @param i - the first parameter is 1, the second is 2, ...
   * @param f - the parameter value
   * @exception SQLException - if a database-access error occurs.
   */
  public void setFloat(int i, float f)
    throws SQLException;

  /**
   * Set a parameter to a Java int value.
   *
   * @param i - the first parameter is 1, the second is 2, ...
   * @param j - the parameter value
   * @exception SQLException - if a database-access error occurs.
   */
  public void setInt(int i, int j)
    throws SQLException;

  /**
   * Set a parameter to a Java long value. 
   *
   * @param i - the first parameter is 1, the second is 2, ...
   * @param j - the parameter value
   * @exception SQLException - if a database-access error occurs.
   */
  public void setLong(int i, long j)
    throws SQLException;

  /**
   * The maxFieldSize limit (in bytes) is set to limit the size of data that can be returned for any column value;
   * it only applies to BINARY, VARBINARY, LONGVARBINARY, CHAR, VARCHAR, and LONGVARCHAR fields. If the limit is exceeded,
   * the excess data is silently discarded. For maximum portability use values greater than 256.
   *
   * @param i - the new max column size limit; zero means unlimited
   * @exception SQLException - if a database-access error occurs.
   */
  public void setMaxFieldSize(int i)
    throws SQLException;

  /**
   * The maxRows limit is set to limit the number of rows that any RowSet can contain. If the limit is exceeded,
   * the excess rows are silently dropped.
   *
   * @param i - the new max rows limit; zero means unlimited
   * @exception SQLException - if a database-access error occurs.
   */
  public void setMaxRows(int i)
    throws SQLException;

  /**
   * <p>Set a parameter to SQL NULL.</p>
   *
   * <p><b>Note:</b> You must specify the parameter's SQL type.</p>
   *
   * @param parameterIndex - the first parameter is 1, the second is 2, ...
   * @param sqlType - SQL type code defined by java.sql.Types
   * @exception SQLException - if a database-access error occurs.
   */
  public void setNull(int parameterIndex, int sqlType)
    throws SQLException;

  /**
   * <p>JDBC 2.0 Set a parameter to SQL NULL. This version of setNull should be used for user-named types and
   * REF type parameters. Examples of user-named types include: STRUCT, DISTINCT, JAVA_OBJECT, and named array types.</p>
   *
   * <p><b>Note:</b> To be portable, applications must give the SQL type code and the fully qualified SQL type name when
   * specifying a NULL user-named or REF parameter. In the case of a user-named type the name is the type name of the
   * parameter itself. For a REF parameter the name is the type name of the referenced type. If a JDBC driver does not
   * need the type code or type name information, it may ignore it. Although it is intended for user-named and Ref parameters,
   * this method may be used to set a null parameter of any JDBC type. If the parameter does not have a user-named or REF
   * type then the typeName is ignored.</p>
   *
   * @param paramIndex - the first parameter is 1, the second is 2, ...
   * @param sqlType - a value from java.sql.Types
   * @param typeName - the fully qualified name of a SQL user-named type, ignored if the parameter is not a user-named type or REF
   * @exception SQLException - if a database-access error occurs.
   */
  public void setNull(int paramIndex, int sqlType, String typeName)
    throws SQLException;

  /**
   * <p>Set the value of a parameter using an object; use the java.lang equivalent objects for integral values.</p>
   *
   * <p>The JDBC specification specifies a standard mapping from Java Object types to SQL types. The given argument
   * java object will be converted to the corresponding SQL type before being sent to the database.</p>
   *
   * <p>Note that this method may be used to pass datatabase specific abstract data types, by using a Driver specific
   * Java type. If the object is of a class implementing SQLData, the rowset should call its method writeSQL() to write
   * it to the SQL data stream. else If the object is of a class implementing Ref, Blob, Clob, Struct, or Array then pass
   * it to the database as a value of the corresponding SQL type. Raise an exception if there is an ambiguity, for example,
   * if the object is of a class implementing more than one of those interfaces.</p>
   *
   * @param parameterIndex - The first parameter is 1, the second is 2, ...
   * @param object - The object containing the input parameter value
   * @exception SQLException - if a database-access error occurs.
   */
  public void setObject(int parameterIndex, Object object)
    throws SQLException;

  /**
   * This method is like setObject above, but the scale used is the scale of the second parameter. Scalar values have a scale
   * of zero. Literal values have the scale present in the literal. While it is supported, it is not recommended that this
   * method not be called with floating point input values.
   *
   * @param parameterIndex - The first parameter is 1, the second is 2, ...
   * @param object - The object containing the input parameter value
   * @param targetSqlType - The SQL type (as defined in java.sql.Types) to be sent to the database. The scale argument may
   * further qualify this type.
   * @exception SQLException - if a database-access error occurs.
   */
  public void setObject(int parameterIndex, Object object, int targetSqlType)
    throws SQLException;

  /**
   * <p>Set the value of a parameter using an object; use the java.lang equivalent objects for integral values.</p>
   *
   * <p>The given Java object will be converted to the targetSqlType before being sent to the database. If the object
   * is of a class implementing SQLData, the rowset should call its method writeSQL() to write it to the SQL data stream.
   * else If the object is of a class implementing Ref, Blob, Clob, Struct, or Array then pass it to the database as
   * a value of the corresponding SQL type.</p>
   *
   * <p>Note that this method may be used to pass datatabase- specific abstract data types.</p>
   *
   * @param parameterIndex - The first parameter is 1, the second is 2, ...
   * @param object - The object containing the input parameter value
   * @param j - The SQL type (as defined in java.sql.Types) to be sent to the database. The scale argument may further qualify this type.
   * @param scale -  For java.sql.Types.DECIMAL or java.sql.Types.NUMERIC types this is the number of digits after the decimal.
   * For all other types this value will be ignored
   * @exception SQLException - if a database-access error occurs.
   */
  public void setObject(int parameterIndex, Object object, int j, int scale)
    throws SQLException;

  /**
   * Set the password.
   *
   * @param string - the password string
   * @exception SQLException - if a database-access error occurs.
   */
  public void setPassword(String string)
    throws SQLException;

  /**
   * The queryTimeout limit is the number of seconds the driver will wait for a Statement to execute.
   * If the limit is exceeded, a SQLException is thrown.
   *
   * @param seconds - the new query timeout limit in seconds; zero means unlimited
   * @exception SQLException - if a database-access error occurs.
   */
  public void setQueryTimeout(int seconds)
    throws SQLException;

  /**
   * Set the read-onlyness of the rowset
   *
   * @param flag - true if read-only, false otherwise
   * @exception SQLException - if a database-access error occurs.
   */
  public void setReadOnly(boolean flag)
    throws SQLException;

  /**
   * Set a REF(&lt;structured-type&gt;) parameter. 
   *
   * @param i - the first parameter is 1, the second is 2, ...
   * @param ref - an object representing data of an SQL REF Type
   * @exception SQLException - if a database-access error occurs.
   */
  public void setRef(int i, Ref ref)
    throws SQLException;

  /**
   * Set a parameter to a Java short value.
   *
   * @param i - the first parameter is 1, the second is 2, ...
   * @param s - the parameter value
   * @exception SQLException - if a database-access error occurs.
   */
  public void setShort(int i, short s)
    throws SQLException;

  /**
   * Set a parameter to a Java String value.
   *
   * @param parameterIndex - the first parameter is 1, the second is 2, ...
   * @param string - the parameter value
   * @exception SQLException - if a database-access error occurs.
   */
  public void setString(int parameterIndex, String string)
    throws SQLException;

  /**
   * Set a parameter to a java.sql.Time value. 
   *
   * @param parameterIndex - the first parameter is 1, the second is 2, ...
   * @param time - the parameter value
   * @exception SQLException - if a database-access error occurs.
   */
  public void setTime(int parameterIndex, Time time)
    throws SQLException;

  /**
   * Set a parameter to a java.sql.Time value. 
   *
   * @param parameterIndex - the first parameter is 1, the second is 2, ...
   * @param time - the parameter value
   * @param calendar - the calendar used
   * @exception SQLException - if a database-access error occurs.
   */
  public void setTime(int parameterIndex, Time time, Calendar calendar)
    throws SQLException;

  /**
   * Set a parameter to a java.sql.Timestamp value.
   *
   * @param parameterIndex - the first parameter is 1, the second is 2, ...
   * @param timestamp - the parameter value
   * @exception SQLException - if a database-access error occurs.
   */
  public void setTimestamp(int parameterIndex, Timestamp timestamp)
    throws SQLException;

  /**
   * Set a parameter to a java.sql.Timestamp value.
   *
   * @param parameterIndex - the first parameter is 1, the second is 2, ...
   * @param timestamp - the parameter value
   * @param calendar - the calendar used
   * @exception SQLException - if a database-access error occurs.
   */
  public void setTimestamp(int parameterIndex, Timestamp timestamp, Calendar calendar)
    throws SQLException;

  /**
   * Set the transaction isolation.
   *
   * @param level - the transaction isolation level
   * @exception SQLException - if a database-access error occurs.
   */
  public void setTransactionIsolation(int level)
    throws SQLException;

  /**
   * Set the rowset type.
   *
   * @param i - a value from ResultSet.TYPE_XXX
   * @exception SQLException - if a database-access error occurs.
   */
  public void setType(int i)
    throws SQLException;

  /**
   * Install a type-map object as the default type-map for this rowset.
   *
   * @param map - a map object
   * @exception SQLException - if a database-access error occurs.
   */
  public void setTypeMap(Map map)
    throws SQLException;

  /**
   * Set the url used to create a connection. Setting this property is optional. If a url is used, a JDBC driver that
   * accepts the url must be loaded by the application before the rowset is used to connect to a database. The rowset
   * will use the url internally to create a database connection when reading or writing data. Either a url or a data
   * source name is used to create a connection, whichever was specified most recently.
   *
   * @param url - a string value, may be null
   * @exception SQLException - if a database-access error occurs.
   */
  public void setUrl(String url)
    throws SQLException;

  /**
   * Set the user name.
   *
   * @param name - a user name
   * @exception SQLException - if a database-access error occurs.
   */
  public void setUsername(String name)
    throws SQLException;
}
