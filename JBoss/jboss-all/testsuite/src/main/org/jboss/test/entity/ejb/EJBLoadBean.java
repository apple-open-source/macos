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

import org.jboss.logging.Logger;

/**
 * A bean to test whether ejb load was called.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.4.2 $
 */
public class EJBLoadBean
   implements EntityBean
{
   private static final Logger log = Logger.getLogger(EJBLoadBean.class);

   private EntityContext entityContext;

   private String name;

   private boolean ejbLoadCalled = false;

   private boolean active = false;

   public String getName()
   {
      log.info("getName");
      assertActive();
      return name;
   }

   public boolean wasEJBLoadCalled()
   {
      log.info("wasEJBLoadCalled");
      assertActive();
      boolean result = ejbLoadCalled;
      ejbLoadCalled = false;
      return result;
   }

   public void noTransaction()
   {
      log.info("noTransaction");
      assertActive();
      ejbLoadCalled = false;
   }
	
   public String ejbCreate(String name)
      throws CreateException
   {
      assertActive();
      log.info("ejbCreate");
      this.name = name;
      return name;
   }
	
   public void ejbPostCreate(String name)
      throws CreateException
   {
      assertActive();
      log.info("ejbPostCreate");
   }

   public String ejbFindByPrimaryKey(String name)
   {
      log.info("ejbFindByPrimaryKey");
      return name;
   }
	
   public void ejbActivate()
   {
      log.info("ejbActivate");
      active = true;
   }
	
   public void ejbLoad()
   {
      log.info("ejbLoad");
      ejbLoadCalled = true;
      assertActive();
   }
	
   public void ejbPassivate()
   {
      log.info("ejbPassivate");
      assertActive();
      active = false;
   }
	
   public void ejbRemove()
      throws RemoveException
   {
      log.info("ejbRemove");
      assertActive();
      active = false;
   }
	
   public void ejbStore()
   {
      log.info("ejbStore");
      assertActive();
   }
	
   public void setEntityContext(EntityContext context)
   {
      log.info("setEntityContext");
      entityContext = context;
   }
	
   public void unsetEntityContext()
   {
      log.info("unsetEntityContext");
      entityContext = null;
   }

   private void assertActive()
   {
      if (active == false)
         throw new RuntimeException("The bean is not active");
   }
}
