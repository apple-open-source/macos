/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jmx.loading;

import javax.ejb.SessionBean;
import javax.ejb.SessionContext;

/**
 * AbstractBean.java
 *
 *
 * @author <a href="mailto:julien_viet@yahoo.fr">Julien Viet</a>
 * @version
 *
 */

public abstract class AbstractBean
   implements SessionBean
{

   public void ejbActivate()
   {
   }

   public void ejbPassivate()
   {
   }

   public void setSessionContext(SessionContext ctx)
   {
   }

   public void unsetSessionContext()
   {
   }

   public void ejbRemove()
   {
   }

}
