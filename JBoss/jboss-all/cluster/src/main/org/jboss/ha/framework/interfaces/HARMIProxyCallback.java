/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.interfaces;

/**
 * DistributedState is a service on top of HAPartition that provides a
 * cluster-wide distributed state. The DistributedState (DS) service
 * provides a <String categorgy, Serializable key, Serializable value> tuple
 * map. Thus, any service, application, container, ... can request its own DS
 * "private space" by working* in its own category (a string name).
 * You work in a category like a Dictionary: you set values by key within a
 * category. Each time a value is added/modified/removed, the modification
 * is made cluster-wide, on all other nodes.
 * Reading values is always made locally (no network access!)
 * Objects can also subscribes to DS events to be notified when some values gets
 * modified/removed/added in a particular category.
 *
 * @author <a href="mailto:sacha.labourey@jboss.org">Sacha Labourey</a>.
 * @author Sacha Labourey
 * @version $Revision: 1.1.2.1 $
 * @see org.jboss.ha.framework.server.HARMIServerImpl
 */
public interface HARMIProxyCallback
{
    public void proxyUpdated ();
}
