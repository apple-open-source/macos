/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.tm.plugins.tyrex;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationHandler;

import org.omg.CosTransactions.Coordinator;
import org.omg.CosTransactions.Resource;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationHandler;

import java.io.Externalizable;
import java.io.IOException;

import org.apache.log4j.Category;


/**
 *   This is the InvocationHandler for the Proxy we hand over to the
 *   originator's Coordinator to control our subordinate transaction
 *
 *   @see org.omg.CosTransactions.Resource,
 *        java.lang.reflect.Proxy,
 *        java.lang.reflect.InvocationHandler,
 *        ResourceRemote,
 *        CoordinatorRemote
 *   @author <a href="mailto:akkerman@cs.nyu.edu">Anatoly Akkerman</a>
 *   @version $Revision: 1.3 $
 */

public class ResourceInvoker implements InvocationHandler, Externalizable {

  private static Method prepare;
  private static Method rollback;
  private static Method commit;
  private static Method commit_one_phase;
  private static Method forget;

  static {
    try {
      // get all the methods through which we may get called
      prepare = Resource.class.getMethod("prepare", null);
      rollback = Resource.class.getMethod("rollback", null);
      commit = Resource.class.getMethod("commit", null);
      commit_one_phase = Resource.class.getMethod("commit_one_phase", null);
      forget = Resource.class.getMethod("forget", null);
    }
    catch (Exception e) {
      e.printStackTrace();
    }
  }

  private Category log = Category.getInstance(this.getClass());
  private ResourceRemoteInterface remoteResource;

  public ResourceInvoker() {
    // for externalization to work
  }

  public ResourceInvoker(Resource res) {
    try {
      remoteResource = new ResourceRemote(res);
    } catch (Exception e) {
      e.printStackTrace();
      remoteResource = null;
      log.warn("ResourceInvoker did not initialize properly! Ther will be problems with this transaction!");
    }
  }

  public void writeExternal(java.io.ObjectOutput out) throws IOException {
    out.writeObject(remoteResource);
  }

  public void readExternal(java.io.ObjectInput in) throws IOException, ClassNotFoundException {
    this.remoteResource = (ResourceRemoteInterface) in.readObject();
  }

  public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {

    // Just call the appropriate method on our RemoteCoordinator

    if (method.equals(prepare)) {
      // DEBUG      log.debug("ResourceInvoker: calling prepare()");
      return remoteResource.prepare();
    }
    else if (method.equals(rollback)) {
      // DEBUG      log.debug("ResourceInvoker: calling rollback()");
      remoteResource.rollback();
      return null;
    }
    else if (method.equals(commit)) {
      // DEBUG      log.debug("ResourceInvoker: calling commit()");
      remoteResource.commit();
      return null;
    }
    else if (method.equals(commit_one_phase)) {
      // DEBUG      log.debug("ResourceInvoker: calling commit_one_phase()");
      remoteResource.commit_one_phase();
      return null;
    }
    else if (method.equals(forget)) {
      // DEBUG      log.debug("ResourceInvoker: calling forget()");
      remoteResource.forget();
      return null;
    }
    else {
      throw new Exception("ResourceInvoker: called through an unknown method!");
    }
  }
}
