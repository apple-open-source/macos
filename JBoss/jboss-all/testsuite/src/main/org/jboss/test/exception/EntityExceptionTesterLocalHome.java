/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.exception;

import javax.ejb.CreateException;
import javax.ejb.EJBLocalHome;
import javax.ejb.FinderException;

/**
 * A test of entity beans exceptions.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public interface EntityExceptionTesterLocalHome
   extends EJBLocalHome
{
    public EntityExceptionTesterLocal create(String key)
       throws CreateException;

    public EntityExceptionTesterLocal findByPrimaryKey(String key)
       throws FinderException;
} 