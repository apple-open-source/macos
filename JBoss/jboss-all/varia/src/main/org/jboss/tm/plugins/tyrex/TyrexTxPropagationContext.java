/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.tm.plugins.tyrex;

import java.io.Externalizable;
import java.io.ObjectOutput;
import java.io.ObjectInput;
import java.io.IOException;
import java.rmi.RemoteException;


// OMG CORBA related stuff (used by Tyrex for transaction context propagation)
import org.omg.CosTransactions.PropagationContext;
import org.omg.CosTransactions.TransIdentity;
import org.omg.CosTransactions.Coordinator;
import org.omg.CosTransactions.Resource;
import org.omg.CosTransactions.otid_t;

import org.apache.log4j.Category;

// We need this to make a proxy for the OMG Coordinator for the remote site
import java.lang.reflect.Proxy;

/**
 *   This class wraps the OMG PropagationContext to be able to pass it
 *   via RMI. Currently we are only taking care of top-level transaction
 *   (no nested transactions) by sending via RMI only
 *   - timeout value
 *   - otid (@see org.omg.CosTransactions.otid_t - representation of Xid)
 *   - Coordinator's Proxy
 *
 *
 *   @see org.omg.CosTransactions.PropagationContext,
 *        org.omg.CosTransactions.otid_t,
 *        org.omg.CosTransactions.Coordinator,
 *        java.lang.reflect.Proxy
 *   @author <a href="mailto:akkerman@cs.nyu.edu">Anatoly Akkerman</a>
 *   @version $Revision: 1.3 $
 */

public class TyrexTxPropagationContext implements Externalizable {

  private Category log = Category.getInstance(this.getClass());

  public TyrexTxPropagationContext() {
    // public, no args constructor for externalization to work

    this.isNull = true;
  }

  protected TyrexTxPropagationContext (PropagationContext tpc) {
    try {
      this.isNull = false; // this is not a null Propagation Context
      this.timeout = tpc.timeout;
      this.coord = new CoordinatorRemote(tpc.current.coord);
      this.otid = tpc.current.otid;
      // DEBUG    log.debug("TyrexTxPropagationContext: created new tpc");
    } catch (RemoteException e) {
      // should never happen, we are instantiating the RemoteCoordinator
      // locally
      e.printStackTrace();
      log.warn("Impossible, CoordinatorRemote threw a RemoteException!");
    }
  }

  // this is called on the remote side
  protected PropagationContext getPropagationContext() {

    if ( !isNull && (tpc == null)) {
      // create these guys once
      // perhaps the coordinator Proxy can be cached and not recreated each
      // time we come in with the same tpc
      coordProxy = (Coordinator) Proxy.newProxyInstance(getClass().getClassLoader(),
                                        new Class[] {Coordinator.class},
                                        new CoordinatorInvoker(this.coord));
      tpc = new PropagationContext( this.timeout,
                                    new TransIdentity(this.coordProxy,
                                                      null,
                                                      this.otid),
                                    new TransIdentity[0], // no parents, but not null
                                    null);
    }

    // DEBUG    log.debug("TyrexTxPropagationContext recreated PropagationContext");
    return tpc;
  }

  public void writeExternal(ObjectOutput out) throws IOException {
    try {
      out.writeBoolean(this.isNull);
      if (! isNull) {
        out.writeInt(this.timeout);
        // DEBUG       log.debug("TPC: wrote timeout");
        out.writeObject(this.coord);
        // DEBUG       log.debug("TPC: wrote CoordinatorRemote stub");
        out.writeObject(this.otid); // otid implements IDLEntity which extends Serializable
        // DEBUG       log.debug("TPC: wrote otid");
      }
    } catch (Exception e) {
      log.warn("Unable to externalize tpc!");
      e.printStackTrace();
      throw new IOException(e.toString());
    }
  }

  public void readExternal(ObjectInput in) throws IOException, ClassNotFoundException {
    try {
      this.isNull = in.readBoolean();
      if (!isNull) {
        this.timeout = in.readInt();
        // DEBUG      log.debug("TPC: read timeout");
        this.coord = (CoordinatorRemoteInterface) in.readObject();
        // DEBUG      log.debug("TPC: read coordinator stub");
        this.otid = (otid_t) in.readObject();
        // DEBUG      log.debug("TPC: read otid");
      }
    } catch (Exception e) {
      e.printStackTrace();
      throw new IOException (e.toString());
    }
  }

  /*
   * The fields of PropagationContext that we want to send to the remote side
   */

  protected int timeout;
  protected CoordinatorRemoteInterface coord;
  protected otid_t otid;

  // this is a special field that gets propagated to the remote side to
  // indicate that this is a null propagation context (i.e. it represents a
  // null transaction). Simply using a null TyrexTxPropagationContext in RMI
  // calls crashes because of NullPointerException in serialization of a
  // method invocation
  protected boolean isNull;

  // cached copy of tpc, so that we need to create it only once
  // on the remote side
  protected PropagationContext tpc = null;

  // proxy that wraps the coordinator remote stub on the remote side
  // This proxy is created when the tpc arrives to the remote side and
  // it is given to the Tyrex TM as part of the recreated tpc
  // to register the subordinate transaction with the originator as a Resource.
  // When Tyrex invokes coordinator.register_resource, this Proxy wraps the Resource
  // with a dynamic Proxy and calls the coordinator remote object passing
  // it the Resource proxy instead of the actual Resource, so that the
  // coordination can proceed through RMI rather than CORBA
  protected Coordinator coordProxy = null;
}
