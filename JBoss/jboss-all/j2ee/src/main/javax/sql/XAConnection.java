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

import java.sql.SQLException;
import javax.transaction.xa.XAResource;

/**
 * An XAConnection object provides support for distributed transactions. An XAConnection may be
 * enlisted in a distributed transaction by means of an XAResource object.
 */
public interface XAConnection extends PooledConnection {

  /**
   * Return an XA resource to the caller.
   *
   * @return the XAResource
   * @exception SQLException - if a database-access error occurs
   */
  public XAResource getXAResource()
    throws SQLException;
}
