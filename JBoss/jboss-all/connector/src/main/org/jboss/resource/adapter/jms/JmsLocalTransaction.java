/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.resource.adapter.jms;

import javax.jms.JMSException;

import javax.resource.ResourceException;
import javax.resource.spi.LocalTransaction;
import javax.resource.spi.ConnectionEvent;
import javax.resource.spi.EISSystemException;

/**
 * ???
 *
 * Created: Tue Apr 17 23:44:05 2001
 *
 * @author <a href="mailto:peter.antman@tim.se">Peter Antman</a>.
 * @version $Revision: 1.1 $
 */
public class JmsLocalTransaction
   implements LocalTransaction
{
   protected JmsManagedConnection mc;
   
   public JmsLocalTransaction(final JmsManagedConnection mc) {
      this.mc = mc;
   }

   public void begin() throws ResourceException {
      // NOOP - begin is automatic in JMS
      // Should probably send event
      ConnectionEvent ev = new ConnectionEvent(mc, ConnectionEvent.LOCAL_TRANSACTION_STARTED);
      mc.sendEvent(ev);
   }
    
   public void commit() throws ResourceException {
      try {
         mc.getSession().commit();
         ConnectionEvent ev = new ConnectionEvent(mc, ConnectionEvent.LOCAL_TRANSACTION_COMMITTED);
         mc.sendEvent(ev);
      }
      catch (JMSException ex) {
         ResourceException re =
            new EISSystemException("Could not commit LocalTransaction : " + ex.getMessage());
         re.setLinkedException(ex);
         throw re;
      }
    }
   
   public void rollback() throws ResourceException {
      try {
         mc.getSession().rollback();
         ConnectionEvent ev = new ConnectionEvent(mc, ConnectionEvent.LOCAL_TRANSACTION_ROLLEDBACK);
         mc.sendEvent(ev);
      }
      catch (JMSException ex) {
         ResourceException re =
            new EISSystemException("Could not rollback LocalTransaction : " + ex.getMessage());
         re.setLinkedException(ex);
         throw re;
      }
   }
}



