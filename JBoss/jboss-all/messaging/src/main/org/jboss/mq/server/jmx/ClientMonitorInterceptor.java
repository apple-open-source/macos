/*
 * JBoss, the OpenSource J2EE webOS
 * 
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mq.server.jmx;

import org.jboss.mq.server.JMSServerInterceptor;

/**
 * JMX MBean implementation DelayInterceptor.
 *
 * @jmx:mbean extends="org.jboss.mq.server.jmx.InterceptorMBean"
 * @author     <a href="hiram.chirino@jboss.org">Hiram Chirino</a>
 * @version    $Revision: 1.1.4.1 $
 */
public class ClientMonitorInterceptor
	extends InterceptorMBeanSupport
	implements ClientMonitorInterceptorMBean {

	private org.jboss.mq.server.ClientMonitorInterceptor interceptor =
		new org.jboss.mq.server.ClientMonitorInterceptor();

	long clientTimeout = 1000 * 60;
	Thread serviceThread;

	public JMSServerInterceptor getInterceptor() {
		return interceptor;
	}

	/**
	 * Returns the clientTimeout.
	 * @return long
	 * @jmx:managed-attribute
	 */
	public long getClientTimeout() {
		return clientTimeout;
	}

	/**
	 * Sets the clientTimeout.
	 * @param clientTimeout The clientTimeout to set
	 * @jmx:managed-attribute
	 */
	public void setClientTimeout(long clientTimeout) {
		this.clientTimeout = clientTimeout;
	}

	/**
	 * @see org.jboss.system.ServiceMBeanSupport#startService()
	 */
	protected void startService() throws Exception {
		super.startService();
		if( serviceThread != null )
			return;
			
		serviceThread = new Thread(new Runnable() {
			public void run() {
				try {
					while(true) {
						Thread.sleep(clientTimeout);				
						interceptor.disconnectInactiveClients(
							System.currentTimeMillis() - clientTimeout);
					}
				} catch (InterruptedException e) {
				}
			}
		}, "ClientMonitor Service Thread");
		serviceThread.setDaemon(true);
		serviceThread.start();
	}

	/**
	 * @see org.jboss.system.ServiceMBeanSupport#stopService()
	 */
	protected void stopService() throws Exception {
		if (serviceThread != null) {
			serviceThread.interrupt();
			serviceThread = null;
		}
		super.stopService();
	}

}
