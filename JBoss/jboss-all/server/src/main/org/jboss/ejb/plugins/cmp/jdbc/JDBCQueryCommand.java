/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.jdbc;

import java.lang.reflect.Method;
import java.util.Collection;
import javax.ejb.FinderException;
import org.jboss.ejb.EntityEnterpriseContext;

/**
 * Common interface for all query commands.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.5 $
 */
public interface JDBCQueryCommand {
   public Collection execute(
         Method finderMethod,
         Object[] args,
         EntityEnterpriseContext ctx) throws FinderException;
}
