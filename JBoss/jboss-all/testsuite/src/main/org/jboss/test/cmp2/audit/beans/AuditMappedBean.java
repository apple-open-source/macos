/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.audit.beans;

import java.util.Date;

import javax.ejb.CreateException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;

/**
 * An entity bean with audit fields mapped to CMP fields.
 *
 * @author    Adrian.Brock@HappeningTimes.com
 * @version   $Revision: 1.1.2.1 $
 */
public abstract class AuditMappedBean
   extends AuditBean
{
   public abstract String getCreatedBy();
   public abstract void setCreatedBy(String s);
   public abstract Date getCreatedTime();
   public abstract void setCreatedTime(Date d);
   public abstract String getUpdatedBy();
   public abstract void setUpdatedBy(String s);
   public abstract Date getUpdatedTime();
   public abstract void setUpdatedTime(Date d);
}
