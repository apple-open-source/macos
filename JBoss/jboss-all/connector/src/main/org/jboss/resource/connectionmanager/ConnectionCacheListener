
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.resource.connectionmanager;

import java.util.Collection;
import java.util.Set;
import javax.resource.ResourceException;
import javax.transaction.SystemException;
import javax.transaction.Transaction;



/**
 * ConnectionCacheListener.java
 *
 *
 * Created: Sat Jan  5 19:47:35 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author <a href="mailto:E.Guib@ceyoniq.com">Erwin Guib</a>
 * @version
 */

public interface ConnectionCacheListener 
{

   void transactionStarted(Collection conns) throws SystemException;

   void reconnect(Collection conns, Set unsharableResources) throws ResourceException;
   
   void disconnect(Collection conns, Set unsharableResources) throws ResourceException;
   
}// ConnectionCacheListener
