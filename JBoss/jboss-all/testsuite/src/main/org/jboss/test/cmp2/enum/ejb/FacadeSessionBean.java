/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.enum.ejb;

import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.ejb.CreateException;

/**
 * @ejb:bean
 *    type="Stateless"
 *    name="Facade"
 *    view-type="remote"
 * @ejb.util generate="physical"
 * @ejb:transaction type="Required"
 * @ejb:transaction-type type="Container"
 */
public class FacadeSessionBean
   implements SessionBean
{
   // Business methods

   /**
    * @ejb.interface-method
    */
   public ColorEnum getColorForId(Long id)
      throws Exception
   {
      ChildLocal child = ChildUtil.getLocalHome().findByPrimaryKey(id);
      return child.getColor();
   }

   /**
    * @ejb.interface-method
    */
   public AnimalEnum getAnimalForId(Long id)
      throws Exception
   {
      ChildLocal child = ChildUtil.getLocalHome().findByPrimaryKey(id);
      return child.getAnimal();
   }

   /**
    * @ejb.interface-method
    */
   public void setColor(Long id, ColorEnum color)
      throws Exception
   {
      ChildLocal child = ChildUtil.getLocalHome().findByPrimaryKey(id);
      child.setColor(color);
   }

   /**
    * @ejb.interface-method
    */
   public void setAnimal(Long id, AnimalEnum animal)
      throws Exception
   {
      ChildLocal child = ChildUtil.getLocalHome().findByPrimaryKey(id);
      child.setAnimal(animal);
   }

   /**
    * @ejb.interface-method
    */
   public void createChild(Long childId)
      throws Exception
   {
      ChildUtil.getLocalHome().create(childId);
   }

   /**
    * @ejb.interface-method
    */
   public void removeChild(Long childId)
      throws Exception
   {
      ChildUtil.getLocalHome().remove(childId);
   }

   // SessionBean implementation

   /**
    * @exception  CreateException Description of Exception
    * @ejb.create-method
    */
   public void ejbCreate() throws CreateException
   {
   }

   public void ejbActivate()
   {
   }

   public void ejbPassivate()
   {
   }

   public void ejbRemove()
   {
   }

   public void setSessionContext(SessionContext ctx)
   {
   }
}
