/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.tm.plugins.tyrex;

import java.rmi.Remote;
import java.lang.reflect.Proxy;
import java.rmi.RemoteException;

import org.omg.CosTransactions.Resource;
import org.omg.CosTransactions.Inactive;

/**
 *   Subset of the org.omg.CosTransactions.Coordinator interface
 *   necessary to register a remote subordinate transaction
 *
 *   @see org.omg.CosTransactions.Coordinator,
 *        org.omg.CosTransactions.Resource,
 *        CoordinatorRemote
 *   @author <a href="mailto:akkerman@cs.nyu.edu">Anatoly Akkerman</a>
 *   @version $Revision: 1.1.1.1 $
 */

public interface CoordinatorRemoteInterface extends Remote {
  public void register_resource(Resource serializableResource) throws Inactive, RemoteException;
}