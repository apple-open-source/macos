/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.jms.asf;

import java.io.Serializable;
import javax.jms.Connection;
import javax.jms.JMSException;
import javax.jms.MessageListener;

import javax.jms.ServerSessionPool;
import org.jboss.tm.XidFactoryMBean;

/**
 * An implementation of ServerSessionPoolFactory. <p>
 *
 * Created: Fri Dec 22 09:47:41 2000
 *
 * @author    <a href="mailto:peter.antman@tim.se">Peter Antman</a> .
 * @author    <a href="mailto:hiram.chirino@jboss.org">Hiram Chirino</a> .
 * @version   $Revision: 1.8 $
 */
public class StdServerSessionPoolFactory
       implements ServerSessionPoolFactory, Serializable
{
   /**
    * The name of this factory.
    */
   private String name;

   private XidFactoryMBean xidFactory;

   /**
    * Construct a <tt>StdServerSessionPoolFactory</tt> .
    */
   public StdServerSessionPoolFactory()
   {
      super();
   }

   /**
    * Set the name of the factory.
    *
    * @param name  The name of the factory.
    */
   public void setName(final String name)
   {
      this.name = name;
   }

   /**
    * Get the name of the factory.
    *
    * @return   The name of the factory.
    */
   public String getName()
   {
      return name;
   }

   /**
    * The <code>setXidFactory</code> method supplies the XidFactory that 
    * server sessions will use to get Xids to control local transactions.
    *
    * @param xidFactory a <code>XidFactoryMBean</code> value
    */
   public void setXidFactory(final XidFactoryMBean xidFactory)
   {
      this.xidFactory = xidFactory;
   }

   /**
    * The <code>getXidFactory</code> method returns the XidFactory that 
    * server sessions will use to get xids..
    *
    * @return a <code>XidFactoryMBean</code> value
    */
   public XidFactoryMBean getXidFactory()
   {
      return xidFactory;
   }

   /**
    * Create a new <tt>ServerSessionPool</tt> .
    *
    * @param con
    * @param maxSession
    * @param isTransacted
    * @param ack
    * @param listener
    * @param isContainerManaged          Description of Parameter
    * @return                            A new pool.
    * @throws JMSException
    * @exception javax.jms.JMSException  Description of Exception
    */
   public javax.jms.ServerSessionPool getServerSessionPool(javax.jms.Connection con, int maxSession, boolean isTransacted, int ack, boolean useLocalTX, javax.jms.MessageListener listener) throws javax.jms.JMSException
   {
      ServerSessionPool pool = (ServerSessionPool)new StdServerSessionPool(con, isTransacted, ack, useLocalTX, listener, maxSession, xidFactory);
      return pool;
   }
}
