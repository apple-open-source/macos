/*
 * Copyright (c) 2000 Peter Antman DN <peter.antman@dn.se>
 * Copyright (c) 2000 Peter Hiram Chirino (Cojonudo14@hotmail.com)
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
import javax.jms.ObjectMessage;
/**
 * ObjectMessageBean.java
 * This test the ability to send an ObjectMessage to a MDB
 *
 * Adapted from the QueueBean class
 *
 */

public class ObjectMessageBean implements MessageDrivenBean, MessageListener{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
    private MessageDrivenContext ctx = null;
    public ObjectMessageBean() {
	
    }
    public void setMessageDrivenContext(MessageDrivenContext ctx)
	throws EJBException {
	this.ctx = ctx;
    }
    
    public void ejbCreate() {}

    public void ejbRemove() {ctx=null;}

    public void onMessage(Message message) {
	try {	
	     ObjectMessage om = (ObjectMessage)message;
	     log.debug("DEBUG: ObjectMessageBean got object: " + 
		   om.getObject().toString() );
	} catch ( Throwable e ) {
		log.error("failed", e);
	}
    }
} 


