/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.jvm;

import java.util.Properties;
import org.jboss.mq.il.ServerIL;

import org.jboss.mq.il.ServerILFactory;
import org.jboss.mq.server.JMSServerInterceptor;

/**
 *  Implements the ServerILFactory interface to create a new JVMServerIL
 *
 * @author     David Maplesden
 * @created    August 16, 2001
 * @version    $Revision: 1.4 $
 */
public class JVMServerILFactory implements ServerILFactory {

   /**
    *  Used to construct the GenericConnectionFactory (bindJNDIReferences()
    *  builds it)
    *
    * @return                The ServerIL value
    * @exception  Exception  Description of Exception
    * @returns               ServerIL the instance of this IL
    */
   public ServerIL getServerIL()
      throws Exception {
      // We leave this for now, since a ServerIL seems to be bound in JNDI
      // in JVMServerILService.bindJNDIReferences()
      // FIXME, removed return of server, since it bypasses the new Invoker logic. Where is this used?
      //return new JVMServerIL( JMSServer.lookupJMSServer() );
      throw new Exception("WOW, JVM does not find its server to invok.!!!");
   }


   public void init( Properties init ) {
   }

}
