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

import java.util.EventListener;

/**
 * The RowSetListener interface is implemented by a component that wants to be notified when
 * a significant event happens in the life of a RowSet
 */
public interface RowSetListener extends EventListener {

  /**
   * Called when a rowset's cursor is moved.
   *
   * @param rowSetEvent - an object describing the event
   */
  public void cursorMoved(RowSetEvent rowSetEvent);

  /**
   * Called when a row is inserted, updated, or deleted.
   *
   * @param rowSetEvent - an object describing the event
   */
  public void rowChanged(RowSetEvent rowSetEvent);

  /**
   * Called when the rowset is changed.
   *
   * @param rowSetEvent - an object describing the event
   */
  public void rowSetChanged(RowSetEvent rowSetEvent);
}
