/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.keygen.ejb;

import javax.ejb.CreateException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.RemoveException;

/** The Integer pk based bean implementation
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public abstract class IntegerPKBean implements EntityBean
{
   public Integer ejbCreate(String value) throws CreateException
   {
      setValue(value);
      return null;
   }
   public void ejbPostCreate(String value)
   {
   }

   public abstract Integer getKey();
   public abstract void setKey(Integer key);
   public abstract String getValue();
   public abstract void setValue(String value);

   public void setEntityContext(EntityContext ctx)
   {
   }

   public void ejbActivate()
   {
   }

   public void ejbLoad()
   {
   }

   public void ejbStore()
   {
   }

   public void ejbPassivate()
   {
   }

   public void ejbRemove() throws RemoveException
   {
   }

   public void unsetEntityContext()
   {
   }
}
