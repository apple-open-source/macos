/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cts.test;

import java.rmi.RemoteException;
import java.rmi.server.UnicastRemoteObject;

import org.apache.log4j.Category;
import org.jboss.test.cts.interfaces.ClientCallback;

//public class ClientCallbackImpl extends UnicastRemoteObject implements ClientCallback
public class ClientCallbackImpl implements ClientCallback
{
   private static Category log = Category.getInstance(ClientCallbackImpl.class);
   private boolean wasCalled;

   public ClientCallbackImpl()
   {
   }

   public String callback(String data) throws RemoteException
   {
      log.debug("callback, data="+data);
      wasCalled = true;
      return data;
   }

   public boolean wasCalled()
   {
      return wasCalled;
   }
}

