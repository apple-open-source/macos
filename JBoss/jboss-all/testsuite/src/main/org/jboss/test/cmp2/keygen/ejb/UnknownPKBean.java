/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.test.cmp2.keygen.ejb;

import javax.ejb.CreateException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.RemoveException;

/**
 *
 * @author <a href="mailto:jeremy@boynes.com">Jeremy Boynes</a>
 */
public abstract class UnknownPKBean implements EntityBean
{
   public Object ejbCreate(String value) throws CreateException
   {
      setValue(value);
      return null;
   }

   public void ejbPostCreate(String value)
   {
   }

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
