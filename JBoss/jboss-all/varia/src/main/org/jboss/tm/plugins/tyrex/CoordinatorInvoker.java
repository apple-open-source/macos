/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.tm.plugins.tyrex;

// We need this to make a proxy for the OMG Coordinator for the remote site
import java.lang.reflect.Proxy;
import java.lang.reflect.Method;

// OMG CORBA related stuff (used by Tyrex for transaction context propagation)
import org.omg.CosTransactions.Coordinator;
import org.omg.CosTransactions.Resource;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationHandler;

import java.io.Externalizable;
import java.io.IOException;


/**
 *  This is the InvocationHandler for the Proxy to the originator's Coordinator.
 *  We allow only register_resource() method to be called to register the
 *  subordinate transaction as a Resource with the Coordinator.
 *
 *   @see org.omg.CosTransactions.Coordinator#register_resource,
 *        java.lang.reflect.Proxy,
 *        java.lang.reflect.InvocationHandler,
 *        CoordinatorRemote,
 *        ResourceRemote
 *   @author <a href="mailto:akkerman@cs.nyu.edu">Anatoly Akkerman</a>
 *   @version $Revision: 1.2 $
 */

class CoordinatorInvoker implements InvocationHandler {

    private static Method register_resource;

    static {
      try {
        register_resource = Coordinator.class.getMethod("register_resource",
                                                        new Class[] {Resource.class});
      }
      catch (Exception e) {
        e.printStackTrace();
      }
    }

    private CoordinatorRemoteInterface remoteCoordinator;

    protected CoordinatorInvoker (CoordinatorRemoteInterface coord) {
      remoteCoordinator = coord;
    }

    public Object invoke (Object proxy, Method method, Object[] args)
        throws Throwable {

        if (method.equals(register_resource)) {

          // Wrap the Resource in a serializable Proxy and ship it with the
          // call to the CoordinatorRemote
          Resource serializableResource =
              (Resource) Proxy.newProxyInstance (getClass().getClassLoader(),
                                                 new Class[] {Resource.class},
                                                 new ResourceInvoker((Resource) args[0])); // args should be not null
          // DEBUG          Logger.debug("TyrexTxPropagationContex: Created Proxy for Resource, calling Proxy for Coordinator");
          // call our Coordinator
          remoteCoordinator.register_resource(serializableResource);
          // register_resource in RemoteCoordinator is a 'void' method call
          // while the actual Coordinator returns a RecoveryCoordinator
          // for simplicity we ignore the RecoveryCoordinator for now
          return null;
        }
        else {
          // all this trickery was only for the purpose of the remote
          // Tyrex instance being able to register it's transaction as a
          // resource, so we do not support any other methods besides "register_resource"
          throw new Exception("CoordinatorInvoker Proxy was called through an unknown method");
        }
    }
}