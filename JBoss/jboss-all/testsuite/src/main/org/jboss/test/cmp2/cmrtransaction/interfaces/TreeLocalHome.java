/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.cmrtransaction.interfaces;

import javax.ejb.EJBLocalHome;
import javax.ejb.CreateException;
import javax.ejb.FinderException;

/**
 * @author  B Stansberry brian_stansberry@wanconcepts.com
 */
public interface TreeLocalHome extends EJBLocalHome
{
    TreeLocal create(String id, TreeLocal parent) throws CreateException;

    TreeLocal findByPrimaryKey(String id) throws FinderException;

}
