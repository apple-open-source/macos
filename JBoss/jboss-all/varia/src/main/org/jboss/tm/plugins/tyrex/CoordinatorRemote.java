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

import org.omg.CosTransactions.Coordinator;
import org.omg.CosTransactions._CoordinatorImplBase;
import org.omg.CosTransactions.RecoveryCoordinator;
import org.omg.CosTransactions.Resource;

import org.omg.CosTransactions.Inactive;

/**
 *   RMI Remote Proxy that enables the remote Transaction Manager
 *   to register the subordinate transaction as a resource
 *   with the originator's Coordinator
 *
 *   @see CoordinatorRemoteInterface, CoordinatorInvoker, ResourceRemote
 *   @author <a href="mailto:akkerman@cs.nyu.edu">Anatoly Akkerman</a>
 *   @version $Revision: 1.2 $
 */

public class CoordinatorRemote extends java.rmi.server.UnicastRemoteObject implements CoordinatorRemoteInterface {

  private org.omg.CosTransactions._CoordinatorImplBase localCoordinator;

  protected CoordinatorRemote(Coordinator coord) throws RemoteException {
    localCoordinator = (org.omg.CosTransactions._CoordinatorImplBase) coord;
  }

  public void register_resource(Resource serializableResource) throws Inactive, RemoteException {
    // DEBUG    Logger.debug("CoordinatorRemote: Registering resource");
    RecoveryCoordinator recoveryCoord = localCoordinator.register_resource(serializableResource);
    // ignore the recovery coordinator for now
    // if we choose, we can wrap the RecoveryCoordinator
    // and return its serializable dynamic proxy to the other side

    // DEBUG    Logger.debug("CoordinatorRemote: Resource registered");
  }
}