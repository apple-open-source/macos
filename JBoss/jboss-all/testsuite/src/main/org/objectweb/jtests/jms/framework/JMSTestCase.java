/*
 * JORAM: Java(TM) Open Reliable Asynchronous Messaging
 * Copyright (C) 2002 INRIA
 * Contact: joram-team@objectweb.org
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 * 
 * Initial developer(s): Jeff Mesnil (jmesnil@inrialpes.fr)
 * Contributor(s): ______________________________________.
 */

package org.objectweb.jtests.jms.framework;

import junit.framework.*;
import javax.jms.JMSException;

/**
 * Class extending <code>junit.framework.TestCase</code> to
 * provide a new <code>fail()</code> method with an <code>Exception</code>
 * as parameter.
 *<br />
 * Every Test Case for JMS should extend this class instead of <code>junit.framework.TestCase</code>
 *
 * @author Jeff Mesnil (jmesnil@inrialpes.fr)
 * @version $Id: JMSTestCase.java,v 1.1 2002/04/21 21:15:19 chirino Exp $
 */
public class JMSTestCase extends TestCase {
    
    /**
     * Fails a test with an exception which will be used for a message.
     * 
     * If the exception is an instance of <code>javax.jms.JMSException</code>, the
     * message of the failure will contained both the JMSException and its linked exception
     * (provided there's one).
     */
    public void fail (Exception e) {
	if (e instanceof javax.jms.JMSException) {
	    JMSException exception = (JMSException) e;
	    String message = e.toString();
	    Exception linkedException = exception.getLinkedException();  
	    if (linkedException != null) {
		message += " [linked exception: "+ linkedException +"]";
	    }
	    super.fail(message);
	} else {
	    super.fail(e.getMessage());
	}
    }    
    
    public JMSTestCase(String name) {
	super(name);
    }
}

    
