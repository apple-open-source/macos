/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.tm.plugins.tyrex;

import java.rmi.RemoteException;

import org.omg.CosTransactions.HeuristicCommit;
import org.omg.CosTransactions.HeuristicMixed;
import org.omg.CosTransactions.HeuristicHazard;
import org.omg.CosTransactions.HeuristicRollback;
import org.omg.CosTransactions.NotPrepared;

import org.omg.CosTransactions.Vote;

import org.omg.CosTransactions.Resource;
import org.omg.CosTransactions._ResourceImplBase;


/**
 *   RMI Remote Proxy that enables the Coordinator on the originating
 *   side to control the subordinate transaction
 *
 *   @see ResourceRemoteInterface,  ResourceInvoker, CoordinatorRemote
 *   @author <a href="mailto:akkerman@cs.nyu.edu">Anatoly Akkerman</a>
 *   @version $Revision: 1.2 $
 */

public class ResourceRemote extends java.rmi.server.UnicastRemoteObject implements ResourceRemoteInterface {

  private _ResourceImplBase localResource;

  protected ResourceRemote(Resource resource) throws RemoteException{
    localResource = (_ResourceImplBase) resource;
  }

  public Vote prepare() throws HeuristicMixed, HeuristicHazard, RemoteException {
    //DEBUG    Logger.debug("ResourceRemote: preparing ...");
    return localResource.prepare();
  }

  public void rollback() throws HeuristicCommit, HeuristicMixed, HeuristicHazard, RemoteException {
    //DEBUG    Logger.debug("ResourceRemote: rolling back ...");
    localResource.rollback();
    //DEBUG    Logger.debug("ResourceRemote: rolled back.");
  }

  public void commit() throws NotPrepared, HeuristicRollback,
                       HeuristicMixed, HeuristicHazard, RemoteException {
    //DEBUG    Logger.debug("ResourceRemote: committing ...");
    localResource.commit();
    //DEBUG    Logger.debug("ResourceRemote: committed.");
  }

  public void commit_one_phase() throws HeuristicHazard, RemoteException {
    //DEBUG    Logger.debug("ResourceRemote: One phase committing ...");
    localResource.commit_one_phase();
    //DEBUG    Logger.debug("ResourceRemote: committed.");
  }

  public void forget() throws RemoteException{
    //DEBUG    Logger.debug("ResourceRemote: forgetting ...");
    localResource.forget();
    //DEBUG    Logger.debug("ResourceRemote: forgot.");
  }
}