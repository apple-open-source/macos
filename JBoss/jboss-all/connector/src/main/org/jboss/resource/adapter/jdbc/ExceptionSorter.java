package org.jboss.resource.adapter.jdbc;

import java.sql.SQLException;



/**
 * ExceptionSorter.java
 *
 *
 * Created: Fri Mar 14 21:53:08 2003
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version 1.0
 */

public interface ExceptionSorter
{
   boolean isExceptionFatal(SQLException e);
}// ExceptionSorter
