/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.jaws.bmp;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

import java.util.Collection;
import java.util.ArrayList;

import javax.ejb.FinderException;

import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.logging.Logger;

import org.jboss.ejb.plugins.jaws.JPMFindEntitiesCommand;

/**
 * JAWSPersistenceManager CustomFindByEntitiesCommand.
 * Implements bridge for custom implemented finders in container managed entity beans.
 * These methods are called ejbFindX in the EJB implementation class, where X can be
 * anything. Such methods are called findX in the Home interface. The EJB implementation
 * must return a Collection of primary keys.
 *
 * @see org.jboss.ejb.plugins.jaws.jdbc.JDBCFindEntitiesCommand
 * @author <a href="mailto:michel.anke@wolmail.nl">Michel de Groot</a>
 * @version $Revision: 1.13 $
 */
public class CustomFindByEntitiesCommand
   implements JPMFindEntitiesCommand
{
   // Attributes ----------------------------------------------------
   static Logger log = Logger.getLogger(CustomFindByEntitiesCommand.class);

   /** The method implementing the finder. */
   protected Method finderImplMethod;

   /** Command name, used for debug trace. */
   protected String name;

   // Constructors --------------------------------------------------

   /**
    * Constructs a JAWS command which can handle multiple entity finders
    * that are BMP implemented.
    *
    * @param finderMethod the EJB finder method implementation
    */
   public CustomFindByEntitiesCommand(Method finderMethod)
      throws IllegalArgumentException
   {
      finderImplMethod = finderMethod;
      // set name for debugging purposes
      name = "Custom finder "+finderMethod.getName();
      if (log.isDebugEnabled())
         log.debug("Finder:"+name);
   }

   // JPMFindEntitiesCommand implementation -------------------------

   public Collection execute(Method finderMethod,
                                Object[] args,
                                EntityEnterpriseContext ctx)
      throws Exception
   {

      // invoke implementation method on ejb instance
      try {
         // if expected return type is Collection, return as is
         // if expected return type is not Collection, wrap result in Collection
         if (finderMethod.getReturnType().equals(Collection.class))  {
            return (Collection)finderImplMethod.invoke(ctx.getInstance(),args);
         } else {
            Collection coll = new ArrayList(1);
            coll.add(finderImplMethod.invoke(ctx.getInstance(),args));
            return coll;
         }
      } catch (IllegalAccessException e1) {
         throw new FinderException("Unable to access finder implementation:"+finderImplMethod.getName());
      } catch (IllegalArgumentException e2) {
         throw new FinderException("Illegal arguments for finder implementation:"+finderImplMethod.getName());
      } catch (ExceptionInInitializerError e5) {
         throw new FinderException("Unable to initialize finder implementation:"+finderImplMethod.getName());
        } catch (InvocationTargetException e) {
            Throwable target  = e.getTargetException();
            if(target instanceof Exception) {
                throw (Exception)target;
            }
            throw new FinderException("Unable to initialize finder implementation: " + finderImplMethod.getName());
        }
   }
}
