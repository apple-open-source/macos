/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.hasessionstate.server;

import javax.management.ObjectName;

import org.jboss.mx.util.ObjectNameFactory;

/**
 * MBEAN interface for HASessionState service.
 *
 * @see org.jboss.ha.hasessionstate.interfaces.HASessionState
 *
 * @author sacha.labourey@cogito-info.ch
 * @version $Revision: 1.8.4.2 $
 *
 * <p><b>Revisions:</b><br>
 */
public interface HASessionStateServiceMBean
   extends org.jboss.system.ServiceMBean
{
   ObjectName OBJECT_NAME = ObjectNameFactory.create("jboss:service=HASessionState");

   String getJndiName();
   void setJndiName(String newName);

   String getPartitionName();
   void setPartitionName(String name);
   
   long getBeanCleaningDelay();
   void setBeanCleaningDelay(long newDelay);
}
