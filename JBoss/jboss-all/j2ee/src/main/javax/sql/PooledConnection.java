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

import java.sql.Connection;
import java.sql.SQLException;

/**
 * A PooledConnection object is a connection object that provides hooks for connection pool management.
 * A PooledConnection object represents a physical connection to a data source.
 */
public interface PooledConnection {

  /**
   * Add an event listener.
   *
   * @param connectionEventListener - The listener
   */
  public void addConnectionEventListener(ConnectionEventListener connectionEventListener);

  /**
   * Close the physical connection.
   *
   * @exception SQLException - if a database-access error occurs.
   */
  public void close()
    throws SQLException;

  /**
   * Create an object handle for this physical connection. The object returned is a temporary handle used by
   * application code to refer to a physical connection that is being pooled.
   *
   * @return a Connection object
   * @exception SQLException - if a database-access error occurs.
   */
  public Connection getConnection()
    throws SQLException;

  /**
   * Remove an event listener.
   *
   * @param connectionEventListener - The listener
   */
  public void removeConnectionEventListener(ConnectionEventListener connectionEventListener);
}
