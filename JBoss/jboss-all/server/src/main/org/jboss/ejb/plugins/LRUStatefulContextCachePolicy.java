/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins;


import java.util.TimerTask;

import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.MetaData;

import org.w3c.dom.Element;

/**
 * Least Recently Used cache policy for StatefulSessionEnterpriseContexts.
 *
 * @author <a href="mailto:simone.bordet@compaq.com">Simone Bordet</a>
 * @version $Revision: 1.8 $
 */
public class LRUStatefulContextCachePolicy extends LRUEnterpriseContextCachePolicy
{
	// Constants -----------------------------------------------------

	// Attributes ----------------------------------------------------
	/* The age after which a bean is automatically removed */
	private long m_maxBeanLife;
	/* The remover timer task */
	private TimerTask m_remover;
	/* The period of the remover's runs */
	private long m_removerPeriod;
	/* The stateful cache */
	private StatefulSessionInstanceCache m_cache;

	// Static --------------------------------------------------------

	// Constructors --------------------------------------------------
	/**
	 * Creates a LRU cache policy object given the instance cache that use
	 * this policy object.
	 */
	public LRUStatefulContextCachePolicy(AbstractInstanceCache eic)
	{
		super(eic);
		m_cache = (StatefulSessionInstanceCache)eic;
	}

	// Public --------------------------------------------------------

	// Monitorable implementation ------------------------------------

	// Z implementation ----------------------------------------------
	public void start()
	{
		super.start();
		if (m_maxBeanLife > 0)
		{
			m_remover = new RemoverTask(m_removerPeriod);
         long delay = (long) (Math.random() * m_removerPeriod);
         tasksTimer.schedule(m_remover, delay, m_removerPeriod);
		}
	}

	public void stop()
	{
		if (m_remover != null) {m_remover.cancel();}
		super.stop();
	}
	/**
	 * Reads from the configuration the parameters for this cache policy, that are
	 * all optionals.
	 */
	public void importXml(Element element) throws DeploymentException
	{
		super.importXml(element);

		String rp = MetaData.getElementContent(MetaData.getOptionalChild(element, "remover-period"));
		String ml = MetaData.getElementContent(MetaData.getOptionalChild(element, "max-bean-life"));
		try
		{
			if (rp != null)
			{
				int p = Integer.parseInt(rp);
				if (p <= 0) {throw new DeploymentException("Remover period can't be <= 0");}
				m_removerPeriod = p * 1000;
			}
			if (ml != null)
			{
				int a = Integer.parseInt(ml);
				if (a <= 0) {throw new DeploymentException("Max bean life can't be <= 0");}
				m_maxBeanLife = a * 1000;
			}
		}
		catch (NumberFormatException x)
		{
			throw new DeploymentException("Can't parse policy configuration", x);
		}
	}

	// Y overrides ---------------------------------------------------

	// Package protected ---------------------------------------------

	// Protected -----------------------------------------------------

	// Private -------------------------------------------------------

	// Inner classes -------------------------------------------------
	/**
	 * This TimerTask removes beans that have not been called for a while.
	 */
	protected class RemoverTask extends OveragerTask
	{
		protected RemoverTask(long period)
		{
			super(period);
		}
		protected String getTaskLogMessage() {return "Removing from cache bean";}
		protected String getJMSTaskType() {return "REMOVER";}
		protected void kickOut(LRUCacheEntry entry) {remove(entry.m_key);}
		protected long getMaxAge() {return m_maxBeanLife;}

		public void run()
		{
         if( m_cache == null )
         {
            cancel();
            return;
         }

			synchronized (m_cache.getCacheLock())
			{
				// Remove beans from cache, if present
				super.run();

				// Now remove passivated beans
				m_cache.removePassivated(getMaxAge() - super.getMaxAge());
			}
		}
	}
}
