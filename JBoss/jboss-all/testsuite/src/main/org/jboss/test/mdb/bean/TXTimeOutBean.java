/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.mdb.bean;

import javax.ejb.MessageDrivenBean;
import javax.ejb.MessageDrivenContext;
import javax.ejb.EJBException;

import javax.jms.MessageListener;
import javax.jms.Message;
/**
 * The TXTimeOutBean simulates when the onMessage() takes
 * a long time to process the message.  When this happens,
 * the TM might time-out the transaction.  This bean 
 * can be used to see if the TX times outs occur.
 *
 * @author Hiram Chirino
 */

public class TXTimeOutBean implements MessageDrivenBean, MessageListener{

   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
	long PROCESSING_DELAY = 10; // simulate 10 seconds of processing
	
    public void setMessageDrivenContext(MessageDrivenContext ctx) {}
    public void ejbCreate() {}
    public void ejbRemove() {}

    public void onMessage(Message message) {
    	try {
			log.debug("Simulating "+PROCESSING_DELAY+" second(s) of message processing ");
			Thread.sleep(PROCESSING_DELAY*1000);
			log.debug("Message processing simulation done.");
    	} catch (Throwable ignore) {}
    }
} 


