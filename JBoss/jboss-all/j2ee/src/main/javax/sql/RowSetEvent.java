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

import java.util.EventObject;

/**
 * A RowSetEvent is generated when something important happens in the life of a rowset, like when a column value changes.
 */
public class RowSetEvent extends EventObject {

  /**
   * Construct a RowSetEvent object.
   *
   * @param rowSet - the source of the event
   */
  public RowSetEvent(RowSet rowSet) {
    super(rowSet);
  }
}
