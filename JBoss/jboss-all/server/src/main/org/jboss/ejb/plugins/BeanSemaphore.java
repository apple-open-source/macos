/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins;


import org.jboss.util.Semaphore;


/**
 * Allow ability to mark a Semaphore as no longer valid for mutex locking
 *
 * @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
 * @version $Revision: 1.2 $
 */
public class BeanSemaphore extends Semaphore
{
    private boolean m_valid = true;
    public BeanSemaphore(int allowed)
    {
	super(allowed);
	m_valid = true;
    }

    public void invalidate()
    {
	m_valid = false;
    }

    public boolean isValid()
    {
	return m_valid;
    }
    
}
