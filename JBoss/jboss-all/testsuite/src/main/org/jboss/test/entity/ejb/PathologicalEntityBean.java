/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.entity.ejb;

import javax.ejb.CreateException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.RemoveException;

import org.jboss.test.entity.interfaces.Pathological;
import org.jboss.test.entity.interfaces.PathologicalEntityHome;
import org.jboss.test.entity.interfaces.PathologicalEntity;

/**
 * A Bad entity.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class PathologicalEntityBean
   implements EntityBean
{
   private EntityContext entityContext;

   private String name;

   public String getName()
   {
      pathological();
      return name;
   }

   public String getSomething()
   {
      pathological();
      return "Something";
   }

   public void setSomething(String value)
   {
      pathological();
   }
	
   public String ejbCreate(String name)
      throws CreateException
   {
      pathological();
      this.name = name;
      return name;
   }
	
   public void ejbPostCreate(String name)
      throws CreateException
   {
   }

   public String ejbFindByPrimaryKey(String name)
   {
      pathological();
      return name;
   }
	
   public void ejbActivate()
   {
      pathological();
   }
	
   public void ejbLoad()
   {
      pathological();
   }
	
   public void ejbPassivate()
   {
      pathological();
   }
	
   public void ejbRemove()
      throws RemoveException
   {
      pathological();
   }
	
   public void ejbStore()
   {
      pathological();
   }
	
   public void setEntityContext(EntityContext context)
   {
      pathological();
      entityContext = context;
   }
	
   public void unsetEntityContext()
   {
      pathological();
      entityContext = null;
   }

   private void pathological()
   {
      if (Pathological.isPathological())
         throw new Error("Handle this correctly please");
   }
}
