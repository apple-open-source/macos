
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.resource.adapter.jdbc;

import org.jboss.util.LRUCachePolicy;
import org.jboss.logging.Logger;

import java.sql.SQLException;
import java.sql.PreparedStatement;

/**
 *  LRU cache for PreparedStatements.  When ps ages out, close it.
 *
 * Created: Mon Aug 12 21:53:02 2002
 *
 * @author <a href="mailto:bill@jboss.org">Bill Burke</a>
 * @version $Revision: 1.1.2.2 $
 */
public class PreparedStatementCache extends LRUCachePolicy
{

   private final Logger log = Logger.getLogger(getClass());

   public PreparedStatementCache(int max)
   {
      super(2, max);
      create();
   }

   protected void ageOut(LRUCachePolicy.LRUCacheEntry entry)
   {
      try
      {
         CachedPreparedStatement ws = (CachedPreparedStatement) entry.m_object;
         PreparedStatement ps = ws.getUnderlyingPreparedStatement();
         ps.close();
      }
      catch (SQLException e)
      {
         log.error("Failed closing cached statement", e);
      }
      finally
      {
         super.ageOut(entry);
      }
   }

}
