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

import java.io.PrintWriter;
import java.sql.SQLException;

/**
 * A factory for XAConnection objects. An object that implements the XADataSource interface
 * is typically registered with a JNDI service provider. 
 */
public interface XADataSource {

  /**
   * <p>Get the log writer for this data source.</p>
   *
   * <p>The log writer is a character output stream to which all logging and tracing messages
   * for this data source object instance will be printed. This includes messages printed by the
   * methods of this object, messages printed by methods of other objects manufactured by this object,
   * and so on. Messages printed to a data source specific log writer are not printed to the log writer
   * associated with the java.sql.Drivermanager class. When a data source object is created the log
   * writer is initially null, in other words, logging is disabled.</p>
   *
   * @return the log writer for this data source, null if disabled
   * @exception SQLException - if a database-access error occurs.
   */
  public PrintWriter getLogWriter()
    throws SQLException;

  /**
   * Gets the maximum time in seconds that this data source can wait while attempting to connect to
   * a database. A value of zero means that the timeout is the default system timeout if there is one;
   * otherwise it means that there is no timeout. When a data source object is created the login timeout
   * is initially zero.
   *
   * @return the data source login time limit
   * @exception SQLException - if a database-access error occurs.
   */
  public int getLoginTimeout()
    throws SQLException;

  /**
   * Attempt to establish a database connection.
   *
   * @return a Connection to the database
   * @exception SQLException - if a database-access error occurs.
   */
  public XAConnection getXAConnection()
    throws SQLException;

  /**
   * Attempt to establish a database connection.
   *
   * @param user - the database user on whose behalf the Connection is being made
   * @param password - the user's password
   * @return a Connection to the database
   * @exception SQLException - if a database-access error occurs.
   */
  public XAConnection getXAConnection(String user, String password)
    throws SQLException;

  /**
   * <p>Set the log writer for this data source.</p>
   *
   * <p>The log writer is a character output stream to which all logging and tracing messages
   * for this data source object instance will be printed. This includes messages printed by the
   * methods of this object, messages printed by methods of other objects manufactured by this object,
   * and so on. Messages printed to a data source specific log writer are not printed to the log writer
   * associated with the java.sql.Drivermanager class. When a data source object is created the log writer
   * is initially null, in other words, logging is disabled.</p>
   *
   * @param printWriter - the new log writer; to disable, set to null
   * @exception SQLException - if a database-access error occurs.
   */
  public void setLogWriter(PrintWriter printWriter)
    throws SQLException;

  /**
   * Sets the maximum time in seconds that this data source will wait while attempting to connect
   * to a database. A value of zero specifies that the timeout is the default system timeout if there
   * is one; otherwise it specifies that there is no timeout. When a data source object is created the
   * login timeout is initially zero.
   *
   * @param sec - the data source login time limit
   * @exception SQLException - if a database-access error occurs.
   */
  public void setLoginTimeout(int sec)
    throws SQLException;
}
