/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.audit.interfaces;

import java.util.Date;

import javax.ejb.EJBLocalObject;

/**
 * An entity bean with audit fields mapped to cmp fields.
 *
 * @author    Adrian.Brock@HappeningTimes.com
 * @version   $Revision: 1.1.2.1 $
 */
public interface AuditMapped
   extends Audit
{
   public String getCreatedBy();
   public void setCreatedBy(String s);
   public Date getCreatedTime();
   public void setCreatedTime(Date d);
   public String getUpdatedBy();
   public void setUpdatedBy(String s);
   public Date getUpdatedTime();
   public void setUpdatedTime(Date d);
}
