/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.keygen.ejb;

import java.util.Collection;
import javax.ejb.CreateException;
import javax.ejb.EJBLocalHome;
import javax.ejb.FinderException;

/** An Integer pk bean local home
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public interface IntegerPKLocalHome extends EJBLocalHome
{
   public UnknownPKLocal create(String value) throws CreateException;
   public UnknownPKLocal findByPrimaryKey(Integer pk) throws FinderException;
   public Collection findAll() throws FinderException;
}
