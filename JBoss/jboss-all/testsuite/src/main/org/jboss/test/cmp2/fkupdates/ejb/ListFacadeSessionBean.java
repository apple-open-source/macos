/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.fkupdates.ejb;

import org.jboss.test.cmp2.fkupdates.util.DirtyTrackerInterceptor;

import javax.naming.NamingException;
import javax.ejb.FinderException;
import javax.ejb.RemoveException;
import javax.ejb.CreateException;
import java.util.Iterator;
import java.util.List;


/**
 * @ejb.bean
 *    name="ListFacade"
 *	   type="Stateless"
 *    view-type="remote"
 * @ejb.util generate="physical"
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public abstract class ListFacadeSessionBean
   implements javax.ejb.SessionBean
{
   /**
    * @ejb.interface-method
    * @ejb:transaction  type="RequiresNew"
    */
   public void initData()
      throws NamingException, FinderException, RemoveException, CreateException
   {
      ListEntityLocalHome listHome = ListEntityUtil.getLocalHome();
      for(Iterator listIter = listHome.findAll().iterator(); listIter.hasNext();)
      {
         ListEntityLocal listEntity = (ListEntityLocal)listIter.next();
         listEntity.remove();
      }
      ListEntityLocal list = listHome.create(new Integer(1), "list");

      ListItemEntityLocalHome listItemHome = ListItemEntityUtil.getLocalHome();
      for(Iterator listItemIter = listItemHome.findAll().iterator(); listItemIter.hasNext();)
      {
         ListItemEntityLocal li = (ListItemEntityLocal)listItemIter.next();
         li.remove();
      }

      ListItemEntityLocal li = listItemHome.create(new Integer(11), null);
      li.setList(list);
   }

   /**
    * @ejb.interface-method
    * @ejb:transaction  type="RequiresNew"
    */
   public List loadItems(ListEntityPK pk)
      throws Exception, javax.naming.NamingException
   {
      DirtyTrackerInterceptor dirtyTracker = DirtyTrackerInterceptor.getCurrentInstance();
      dirtyTracker.clearDirtyFields();
      dirtyTracker.startTracking();

      ListEntityLocal local = ListEntityUtil.getLocalHome().findByPrimaryKey(pk);
      for(Iterator iter = local.getItems().iterator(); iter.hasNext();)
      {
         ListItemEntityLocal item = (ListItemEntityLocal)iter.next();
         item.getListIdFK();
      }
      return dirtyTracker.getDirtyFields();
   }

   /**
    * @ejb.create-method
    */
   public void ejbCreate() throws javax.ejb.CreateException {}

   public void setSessionContext(javax.ejb.SessionContext ctx) {}
}
