/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mq.il.ha.examples;

import javax.jms.JMSException;
import javax.naming.NamingException;

/**
 * 
 * Helps with manual testing of the HA JMS
 * 
 * @author  Ivelin Ivanov <ivelin@apache.org>
 *
 */
public interface HAJMSClientMBean extends org.jboss.system.ServiceMBean
{
  public abstract String getConnectionException();
  public abstract String getLastMessage() throws JMSException;
  public abstract void publishMessageToTopic(String text) throws JMSException;
  public abstract void sendMessageToQueue(String text) throws JMSException;
  public abstract void connect() throws NamingException, JMSException;
  public abstract void disconnect() throws NamingException, JMSException;
}
