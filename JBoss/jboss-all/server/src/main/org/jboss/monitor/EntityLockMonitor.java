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
import java.util.HashMap;
import java.util.Set;
import java.util.Arrays;
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
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.naming.NonSerializableFactory;
import javax.naming.NamingException;
import javax.naming.InitialContext;
import javax.naming.Context;
import javax.naming.Name;
import javax.naming.NameNotFoundException;
import javax.naming.StringRefAddr;
import javax.naming.Reference;

/**
 *
 * @see Monitorable
 * @author <a href="mailto:bill@jboss.org">Bill Burke</a>
 * @version $Revision: 1.1.4.3 $
 */
public class EntityLockMonitor
   extends ServiceMBeanSupport
   implements EntityLockMonitorMBean, MBeanRegistration
{
   // Constants ----------------------------------------------------
   public static final String JNDI_NAME = "EntityLockMonitor";
   // Attributes ---------------------------------------------------
   static Logger log = Logger.getLogger(EntityLockMonitor.class);
   MBeanServer m_mbeanServer;
   // Static -------------------------------------------------------
   
   // Constructors -------------------------------------------------
   public EntityLockMonitor()
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

   protected HashMap monitor = new HashMap();
   protected long contenders = 0;
   protected long maxContenders = 0;
   protected ArrayList times = new ArrayList();
   protected long contentions = 0;
   protected long total_time = 0;
   protected long sumContenders = 0;
   
   
   public synchronized void incrementContenders()
   {
      contentions++;
      contenders++;
      if (contenders > maxContenders) maxContenders = contenders;
      sumContenders += contenders;
   }
   public synchronized void decrementContenders(long time)
   {
      times.add(new Long(time));
      contenders--;
   }
   public synchronized long getAverageContenders()
   {
      if (contentions == 0) return 0;
      return sumContenders / contentions;
   }
   public synchronized long getMaxContenders()
   {
      return maxContenders;
   }

   public synchronized long getMedianWaitTime()
   {
      if (times.size() < 1) return 0;

      Long[] alltimes = (Long[])times.toArray(new Long[times.size()]);
      long[] thetimes = new long[alltimes.length];
      for (int i = 0; i < thetimes.length; i++)
      {
         thetimes[i] = alltimes[i].longValue();
      }
      Arrays.sort(thetimes);
      return thetimes[thetimes.length / 2];
   }

   public synchronized long getTotalContentions()
   {
      return contentions;
   }

   public LockMonitor getEntityLockMonitor(String ejbName)
   {
      LockMonitor lm = null;
      synchronized(monitor)
      {
         lm = (LockMonitor)monitor.get(ejbName);
         if (lm == null)
         {
            lm = new LockMonitor(this);
            monitor.put(ejbName, lm);
         }
      }
      return lm;
   }

   public String printLockMonitor()
   {
      StringBuffer rtn = new StringBuffer();
      rtn.append("<table width=\"1\" border=\"1\">");
      rtn.append("<tr><td><b>EJB NAME</b></td><td><b>Total Lock Time</b></td><td><b>Num Contentions</b></td><td><b>Time Outs</b></td></tr>");
      synchronized(monitor)
      {
         Iterator it = monitor.keySet().iterator();
         while (it.hasNext())
         {
            rtn.append("<tr>");
            String ejbName = (String)it.next();
            rtn.append("<td>");
            rtn.append(ejbName);
            rtn.append("</td>");
            LockMonitor lm = (LockMonitor)monitor.get(ejbName);
            rtn.append("<td>");
            rtn.append(("" + lm.total_time));
            rtn.append("</td>");
            rtn.append("<td>");
            rtn.append(("" + lm.num_contentions));
            rtn.append("</td>");
            rtn.append("<td>");
            rtn.append(("" + lm.timeouts));
            rtn.append("</td>");
            rtn.append("</tr>");
         }
      }
      rtn.append("</table>");
      return rtn.toString();
   }
   
   public synchronized void clearMonitor()
   {
      contenders = 0;
      maxContenders = 0;
      times.clear();
      contentions = 0;
      total_time = 0;
      sumContenders = 0;

      synchronized(monitor)
      {
         Iterator it = monitor.keySet().iterator();
         while (it.hasNext())
         {
            String ejbName = (String)it.next();
            LockMonitor lm = (LockMonitor)monitor.get(ejbName);
            synchronized (lm)
            {
               lm.timeouts = 0;
               lm.total_time = 0;
               lm.num_contentions = 0;
            }
         }
      }
   }
   

   protected void startService()
      throws Exception
   {
      bind();

	  log.info("EntityLockMonitor started");
   }

   protected void stopService() {
      try
      {
         unbind();
      }
      catch (Exception ignored) {}

	  log.info("EntityLockMonitor stopped");
   }

   private void bind() throws NamingException
   {
      Context ctx = new InitialContext();

      // Ah ! We aren't serializable, so we use a helper class
      NonSerializableFactory.bind(JNDI_NAME, this);
      
      // The helper class NonSerializableFactory uses address type nns, we go on to
      // use the helper class to bind ourselves in JNDI
      StringRefAddr addr = new StringRefAddr("nns", JNDI_NAME);
      Reference ref = new Reference(EntityLockMonitor.class.getName(), addr, NonSerializableFactory.class.getName(), null);
      ctx.bind(JNDI_NAME, ref);
   }
   
   private void unbind() throws NamingException
   {
      new InitialContext().unbind(JNDI_NAME);
      NonSerializableFactory.unbind(JNDI_NAME);
   }

   // Inner classes -------------------------------------------------
}

