/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.jdbc;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Enumeration;
import java.util.List;

import javax.ejb.FinderException;

import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.logging.Logger;

/**
 * CMPStoreManager CustomFindByEntitiesCommand.
 * Implements bridge for custom implemented finders in container managed entity
 * beans. These methods are called ejbFindX in the EJB implementation class,
 * where X can be anything. Such methods are called findX in the Home and/or 
 * the LocalHome interface. 
 *
 * @see org.jboss.ejb.plugins.cmp.jdbc.JDBCFindEntitiesCommand
 * @author <a href="mailto:michel.anke@wolmail.nl">Michel de Groot</a>
 * @version $Revision: 1.8 $
 */
public class JDBCCustomFinderQuery implements JDBCQueryCommand {
   private Logger log;

   /**
    *  method that implements the finder
    */
   private Method finderMethod;

   /**
    * Constructs a command which can handle multiple entity finders
    * that are BMP implemented.
    * @param finderMethod the EJB finder method implementation
    */
   public JDBCCustomFinderQuery(JDBCStoreManager manager, Method finderMethod) {
      this.finderMethod = finderMethod;
      this.log = Logger.getLogger(
            this.getClass().getName() + 
            "." + 
            manager.getMetaData().getName() +
            "." + 
            finderMethod.getName());

      log.debug("Finder: Custom finder " + finderMethod.getName());
   }

   public Collection execute(
         Method unused,
         Object[] args,
         EntityEnterpriseContext ctx) throws FinderException {

      try {
         // invoke implementation method on ejb instance
         Object value = finderMethod.invoke(ctx.getInstance(), args);

         // if expected return type is Collection, return as is
         // if expected return type is not Collection, wrap value in Collection
         if(value instanceof Enumeration) {
            Enumeration enum = (Enumeration)value;
            List result = new ArrayList();
            while(enum.hasMoreElements()) {
               result.add(enum.nextElement());
            }
            return result;
         } else if(value instanceof Collection) {
            return (Collection)value;
         } else {
            return Collections.singleton(value);
         }
      } catch(IllegalAccessException e) {
         log.error("Error invoking custom finder " + finderMethod.getName(), e);
         throw new FinderException("Unable to access finder implementation: " +
               finderMethod.getName());
      } catch(IllegalArgumentException e) {
         log.error("Error invoking custom finder " + finderMethod.getName(), e);
         throw new FinderException("Illegal arguments for finder " +
               "implementation: " + finderMethod.getName());
      } catch(InvocationTargetException e) {
         log.error("Error invoking custom finder " + finderMethod.getName(), 
               e.getTargetException());
         throw new FinderException("Errror invoking cutom finder " + 
               finderMethod.getName() + ": " + e.getTargetException());
      }
   }

}
