/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il;

import org.jboss.mq.Connection;

/**
 *  This class manages the lifecycle of an instance of the ClientIL
 *  Implementations of this class should have a default constructor.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.2 $
 */
public interface ClientILService {

   /**
    *  After construction, the ClientILService is initialized with a reference
    *  to the Connection object and the connection properties.
    *
    * @param  connection     Description of Parameter
    * @param  props          Description of Parameter
    * @exception  Exception  Description of Exception
    */
   public void init( Connection connection, java.util.Properties props )
      throws Exception;

   /**
    *  After initialization, this method may be called to obtain a reference to
    *  a ClientIL object. This object will eventually be serialized and sent to
    *  the server so that he can invoke methods on connection it was initialized
    *  with.
    *
    * @return                The ClientIL value
    * @exception  Exception  Description of Exception
    */
   public ClientIL getClientIL()
      throws Exception;


   /**
    *  Once started, the ClientIL instance should process all server requests.
    *
    * @exception  Exception  Description of Exception
    */
   public void start()
      throws Exception;

   /**
    *  Once stopped, the ClientIL instance stop processing all server requests.
    *  if( cr_server != null ) cr_server.close(); if (cr!=null && cr instanceof
    *  java.rmi.Remote) { java.rmi.server.UnicastRemoteObject.unexportObject((java.rmi.Remote)cr,
    *  true); }
    *
    * @exception  Exception  Description of Exception
    */

   public void stop()
      throws Exception;
}
