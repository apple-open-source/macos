/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.cmrtransaction.interfaces;

import java.util.Collection;

import javax.ejb.EJBLocalObject;

/**
 * @author  B Stansberry brian_stansberry@wanconcepts.com
 */
public interface TreeLocal extends EJBLocalObject
{
    String getId();

    int getSortOrder();

    Collection getMenuChildren();

    TreeLocal getMenuParent();
    
    void setMenuParent(TreeLocal menuParent);

    void setPrecededBy(TreeLocal precedingNode);
}
