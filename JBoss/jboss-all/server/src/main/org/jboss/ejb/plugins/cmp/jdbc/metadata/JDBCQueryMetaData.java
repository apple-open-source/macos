/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.metadata;

import java.lang.reflect.Method;

/**
 * This interface is used to identify a query that will be invoked in
 * responce to the invocation of a finder method in a home interface or
 * an ejbSelect method in a bean implementation class.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @author <a href="danch@nvisia.com">danch</a>
 * @author <a href="on@ibis.odessa.ua">Oleg Nitz</a>
 * @version $Revision: 1.6 $
 */
public interface JDBCQueryMetaData {
   /**
    * Gets the method which invokes this query.
    * @return the Method object which invokes this query
    */
   public Method getMethod();

   /**
    * Is the result set of ejbSelect is mapped to local ejb objects or 
    * remote ejb objects.
    * @return true, if the result set is to be local objects
    */
   public boolean isResultTypeMappingLocal();

   /**
    * Gets the read ahead metadata for the query.
    * @return the read ahead metadata for the query.
    */
   public JDBCReadAheadMetaData getReadAhead();
}
