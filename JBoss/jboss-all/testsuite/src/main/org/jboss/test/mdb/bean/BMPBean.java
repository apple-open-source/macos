/*
 * Copyright (c) 2000 Peter Antman DN <peter.antman@dn.se>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
package org.jboss.test.mdb.bean;

import javax.ejb.MessageDrivenBean;
import javax.ejb.MessageDrivenContext;
import javax.ejb.EJBException;

import javax.jms.MessageListener;
import javax.jms.Message;
import javax.naming.InitialContext;
import javax.transaction.Status;
import javax.transaction.Transaction;
import javax.transaction.TransactionManager;
import javax.transaction.xa.XAResource;

/**
 * MessageBeanImpl.java
 *
 *
 * Created: Sat Nov 25 18:07:50 2000
 *
 * @author 
 * @version
 */

public class BMPBean implements MessageDrivenBean, MessageListener{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
    private MessageDrivenContext ctx = null;
    public BMPBean() {
	
    }
    public void setMessageDrivenContext(MessageDrivenContext ctx)
	throws EJBException {
	this.ctx = ctx;
    }
    
    public void ejbCreate() {}

    public void ejbRemove() {ctx=null;}

    public void onMessage(Message message) {

		log.debug("DEBUG: BMPBean got message" + message.toString() );
      	try {
       	  TransactionManager tm = (TransactionManager)new InitialContext().lookup("java:/TransactionManager");
	      Transaction threadTx = tm.suspend();
			log.debug("DEBUG Tx="+threadTx);

	log.debug("Sleeping for 10 seconds");
      try { Thread.currentThread().sleep(1000*10);
      } catch ( InterruptedException e ) {}
	log.debug("Sleep done");

			if( threadTx != null )
	      		tm.resume(threadTx);
      	} catch (Exception e) {
       	  log.debug("BMPBean Error:"+e);
      	}
    }
} 


