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

/**
 * An object that implements the RowSetWriter interface may be registered with a RowSet object
 * that supports the reader/writer paradigm. The RowSetWriter.writeRow() method is called internally
 * by a RowSet that supports the reader/writer paradigm to write the contents of the rowset to a
 * data source.
 */
public interface RowSetWriter {

  /**
   * This method is called to write data to the data source that is backing the rowset.
   *
   * @param rowSetInternal - the calling rowset
   * @return true if the row was written, false if not due to a conflict
   * @exception SQLException - if a database-access error occur
   */
  public boolean writeData(RowSetInternal rowSetInternal)
    throws SQLException;
}
