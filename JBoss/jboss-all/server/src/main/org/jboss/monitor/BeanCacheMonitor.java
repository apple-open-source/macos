/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.monitor;



import java.net.URL;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import javax.management.JMException;
import javax.management.MBeanRegistration;
import javax.management.MBeanServer;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import org.jboss.ejb.Container;
import org.jboss.ejb.EJBDeployer;
import org.jboss.ejb.EJBDeployerMBean;
import org.jboss.ejb.EjbModule;
import org.jboss.ejb.EntityContainer;
import org.jboss.ejb.InstanceCache;
import org.jboss.ejb.StatefulSessionContainer;
import org.jboss.logging.Logger;
import org.jboss.monitor.client.BeanCacheSnapshot;

/**
 *
 * @see Monitorable
 * @author <a href="mailto:simone.bordet@compaq.com">Simone Bordet</a>
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version $Revision: 1.9 $
 */
public class BeanCacheMonitor
   implements BeanCacheMonitorMBean, MBeanRegistration
{
   // Constants ----------------------------------------------------
   
   // Attributes ---------------------------------------------------
   static Logger log = Logger.getLogger(BeanCacheMonitor.class);
   MBeanServer m_mbeanServer;
   // Static -------------------------------------------------------
   
   // Constructors -------------------------------------------------
   public BeanCacheMonitor()
   {}
   
   // Public -------------------------------------------------------
   
   // MBeanRegistration implementation -----------------------------------
   public ObjectName preRegister(MBeanServer server, ObjectName name)
   throws Exception
   {
      m_mbeanServer = server;
      return name;
   }
   
   public void postRegister(Boolean registrationDone)
   {}
   public void preDeregister() throws Exception
   {}
   public void postDeregister()
   {}
   
   // CacheMonitorMBean implementation -----------------------------------
   /**
    * Describe <code>getSnapshots</code> method here.
    *
    * @return a <code>BeanCacheSnapshot[]</code> value
    * @todo: convert to queries on object names of components.
    */
   public BeanCacheSnapshot[] getSnapshots()
   {
      try 
      {
         Collection snapshots = listSnapshots();
         BeanCacheSnapshot[] snapshotArray = new BeanCacheSnapshot[snapshots.size()];
         return (BeanCacheSnapshot[])snapshots.toArray(snapshotArray);
      }
      catch (JMException e)
      {
         log.error("Problem getting bean cache snapshots", e);
         return null;  
      } // end of try-catch      
   }

   /**
    * The <code>listSnapshots</code> method returns a collection
    * of BeanSnapshots showing the 
    *
    * @return a <code>Collection</code> value
    * @exception JMException if an error occurs
    */
   public Collection listSnapshots() throws JMException
   {
      ArrayList cacheSnapshots = new ArrayList();

      Collection ejbModules = m_mbeanServer.queryNames(EjbModule.EJB_MODULE_QUERY_NAME, null);
      
      // For each application, getContainers()
      for (Iterator i = ejbModules.iterator(); i.hasNext(); )
      {
         ObjectName ejbModule = (ObjectName) i.next();
         String name = ejbModule.getKeyProperty("jndiName");
         
         // Loop on each container of the application
         //Since we are just totaling everything, do a query on container object names

         Collection containers = (Collection)m_mbeanServer.getAttribute(ejbModule, "Containers");
         for (Iterator cs = containers.iterator(); cs.hasNext();)
         {
            // Get the cache for each container
            InstanceCache cache = null;
            Object container = cs.next();
            if (container instanceof EntityContainer)
            {
               cache = ((EntityContainer)container).getInstanceCache();
            }
            else if (container instanceof StatefulSessionContainer)
            {
               cache = ((StatefulSessionContainer)container).getInstanceCache();
            }
            
            // Take a cache snapshot
            if (cache instanceof Monitorable)
            {
               BeanCacheSnapshot snapshot = new BeanCacheSnapshot();
               snapshot.m_application = name;
               snapshot.m_container = ((Container)container).getBeanMetaData().getEjbName();
               ((Monitorable)cache).sample(snapshot);
               cacheSnapshots.add(snapshot);
            }
         }
      }      
      return cacheSnapshots;
   }
   
   // Inner classes -------------------------------------------------
}

