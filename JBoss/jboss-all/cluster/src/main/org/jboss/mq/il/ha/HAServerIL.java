/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.ha;

import org.jboss.mq.server.JMSServerInvoker;
import org.jboss.mq.il.rmi.RMIServerIL;
import org.jboss.mq.il.Invoker;
import org.jboss.mq.il.ServerIL;

/**
 * The HA implementation of the ServerIL object
 *
 * @author    Hiram Chirino (Cojonudo14@hotmail.com)
 * @version   $Revision: 1.4 $
 */
public class HAServerIL extends RMIServerIL {
   ServerIL haServerILProxy;

   public HAServerIL(Invoker s) throws java.rmi.RemoteException {
      super(s);
   }

   /**
    * Gets the haServerILProxy
    * @return Returns a ServerIL
    */
   public ServerIL getHaServerILProxy() {
      return haServerILProxy;
   }
   /**
    * Sets the haServerILProxy
    * @param haServerILProxy The haServerILProxy to set
    */
   public void setHaServerILProxy(ServerIL haServerILProxy) {
      this.haServerILProxy = haServerILProxy;
   }
   
   /**
    * No need to clone because there are no instance variables tha can get
    * clobbered. All Multiple connections can share the same JVMServerIL object
    * We don't return ourself but the HA proxy.
    *
    * @return   Description of the Returned Value
    */
   public ServerIL cloneServerIL()
   {
      return haServerILProxy;
   }
}
