/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 * 2001/04/08: kjenks: Initial author
 * 2001/06/14: jpedersen: Updated javadoc
 */
package javax.sql;

import java.sql.SQLException;
import java.util.EventObject;

/**
 * The ConnectionEvent class provides information about the source of a connection related event.
 * ConnectionEvent objects provide the following information:
 * <ul>
 * <li>the pooled connection that generated the event 
 * <li>the SQLException about to be thrown to the application ( in the case of an error event)
 * </ul>
 */
public class ConnectionEvent extends EventObject {
  private SQLException ex;
  
  /**
   * Construct a ConnectionEvent object. SQLException defaults to null.
   *
   * @param pooledConnection - the pooled connection that is the source of the event
   */
  public ConnectionEvent(PooledConnection pooledConnection) {
    super(pooledConnection);
    ex = null;
  }

  /**
   * Construct a ConnectionEvent object.
   *
   * @param pooledConnection - the pooled connection that is the source of the event
   * @param e - the SQLException about to be thrown to the application
   */
  public ConnectionEvent(PooledConnection pooledConnection, SQLException e) {
    super(pooledConnection);
    ex = null;
    ex = e;
  }

  /**
   * Gets the SQLException about to be thrown. May be null.
   *
   * @return The SQLException about to be thrown
   */
  public SQLException getSQLException() {
    return ex;
  }
}
