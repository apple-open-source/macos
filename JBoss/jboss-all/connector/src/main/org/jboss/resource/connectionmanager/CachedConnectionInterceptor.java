
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.resource.connectionmanager;

import java.lang.reflect.Method;
import java.rmi.RemoteException;
import java.util.Collection;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

import javax.ejb.RemoveException;
import javax.management.MBeanServer;
import javax.resource.ResourceException;

import org.jboss.ejb.Container;
import org.jboss.ejb.EnterpriseContext;
import org.jboss.ejb.EntityContainer;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.EntityPersistenceManager;
import org.jboss.ejb.plugins.AbstractInterceptor;
import org.jboss.invocation.Invocation;
import org.jboss.logging.Logger;
import org.jboss.metadata.ApplicationMetaData;
import org.jboss.metadata.BeanMetaData;
import org.jboss.metadata.ResourceRefMetaData;
import org.jboss.mx.util.JMXExceptionDecoder;
import org.jboss.mx.util.MBeanServerLocator;



/**
 * CachedConnectionInterceptor.java
 * 
 *
 * Created: Sat Jan 12 01:22:06 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author <a href="mailto:E.Guib@ceyoniq.com">Erwin Guib</a>
 * @version
 */

public class CachedConnectionInterceptor 
   extends AbstractInterceptor 
   implements EntityPersistenceManager
{

   private final CachedConnectionManager ccm;
   private final Logger log = Logger.getLogger(getClass());

   private Container container;

   private EntityPersistenceManager pm;

   // contains the JNDI names of unshareable resources
   private Set unsharableResources = new HashSet();

   public CachedConnectionInterceptor ()
      throws Exception
   {
      //ccm = CachedConnectionManager.getInstance();
      try 
      {
         MBeanServer server = MBeanServerLocator.locateJBoss();
         ccm = (CachedConnectionManager)server.getAttribute(
            CachedConnectionManagerMBean.OBJECT_NAME,
            "Instance");
      }
      catch (Exception e)
      {
         JMXExceptionDecoder.rethrow(e);
         throw e; //to keep the compiler happy about ccm initialization.
      } // end of try-catch
   }
   // implementation of org.jboss.system.Service interface

   /**
    *
    * @exception java.lang.Exception <description>
    */
   public void start() throws Exception
   {
      log.debug("start called in CachedConnectionInterceptor");
      if (container == null) 
      {
         log.info("container is null, can't steal persistence manager");
         return;
      } // end of if ()
      if (container instanceof EntityContainer) 
      {
         EntityContainer ec = (EntityContainer)container;
      
         if (ec.getPersistenceManager() == null) 
         {
            log.info("no persistence manager in container!");
            return;
         } // end of if ()
         if (ec.getPersistenceManager() == this) 
         {
            log.info(" persistence manager in container already set!");
            return;
         } // end of if ()
         pm = ec.getPersistenceManager();
         ec.setPersistenceManager(this);
      } // end of if ()


      // get the JNDI names for all resources that are referenced "Unshareable"
      BeanMetaData bmd = container.getBeanMetaData();
      ApplicationMetaData appMetaData = bmd.getApplicationMetaData();
      ResourceRefMetaData resRefMetaData;
      String jndiName;
      
      for (Iterator iter = bmd.getResourceReferences(); iter.hasNext(); )
      {
         resRefMetaData = (ResourceRefMetaData)iter.next();
         jndiName = resRefMetaData.getJndiName();
         if(jndiName == null)
         {
            jndiName = appMetaData.getResourceByName(resRefMetaData.getResourceName());
         }
         if(jndiName != null && resRefMetaData.isShareable() == false)
         {
            int i = jndiName.indexOf(':');
            if(jndiName.charAt(i + 1) == '/')
            {
               i++;
            }
            unsharableResources.add(jndiName.substring(i + 1));
         }
      }
      
   }

   /**
    *
    */
   public void stop()
   {
      if (container != null && pm != null && ((EntityContainer)container).getPersistenceManager() == this) 
      {
         ((EntityContainer)container).setPersistenceManager(pm);
         pm = null;
      } // end of if ()
      unsharableResources.clear();
   }

   /**
    *
    */
   public void destroy()
   {
      // TODO: implement this org.jboss.system.Service method
   }

   /**
    *
    * @exception java.lang.Exception <description>
    */
   public void create() throws Exception
   {
      // TODO: implement this org.jboss.system.Service method
   }
   // implementation of org.jboss.ejb.Interceptor interface

   /**
    *
    * @param param1 <description>
    * @return <description>
    * @exception java.lang.Exception <description>
    */
   public Object invoke(Invocation mi) throws Exception
   {
      Object key = ((EnterpriseContext) mi.getEnterpriseContext()).getInstance();
      ccm.pushMetaAwareObject(key, unsharableResources);
      try 
      {
         return getNext().invoke(mi);
      }
      finally
      {
         ccm.popMetaAwareObject(unsharableResources);
      } // end of try-catch
   }


   /**
    *
    * @param param1 <description>
    * @return <description>
    * @exception java.lang.Exception <description>
    */
   public Object invokeHome(Invocation mi) throws Exception
   {
      EnterpriseContext ctx = (EnterpriseContext) mi.getEnterpriseContext();
      if (ctx == null) 
      {
         return getNext().invokeHome(mi);
      } // end of if ()
      else
      {
         
         Object key = ctx.getInstance();
         ccm.pushMetaAwareObject(key, unsharableResources);
         try 
         {
            return getNext().invokeHome(mi);
         }
         finally
         {
            ccm.popMetaAwareObject(unsharableResources);
         } // end of try-catch
      
         

      } // end of else
   }

   // implementation of org.jboss.ejb.ContainerPlugin interface

   /**
    *
    * @param param1 <description>
    */
   public void setContainer(Container container)
   {
      this.container = container;
   }

   public Container getContainer()
   {
      return container;
   }

   //EntityPersistenceManager implementation.
   /**
    * Returns a new instance of the bean class or a subclass of the bean class.
    * 
    * @return the new instance
    */
   public Object createBeanClassInstance() throws Exception
   {
      return pm.createBeanClassInstance();
   }

   /**
    * This method is called whenever an entity is to be created. The
    * persistence manager is responsible for calling the ejbCreate methods
    * on the instance and to handle the results properly wrt the persistent
    * store.
    *
    * @param m           the create method in the home interface that was
    *                    called
    * @param args        any create parameters
    * @param instance    the instance being used for this create call
    */
   public void createEntity(Method m,
                     Object[] args,
                     EntityEnterpriseContext instance)
      throws Exception
   {
      pm.createEntity(m, args, instance);
   }

   public void postCreateEntity(Method m,
                     Object[] args,
                     EntityEnterpriseContext instance)
      throws Exception
   {
      pm.postCreateEntity(m, args, instance);
   }



   /**
    * This method is called when single entities are to be found. The
    * persistence manager must find out whether the wanted instance is
    * available in the persistence store, and if so it shall use the
    * ContainerInvoker plugin to create an EJBObject to the instance, which
    * is to be returned as result.
    *
    * @param finderMethod    the find method in the home interface that was
    *                        called
    * @param args            any finder parameters
    * @param instance        the instance to use for the finder call
    * @return                an EJBObject representing the found entity
    */
   public Object findEntity(Method finderMethod,
                     Object[] args,
                     EntityEnterpriseContext instance)
      throws Exception
   {
      return pm.findEntity(finderMethod, args, instance);
   }

   /**
    * This method is called when collections of entities are to be found. The
    * persistence manager must find out whether the wanted instances are
    * available in the persistence store, and if so it shall use the
    * ContainerInvoker plugin to create EJBObjects to the instances, which are
    * to be returned as result.
    *
    * @param finderMethod    the find method in the home interface that was
    *                        called
    * @param args            any finder parameters
    * @param instance        the instance to use for the finder call
    * @return                an EJBObject collection representing the found
    *                        entities
    */
   public Collection findEntities(Method finderMethod,
                           Object[] args,
                           EntityEnterpriseContext instance)
      throws Exception
   {
      return pm.findEntities(finderMethod, args, instance);
   }

   /**
    * This method is called when an entity shall be activated. The persistence
    * manager must call the ejbActivate method on the instance.
    *
    * @param instance    the instance to use for the activation
    * 
    * @throws RemoteException    thrown if some system exception occurs
    */
   public void activateEntity(EntityEnterpriseContext instance)
      throws RemoteException
   {
      pm.activateEntity(instance);
   }

   
   /**
    * This method is called whenever an entity shall be load from the
    * underlying storage. The persistence manager must load the state from
    * the underlying storage and then call ejbLoad on the supplied instance.
    *
    * @param instance    the instance to synchronize
    * 
    * @throws RemoteException    thrown if some system exception occurs
    */
   public void loadEntity(EntityEnterpriseContext instance)
      throws RemoteException
   {
      pm.loadEntity(instance);
   }

      
   /**
    * This method is used to determine if an entity should be stored.
    *
    * @param instance    the instance to check
    * @return true, if the entity has been modified
    * @throws Exception    thrown if some system exception occurs
    */
   public boolean isModified(EntityEnterpriseContext instance) throws Exception
   {
      return pm.isModified(instance);
   }


   /**
    * This method is called whenever an entity shall be stored to the
    * underlying storage. The persistence manager must call ejbStore on the
    * supplied instance and then store the state to the underlying storage.
    *
    * @param instance    the instance to synchronize
    * 
    * @throws RemoteException    thrown if some system exception occurs
    */
   public void storeEntity(EntityEnterpriseContext ctx)
      throws RemoteException
   {
      Object key = ctx.getInstance();
      try 
      {
         ccm.pushMetaAwareObject(key, unsharableResources);
         try 
         {
            pm.storeEntity(ctx);
         }
         finally
         {
            ccm.popMetaAwareObject(unsharableResources);
         } // end of try-catch
      }
      catch (ResourceException e)
      {
         throw new RemoteException("Could not store!: ", e);
      } // end of try-catch
      
   }


   /**
    * This method is called when an entity shall be passivate. The persistence
    * manager must call the ejbPassivate method on the instance.
    *
    * @param instance    the instance to passivate
    * 
    * @throws RemoteException    thrown if some system exception occurs
    */
   public void passivateEntity(EntityEnterpriseContext instance)
      throws RemoteException
   {
       pm.passivateEntity(instance);
   }


   /**
    * This method is called when an entity shall be removed from the
    * underlying storage. The persistence manager must call ejbRemove on the
    * instance and then remove its state from the underlying storage.
    *
    * @param instance    the instance to remove
    * 
    * @throws RemoteException    thrown if some system exception occurs
    * @throws RemoveException    thrown if the instance could not be removed
    */
   public void removeEntity(EntityEnterpriseContext instance)
      throws RemoteException, RemoveException
   {
       pm.removeEntity(instance);
   }



}// CachedConnectionInterceptor
