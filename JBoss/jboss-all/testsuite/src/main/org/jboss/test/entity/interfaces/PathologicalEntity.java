/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.entity.interfaces;

import javax.ejb.EJBLocalObject;

/**
 * A Bad entity.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public interface PathologicalEntity
   extends EJBLocalObject
{
   public String getName();

   public String getSomething();
   public void setSomething(String value);
}

