/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.audit.interfaces;

import javax.ejb.EJBLocalObject;

/**
 * An entity bean with audit fields behind the scenes.
 *
 * @author    Adrian.Brock@HappeningTimes.com
 * @version   $Revision: 1.1.2.1 $
 */
public interface Audit
   extends EJBLocalObject
{
   public String getId();

   public String getStringValue();
   public void setStringValue(String s);
}
