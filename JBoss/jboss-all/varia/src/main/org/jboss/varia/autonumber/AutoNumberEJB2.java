/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.varia.autonumber;

import javax.ejb.EntityBean;
import javax.ejb.CreateException;
import javax.naming.InitialContext;

/**
 * The CMP 2 compatible version of AutoNumberEJB.
 *
 * @version <tt>$Revision: 1.3.2.1 $</tt>
 * @author <a href="mailto:michel.anke@wolmail.nl">Michel de Groot</a>
 * @author <a href="mailto:icoloma@iverdino.com">Ignacio Coloma</a>
 */
public abstract class AutoNumberEJB2
   implements EntityBean
{
   public abstract Integer getValue();

   public abstract void setValue(Integer value);
   
   public abstract String getName();

   public abstract void setName(String name);
   
   public String ejbCreate(String name) throws CreateException
   {
      setName(name);
      setValue(new Integer(0));

      return null;
   }
   
   public void ejbPostCreate(String name) {}
   
   public void ejbActivate() {}

   public void ejbPassivate() {}

   public void ejbLoad() {}

   public void ejbStore() {}

   public void ejbRemove() {}

   public void setEntityContext(javax.ejb.EntityContext unused) {}

   public void unsetEntityContext() {}
   
}
