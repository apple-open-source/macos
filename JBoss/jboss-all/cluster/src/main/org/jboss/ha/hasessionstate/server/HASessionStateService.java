/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.hasessionstate.server;

import org.jboss.system.ServiceMBeanSupport;

import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;

import org.jboss.ha.hasessionstate.server.HASessionStateImpl;

/**
 *   Service class for HASessionState
 *
 *   @see org.jboss.ha.hasessionstate.interfaces.HASessionState
 *   @author sacha.labourey@cogito-info.ch
 *   @version $Revision: 1.7.4.1 $
 *
 * <p><b>Revisions:</b><br>
 */

public class HASessionStateService 
   extends ServiceMBeanSupport 
   implements HASessionStateServiceMBean
{
   protected String jndiName;
   protected String haPartitionName;
   protected long beanCleaningDelay = 0;
   
   protected HASessionStateImpl sessionState;
   
   public String getName ()
   {
      return this.getJndiName ();
   }

   public String getJndiName ()
   {
      return this.jndiName;
   }
   
   public void setJndiName (String newName)
   {
      this.jndiName = newName;
   }
   
   public String getPartitionName ()
   {
      return this.haPartitionName;
   }
   
   public void setPartitionName (String name)
   {
      this.haPartitionName = name;
   }
   
   public long getBeanCleaningDelay ()
   {
      if (this.sessionState == null)
         return this.beanCleaningDelay;
      else
         return this.sessionState.beanCleaningDelay;
   }   
   
   public void setBeanCleaningDelay (long newDelay)
   { this.beanCleaningDelay = newDelay; }
   
   // ******************************************************************
   
   protected ObjectName getObjectName (MBeanServer server, ObjectName name)
      throws MalformedObjectNameException
   {
      return name == null ? OBJECT_NAME : name;
   }
   
   // ******************************************************************
   
   
   protected void createService()
      throws Exception
   {
      this.sessionState = new HASessionStateImpl (this.jndiName, this.haPartitionName, 
						  this.beanCleaningDelay);
      this.sessionState.init ();
   }

   protected void startService () throws Exception
   {
      this.sessionState.start ();
   }
   
   protected void stopService() throws Exception
   {
      this.sessionState.stop ();
      this.sessionState = null;
   }
   
}

