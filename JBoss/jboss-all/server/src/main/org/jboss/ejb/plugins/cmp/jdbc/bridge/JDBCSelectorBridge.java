/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.jdbc.bridge;

/**
 * JDBCSelectorBridge represents one ejbSelect method. 
 *
 * Life-cycle:
 *      Tied to the EntityBridge.
 *
 * Multiplicity:   
 *      One for each entity bean ejbSelect method.       
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.7 $
 */                            
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.Set;
import javax.ejb.EJBException;
import javax.ejb.FinderException;
import javax.ejb.ObjectNotFoundException;
import org.jboss.ejb.plugins.cmp.bridge.SelectorBridge;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCQueryCommand;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCQueryMetaData;

public class JDBCSelectorBridge implements SelectorBridge {
   protected final JDBCQueryMetaData queryMetaData;
   protected final JDBCStoreManager manager;
   
   public JDBCSelectorBridge(
         JDBCStoreManager manager, 
         JDBCQueryMetaData queryMetaData) {

      this.queryMetaData = queryMetaData;      
      this.manager = manager;
   }
   
   public String getSelectorName() {
      return queryMetaData.getMethod().getName();
   }
   
   public Method getMethod() {
      return queryMetaData.getMethod();
   }
   
   public Class getReturnType() {
      return queryMetaData.getMethod().getReturnType();
   }
      
   public Object execute(Object[] args) throws FinderException {
      Collection retVal = null;
      try {
         JDBCQueryCommand query = 
               manager.getQueryManager().getQueryCommand(getMethod());
         retVal = query.execute(getMethod(), args, null);
      } catch(FinderException e) {
         throw e;
      } catch(EJBException e) {
         throw e;
      } catch(Exception e) {
         throw new EJBException("Error in " + getSelectorName(), e);
      }
         
      if(!Collection.class.isAssignableFrom(getReturnType())) {
         // single object
         if(retVal.size() == 0) {
            throw new ObjectNotFoundException();
         }
         if(retVal.size() > 1) {
            throw new FinderException(getSelectorName() + 
                  " returned " + retVal.size() + " objects");
         }
         return retVal.iterator().next();
      } else {
         // collection or set
         if(Set.class.isAssignableFrom(getReturnType())) {
            return new HashSet(retVal);
         } else {
            return new ArrayList(retVal);
         }
      }
   }
}
