/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.entity.interfaces;

import javax.ejb.EJBLocalHome;
import javax.ejb.CreateException;
import javax.ejb.FinderException;

/**
 * A Bad entity.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public interface PathologicalEntityHome
   extends EJBLocalHome
{
   public PathologicalEntity create(String name)
      throws CreateException;
	
   public PathologicalEntity findByPrimaryKey(String name)
	throws FinderException;
}
