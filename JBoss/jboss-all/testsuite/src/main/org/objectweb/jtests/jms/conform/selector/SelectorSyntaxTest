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

package org.objectweb.jtests.jms.conform.selector;

import org.objectweb.jtests.jms.framework.*;
import junit.framework.*;
import org.objectweb.jtests.jms.admin.*;

import javax.jms.*;
import javax.naming.*;

/**
 * Test the syntax of of message selector of JMS
 *
 * @author Jeff Mesnil (jmesnil@inrialpes.fr)
 * @version $Id: SelectorSyntaxTest.java,v 1.1 2002/04/21 21:15:19 chirino Exp $
 */
public class SelectorSyntaxTest extends PTPTestCase {

    /**
     * Test syntax of "<em>identifier</em> IS [NOT] NULL"
     */
    public void testNull() {
        try {
            receiver  = receiverSession.createReceiver(receiverQueue, "prop_name IS NULL");
            receiver  = receiverSession.createReceiver(receiverQueue, "prop_name IS NOT NULL");
        } catch (JMSException e) {
            fail(e);
        }
    }
  
    /**
     * Test syntax of "<em>identifier</em> [NOT] LIKE <em>pattern-value</em> [ESCAPE <em>escape-character</em>]"
     */
    public void testLike() {
        try {
            receiver  = receiverSession.createReceiver(receiverQueue, "phone LIKE '12%3'");
            receiver  = receiverSession.createReceiver(receiverQueue, "word LIKE 'l_se'");
            receiver  = receiverSession.createReceiver(receiverQueue, "underscored LIKE '\\_%' ESCAPE '\\'");
            receiver  = receiverSession.createReceiver(receiverQueue, "phone NOT LIKE '12%3'");
        } catch (JMSException e) {
            fail(e);
        }
    }

    /**
     * Test syntax of "<em>identifier</em> [NOT] IN (<em>string-literal1</em>, <em>string-literal2</em>,...)"
     */
    public void testIn() {
        try {
            receiver  = receiverSession.createReceiver(receiverQueue, "Country IN ('UK', 'US', 'France')");
            receiver  = receiverSession.createReceiver(receiverQueue, "Country NOT IN ('UK', 'US', 'France')");
        } catch (JMSException e) {
            fail(e);
        }
    }
  
    /**
     * Test syntax of "<em>arithmetic-expr1</em> [NOT] BETWEEN <em>arithmetic-expr2</em> and <em>arithmetic-expr3</em>"
     */
    public void testBetween() {
        try {
            receiver  = receiverSession.createReceiver(receiverQueue, "age BETWEEN 15 and 19");
            receiver  = receiverSession.createReceiver(receiverQueue, "age NOT BETWEEN 15 and 19");
        } catch (JMSException e) {
            fail(e);
        }
    }
  
    /**
     * Test diffent syntax for approximate numeric literal (+6.2, -95.7, 7.)
     */
    public void testApproximateNumericLiteral() {
        try {
            receiver  = receiverSession.createReceiver(receiverQueue, "average = +6.2");
            receiver  = receiverSession.createReceiver(receiverQueue, "average = -95.7");
            receiver  = receiverSession.createReceiver(receiverQueue, "average = 7.");
        } catch (JMSException e) {
            fail(e);
        }
    }

    /**
     * Test diffent syntax for exact numeric literal (+62, -957, 57)
     */
    public void testExactNumericLiteral() {
        try {
            receiver  = receiverSession.createReceiver(receiverQueue, "average = +62");
            receiver  = receiverSession.createReceiver(receiverQueue, "max = -957");
            receiver  = receiverSession.createReceiver(receiverQueue, "max = 57");
        } catch (JMSException e) {
            fail(e);
        }
    }

    /**
     * Test diffent syntax for zero as an exact or an approximate numeric literal (0, 0.0, 0.)
     */
    public void testZero() {
        try {
            receiver  = receiverSession.createReceiver(receiverQueue, "max = 0");
            receiver  = receiverSession.createReceiver(receiverQueue, "max = 0.0");
            receiver  = receiverSession.createReceiver(receiverQueue, "max = 0.");
        } catch (JMSException e) {
            fail(e);
        }
    }

    /**
     * Test diffent syntax for string literal ('literal' and 'literal''s')
     */
    public void testString() {
        try {
            receiver  = receiverSession.createReceiver(receiverQueue, "string = 'literal'");
            receiver  = receiverSession.createReceiver(receiverQueue, "string = 'literal''s'");
        } catch (JMSException e) {
            fail(e);
        }
    }
    
    /** 
     * Method to use this class in a Test suite
     */
    public static Test suite() {
        return new TestSuite(SelectorSyntaxTest.class);
    }
    
    public SelectorSyntaxTest(String name) {
        super(name);
    }
}
