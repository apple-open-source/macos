/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.tm.plugins.tyrex;

import java.rmi.Remote;
import java.rmi.RemoteException;

import org.omg.CosTransactions.HeuristicCommit;
import org.omg.CosTransactions.HeuristicMixed;
import org.omg.CosTransactions.HeuristicHazard;
import org.omg.CosTransactions.HeuristicRollback;
import org.omg.CosTransactions.NotPrepared;

import org.omg.CosTransactions.Vote;

/**
 *   Subset of org.omg.CosTransactions.Resource interface
 *   that is necessary for the originator's Coordinator to control
 *   the subordinate remote transaction's completion/rollback
 *
 *   @see org.omg.CosTransactions.Resource, ResourceRemote
 *   @author <a href="mailto:akkerman@cs.nyu.edu">Anatoly Akkerman</a>
 *   @version $Revision: 1.1.1.1 $
 */

interface ResourceRemoteInterface extends Remote {
  public Vote prepare() throws HeuristicMixed, HeuristicHazard, RemoteException;
  public void rollback() throws HeuristicCommit, HeuristicMixed, HeuristicHazard, RemoteException;
  public void commit() throws NotPrepared, HeuristicRollback,
                       HeuristicMixed, HeuristicHazard, RemoteException;
  public void commit_one_phase() throws HeuristicHazard, RemoteException;
  public void forget() throws RemoteException;
}