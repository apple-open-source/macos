package org.jboss.resource.adapter.jdbc;

import java.sql.Connection;
import java.sql.SQLException;

/**
 * Does not check the connection
 *
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class NullValidConnectionChecker
   implements ValidConnectionChecker
{
   public SQLException isValidConnection(Connection c)
   {
      return null;
   }
}
