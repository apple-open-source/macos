/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jmx.loading;

import javax.ejb.SessionBean;


/**
 * ConcreteBean.java
 *
 *
 * @author <a href="mailto:julien_viet@yahoo.fr">Julien Viet</a>
 * @version
 *
 *
 * @ejb:bean        name="Concrete"
 *                  jndi-name="loading/cpmanifest"
 *                  view-type="remote"
 *                  type="Stateless"
 *
 * @ejb:home        extends="javax.ejb.EJBHome"
 *
 * @ejb:interface   extends="javax.ejb.EJBObject"
 *
 */

public class ConcreteBean
   extends AbstractBean
{

   /**
    * @ejb:create-method
    */
   public void ejbCreate() 
   {
   }

}
