/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.jvm;

import javax.jms.Destination;
import javax.jms.JMSException;
import org.jboss.mq.Connection;
import org.jboss.mq.ReceiveRequest;

import org.jboss.mq.SpyDestination;
import org.jboss.mq.il.ClientIL;
import org.jboss.mq.il.ClientILService;

/**
 *  The RMI implementation of the ConnectionReceiver object
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.3 $
 */
public class JVMClientILService implements org.jboss.mq.il.ClientILService {

   //the client IL
   JVMClientIL      clientIL;

   /**
    *  getClientIL method comment.
    *
    * @return                          The ClientIL value
    * @exception  java.lang.Exception  Description of Exception
    */
   public org.jboss.mq.il.ClientIL getClientIL()
      throws java.lang.Exception {
      return clientIL;
   }


   /**
    *  start method comment.
    *
    * @exception  java.lang.Exception  Description of Exception
    */
   public void start()
      throws java.lang.Exception {
      clientIL.stopped = false;
   }

   /**
    * @exception  java.lang.Exception  Description of Exception
    */
   public void stop()
      throws java.lang.Exception {
      clientIL.stopped = true;
   }

   /**
    *  init method comment.
    *
    * @param  connection               Description of Parameter
    * @param  props                    Description of Parameter
    * @exception  java.lang.Exception  Description of Exception
    */
   public void init( org.jboss.mq.Connection connection, java.util.Properties props )
      throws java.lang.Exception {
      clientIL = new JVMClientIL( connection );
   }
}
