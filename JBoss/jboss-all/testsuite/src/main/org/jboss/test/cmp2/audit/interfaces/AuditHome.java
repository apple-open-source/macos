/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.audit.interfaces;

import java.util.Collection;
import javax.ejb.CreateException;
import javax.ejb.EJBLocalHome;
import javax.ejb.FinderException;

/**
 * An entity bean with audit fields behind the scenes.
 *
 * @author    Adrian.Brock@HappeningTimes.com
 * @version   $Revision: 1.1.2.1 $
 */
public interface AuditHome
   extends EJBLocalHome
{
	public Audit create(String id)
         throws CreateException;

	public Audit findByPrimaryKey(String id)
         throws FinderException;

	public Collection findAll()
         throws FinderException;
}
