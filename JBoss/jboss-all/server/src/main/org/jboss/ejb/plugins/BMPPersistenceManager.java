/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.ejb.plugins;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
import java.rmi.RemoteException;
import java.util.Collection;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.Iterator;
import java.util.HashMap;

import javax.ejb.EntityBean;
import javax.ejb.CreateException;
import javax.ejb.FinderException;
import javax.ejb.RemoveException;
import javax.ejb.EJBException;
import javax.ejb.EJBObject;

import org.jboss.ejb.Container;
import org.jboss.ejb.EntityContainer;
import org.jboss.ejb.EntityCache;
import org.jboss.ejb.EntityPersistenceManager;
import org.jboss.ejb.EntityEnterpriseContext;

import org.jboss.logging.Logger;
import org.jboss.metadata.ConfigurationMetaData;

/**
*  Persistence manager for BMP entites.  All calls are simply deligated
*  to the entity implementation class.
*
*  @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
*  @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
*  @author <a href="mailto:andreas.schaefer@madplanet.com">Andreas Schaefer</a>
*  @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
*  @version $Revision: 1.43.2.4 $
*/
public class BMPPersistenceManager
   implements EntityPersistenceManager
{
   // Constants -----------------------------------------------------
   private final static Object[] EMPTY_OBJECT_ARRAY = new Object[0];
   // Attributes ----------------------------------------------------
   Logger log = Logger.getLogger(BMPPersistenceManager.class);

   EntityContainer con;

   Method ejbLoad;
   Method ejbStore;
   Method ejbActivate;
   Method ejbPassivate;
   Method ejbRemove;

   /**
    *  Optional isModified method used by storeEntity
    */
   Method isModified;

   HashMap createMethods = new HashMap();
   HashMap postCreateMethods = new HashMap();
   HashMap finderMethods = new HashMap();
   private int commitOption;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   // Public --------------------------------------------------------
   public void setContainer(Container c)
   {
      con = (EntityContainer)c;
      if( con != null )
      {
         ConfigurationMetaData configuration = con.getBeanMetaData().getContainerConfiguration();
         commitOption = configuration.getCommitOption();
      }
   }

   public void create()
   throws Exception
   {
      ejbLoad = EntityBean.class.getMethod("ejbLoad", new Class[0]);
      ejbStore = EntityBean.class.getMethod("ejbStore", new Class[0]);
      ejbActivate = EntityBean.class.getMethod("ejbActivate", new Class[0]);
      ejbPassivate = EntityBean.class.getMethod("ejbPassivate", new Class[0]);
      ejbRemove = EntityBean.class.getMethod("ejbRemove", new Class[0]);

      // Create cache of create methods
      if (con.getHomeClass() != null)
      {
         Method[] methods = con.getHomeClass().getMethods();
         createMethodCache( methods );
      }
      if (con.getLocalHomeClass() != null)
      {
         Method[] methods = con.getLocalHomeClass().getMethods();
         createMethodCache( methods );
      }

      try
      {
         isModified = con.getBeanClass().getMethod("isModified", new Class[0]);
         if (!isModified.getReturnType().equals(Boolean.TYPE))
            isModified = null; // Has to have "boolean" as return type!
      }
      catch (NoSuchMethodException ignored) {}
   }

    /**
    * Returns a new instance of the bean class or a subclass of the bean class.
    *
    * @return the new instance
    */
    public Object createBeanClassInstance() throws Exception {
        return con.getBeanClass().newInstance();
    }

   private void createMethodCache( Method[] methods )
      throws NoSuchMethodException
   {
      for (int i = 0; i < methods.length; i++)
      {
         String name = methods[i].getName();
         if (name.startsWith("create"))
         {
            String nameSuffix = name.substring(0, 1).toUpperCase() + name.substring(1);
            try
            {
               createMethods.put(methods[i], con.getBeanClass().getMethod("ejb" + nameSuffix, methods[i].getParameterTypes()));
            }
            catch (NoSuchMethodException e)
            {
               log.error("Home Method " + methods[i] + " not implemented in bean class " + con.getBeanClass() + " looking for method named: ejb" + nameSuffix);

               throw e;
            }
            try
            {
               postCreateMethods.put(methods[i], con.getBeanClass().getMethod("ejbPost" + nameSuffix, methods[i].getParameterTypes()));
            }
            catch (NoSuchMethodException e)
            {
               log.error("Home Method " + methods[i] + " not implemented in bean class " + con.getBeanClass() + " looking for method named: ejbPost" + nameSuffix);
               throw e;
            }
         }
      }

      // Create cache of finder methods
      for (int i = 0; i < methods.length; i++)
      {
         if (methods[i].getName().startsWith("find"))
         {
            try
            {
               finderMethods.put(methods[i], con.getBeanClass().getMethod("ejbF" + methods[i].getName().substring(1), methods[i].getParameterTypes()));
            }
            catch (NoSuchMethodException e)
            {
               log.error("Home Method " + methods[i] + " not implemented in bean class");
               throw e;
            }
         }
      }
   }

   public void start()
   {
   }

   public void stop()
   {
   }

   public void destroy()
   {
   }

   public void createEntity(
         Method m,
         Object[] args,
         EntityEnterpriseContext ctx)
   throws Exception
   {
      Object id = null;
      try
      {
         // Call ejbCreate<METHOD)
         Method createMethod = (Method)createMethods.get(m);
         id = createMethod.invoke(ctx.getInstance(), args);
      } catch (IllegalAccessException e)
      {
         // Throw this as a bean exception...(?)
         throw new EJBException(e);
      } catch (InvocationTargetException ite)
      {
         Throwable e = ite.getTargetException();
         if (e instanceof CreateException)
         {
            // Rethrow exception
            throw (CreateException)e;
         }
         else if (e instanceof RemoteException)
         {
            // Rethrow exception
            throw (RemoteException)e;
         }
         else if (e instanceof EJBException)
         {
            // Rethrow exception
            throw (EJBException)e;
         }
         else if (e instanceof RuntimeException)
         {
            // Wrap runtime exceptions
            throw new EJBException((Exception)e);
         }
         else if(e instanceof Exception)
         {
            throw (Exception)e;
         }
         else
         {
            throw (Error)e;
         }
      }

      // set the id
      ctx.setId(id);

      // Create a new CacheKey
      Object cacheKey = ((EntityCache) con.getInstanceCache()).createCacheKey( id );

      // Give it to the context
      ctx.setCacheKey(cacheKey);

      // Create EJBObject
      if (con.getProxyFactory() != null)
      {
         ctx.setEJBObject((EJBObject) con.getProxyFactory().getEntityEJBObject(cacheKey));
      }
      if (con.getLocalHomeClass() != null)
      {
         ctx.setEJBLocalObject(con.getLocalProxyFactory().getEntityEJBLocalObject(cacheKey));
      }
   }

   public void postCreateEntity(
         Method m,
         Object[] args,
         EntityEnterpriseContext ctx)
   throws Exception
   {
      try
      {
         Method postCreateMethod = (Method)postCreateMethods.get(m);
         postCreateMethod.invoke(ctx.getInstance(), args);
      } catch (IllegalAccessException e)
      {
         // Throw this as a bean exception...(?)
         throw new EJBException(e);
      } catch (InvocationTargetException ite)
      {
         Throwable e = ite.getTargetException();
         if (e instanceof CreateException)
         {
            // Rethrow exception
            throw (CreateException)e;
         }
         else if (e instanceof RemoteException)
         {
            // Rethrow exception
            throw (RemoteException)e;
         }
         else if (e instanceof EJBException)
         {
            // Rethrow exception
            throw (EJBException)e;
         }
         else if (e instanceof RuntimeException)
         {
            // Wrap runtime exceptions
            throw new EJBException((Exception)e);
         }
         else if(e instanceof Exception)
         {
            throw (Exception)e;
         }
         else
         {
            throw (Error)e;
         }
      }
   }

   public Object findEntity(Method finderMethod, Object[] args, EntityEnterpriseContext ctx)
   throws Exception
   {
      // call the finder method
      Object objectId = callFinderMethod(finderMethod, args, ctx);

      // get the cache, create a new key and return this new key
      return ((EntityCache)con.getInstanceCache()).createCacheKey( objectId );
   }

   public Collection findEntities(Method finderMethod, Object[] args, EntityEnterpriseContext ctx)
   throws Exception
   {
      // call the finder method
      Object result = callFinderMethod(finderMethod, args, ctx);

      if (result == null)
      {
         // for EJB 1.0 compliance
         // if the bean couldn't find any matching entities
         // it returns null, so we return an empty collection
         return new ArrayList();
      }

      if (result instanceof java.util.Enumeration)
      {
         // to preserve 1.0 spec compatiblity
         ArrayList array = new ArrayList();
         Enumeration enum = (Enumeration) result;
         while (enum.hasMoreElements() == true)
         {
            // Wrap a cache key around the given object id/primary key
            array.add(((EntityCache) con.getInstanceCache()).createCacheKey(enum.nextElement()));
         }
         return array;
      }
      else if (result instanceof java.util.Collection)
      {

         ArrayList array = new ArrayList(((Collection) result).size());
         Iterator enum =  ((Collection) result).iterator();
         while (enum.hasNext())
         {
            // Wrap a cache key around the given object id/primary key
            array.add(((EntityCache) con.getInstanceCache()).createCacheKey(enum.next()));
         }
         return array;
      }
      else
      {
         // so we received something that's not valid
         // throw an exception reporting it
         throw new EJBException("result of finder method is not a valid " +
               "return type: " + result.getClass());
      }
   }

   public void activateEntity(EntityEnterpriseContext ctx)
      throws RemoteException
   {
      // Create a new CacheKey
      Object id = ctx.getId();
      Object cacheKey = ((EntityCache) con.getInstanceCache()).createCacheKey( id );

      // Give it to the context
      ctx.setCacheKey(cacheKey);

      // Create EJBObject
      if (con.getProxyFactory() != null)
      {
         ctx.setEJBObject((EJBObject) con.getProxyFactory().getEntityEJBObject(cacheKey));
      }
      if (con.getLocalHomeClass() != null)
      {
         ctx.setEJBLocalObject(con.getLocalProxyFactory().getEntityEJBLocalObject(cacheKey));
      }

      try
      {
         ejbActivate.invoke(ctx.getInstance(), EMPTY_OBJECT_ARRAY);
      } catch (IllegalAccessException e)
      {
         // Throw this as a bean exception...(?)
         throw new EJBException(e);
      } catch (InvocationTargetException ite)
      {
         Throwable e = ite.getTargetException();
         if (e instanceof RemoteException)
         {
            // Rethrow exception
            throw (RemoteException)e;
         }
         else if (e instanceof EJBException)
         {
            // Rethrow exception
            throw (EJBException)e;
         }
         else if (e instanceof RuntimeException)
         {
            // Wrap runtime exceptions
            throw new EJBException((Exception)e);
         }
      }
   }

   public void loadEntity(EntityEnterpriseContext ctx)
   throws RemoteException
   {
      try
      {
         ejbLoad.invoke(ctx.getInstance(), EMPTY_OBJECT_ARRAY);
      } catch (IllegalAccessException e)
      {
         // Throw this as a bean exception...(?)
         throw new EJBException(e);
      } catch (InvocationTargetException ite)
      {
         Throwable e = ite.getTargetException();
         if (e instanceof RemoteException)
         {
            // Rethrow exception
            throw (RemoteException)e;
         }
         else if (e instanceof EJBException)
         {
            // Rethrow exception
            throw (EJBException)e;
         }
         else if (e instanceof RuntimeException)
         {
            // Wrap runtime exceptions
            throw new EJBException((Exception)e);
         }
      }
   }

   public boolean isModified(EntityEnterpriseContext ctx) throws Exception
   {
      if(isModified == null)
      {
         return true;
      }

      Boolean modified = (Boolean) isModified.invoke(ctx.getInstance(), EMPTY_OBJECT_ARRAY);
      return modified.booleanValue();
   }

   public void storeEntity(EntityEnterpriseContext ctx)
      throws RemoteException
   {
      try
      {
         ejbStore.invoke(ctx.getInstance(), EMPTY_OBJECT_ARRAY);
      } catch (IllegalAccessException e)
      {
         // Throw this as a bean exception...(?)
         throw new EJBException(e);
      } catch (InvocationTargetException ite)
      {
         Throwable e = ite.getTargetException();
         if (e instanceof RemoteException)
         {
            // Rethrow exception
            throw (RemoteException)e;
         }
         else if (e instanceof EJBException)
         {
            // Rethrow exception
            throw (EJBException)e;
         }
         else if (e instanceof RuntimeException)
         {
            // Wrap runtime exceptions
            throw new EJBException((Exception)e);
         }
      }
   }

   public void passivateEntity(EntityEnterpriseContext ctx)
   throws RemoteException
   {
      try
      {
         ejbPassivate.invoke(ctx.getInstance(), EMPTY_OBJECT_ARRAY);
      } catch (IllegalAccessException e)
      {
         // Throw this as a bean exception...(?)
         throw new EJBException(e);
      } catch (InvocationTargetException ite)
      {
         Throwable e = ite.getTargetException();
         if (e instanceof RemoteException)
         {
            // Rethrow exception
            throw (RemoteException)e;
         }
         else if (e instanceof EJBException)
         {
            // Rethrow exception
            throw (EJBException)e;
         }
         else if (e instanceof RuntimeException)
         {
            // Wrap runtime exceptions
            throw new EJBException((Exception)e);
         }
      }
   }

   public void removeEntity(EntityEnterpriseContext ctx)
   throws RemoteException, RemoveException
   {
      try
      {
         ejbRemove.invoke(ctx.getInstance(), EMPTY_OBJECT_ARRAY);
      } catch (IllegalAccessException e)
      {
         // Throw this as a bean exception...(?)
         throw new EJBException(e);
      } catch (InvocationTargetException ite)
      {
         Throwable e = ite.getTargetException();
         if (e instanceof RemoveException)
         {
            // Rethrow exception
            throw (RemoveException)e;
         }
         else if (e instanceof RemoteException)
         {
            // Rethrow exception
            throw (RemoteException)e;
         }
         else if (e instanceof EJBException)
         {
            // Rethrow exception
            throw (EJBException)e;
         }
         else if (e instanceof RuntimeException)
         {
            // Wrap runtime exceptions
            throw new EJBException((Exception)e);
         }
      }
   }

   // Z implementation ----------------------------------------------

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------
   private Object callFinderMethod(Method finderMethod, Object[] args, EntityEnterpriseContext ctx)
   throws Exception
   {
      // Check if findByPrimaryKey
      // If so we check if the entity is in cache first
      if (finderMethod.getName().equals("findByPrimaryKey")
          && commitOption != ConfigurationMetaData.B_COMMIT_OPTION
          && commitOption != ConfigurationMetaData.C_COMMIT_OPTION)
      {
         Object key = ctx.getCacheKey();
         if (key == null)
         {
            key = ((EntityCache)con.getInstanceCache()).createCacheKey(args[0]);
         }
         if (con.getInstanceCache().isActive(key))
            return args[0]; // Object is active -> it exists -> no need to call finder
      }

      // get the finder method
      Method callMethod = (Method)finderMethods.get(finderMethod);

      if (callMethod == null)
      {
         throw new EJBException("couldn't find finder method in bean class. " +
               finderMethod.toString());
      }

      // invoke the finder method
      Object result = null;
      try
      {
         result = callMethod.invoke(ctx.getInstance(), args);
      } catch (IllegalAccessException e)
        {
            // Throw this as a bean exception...(?)
            throw new EJBException(e);
      } catch (InvocationTargetException ite)
        {
            Throwable e = ite.getTargetException();
         if (e instanceof FinderException)
         {
            // Rethrow exception
            throw (FinderException)e;
         }
         else if (e instanceof RemoteException)
         {
            // Rethrow exception
            throw (RemoteException)e;
         }
         else if (e instanceof EJBException)
         {
            // Rethrow exception
            throw (EJBException)e;
         }
         else if (e instanceof RuntimeException)
         {
            // Wrap runtime exceptions
            throw new EJBException((Exception)e);
         }
         else if(e instanceof Exception)
         {
            throw (Exception)e;
         }
         else
         {
            throw (Error)e;
         }
      }

      return result;
   }


   // Inner classes -------------------------------------------------
}




