/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mq.pm;

import javax.management.ObjectName;

import org.jboss.mx.util.ObjectNameFactory;

/**
 * The JMX managment interface for {@link PersistenceManager} MBean.
 *
 * Created: Wed Nov  7 19:30:11 2001
 * @author <a href="mailto:d_jencks@users.sourceforge.net">david jencks</a>
 * @version $Revision: 1.4.4.1 $
 */
public interface PersistenceManagerMBean 
{
   ObjectName OBJECT_NAME = ObjectNameFactory.create("jboss.mq:service=PersistenceManager");
   
   // methods needed for JBossMQService to set up connections
   // and to require the MessageCache.

   Object getInstance();

   ObjectName getMessageCache();

   void setMessageCache(ObjectName messageCache);
}
