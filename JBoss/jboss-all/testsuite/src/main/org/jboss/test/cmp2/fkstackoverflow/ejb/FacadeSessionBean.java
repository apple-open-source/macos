/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.fkstackoverflow.ejb;

import org.apache.log4j.Category;

import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.ejb.CreateException;
import javax.ejb.EJBException;


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
   private static Category log = Category.getInstance(FacadeSessionBean.class);

   // Business methods

   /**
    * @ejb.interface-method
    */
   public void testSimple()
   {
      try
      {
         ChildLocalHome ch = ChildUtil.getLocalHome();
         ChildLocal child = ch.create(new Long(1), "Avoka", new Long(1));
         child.setSimpleParentId(new Long(2));
      }
      catch(Exception e)
      {
         throw new EJBException(e);
      }
   }

   /**
    * @ejb.interface-method
    */
   public void testComplex()
   {
      try
      {
         ChildLocalHome ch = ChildUtil.getLocalHome();
         ChildLocal child = ch.create(new Long(10), "Avoka", new Long(10));
         child.setComplexParentId1(new Long(2));
         child.setComplexParentId2(new Long(3));
      }
      catch(Exception e)
      {
         throw new EJBException(e);
      }
   }

   // SessionBean implementation

   /**
    * @exception  CreateException Description of Exception
    * @ejb.create-method
    */
   public void ejbCreate() throws CreateException {}
   public void ejbActivate() {}
   public void ejbPassivate() {}
   public void ejbRemove() {}

   public void setSessionContext(SessionContext ctx) {}
}
