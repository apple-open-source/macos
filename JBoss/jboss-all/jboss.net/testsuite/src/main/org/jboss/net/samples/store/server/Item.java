/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: Item.java,v 1.1.4.1 2002/09/12 16:18:07 cgjung Exp $

package org.jboss.net.samples.store.server;

import java.util.Collection;
import javax.ejb.EJBLocalObject;
import javax.ejb.EJBLocalHome;
import javax.ejb.CreateException;
import javax.ejb.EntityContext;
import javax.ejb.EntityBean;
import javax.ejb.FinderException;

/**
 * Compact local entity bean that is exposed as a value object
 * via Axis.
 * <br>
 * <h3>Change History</h3>
 * <ul>
 * </ul>
 * @created 21.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.4.1 $
 */

public interface Item extends EJBLocalObject, org.jboss.net.samples.store.Item {

   //wsdl purposes, fuck that
   public String getName();
   public void setName(String name);

   /** home interface */
   public interface Home extends EJBLocalHome {
      public Item create(String name) throws CreateException;
      public Item findByPrimaryKey(String name) throws FinderException;
      public Collection findAll() throws FinderException;
   }

   /** server-side implementation uses CMP2.x */
   public static abstract class Bean
      implements EntityBean, org.jboss.net.samples.store.Item {
         
      public String ejbCreate(String name) throws CreateException {
         setName(name);
         return null;
      }

      //
      // What follows is just CMP2.x mumbo-jumbo
      //

      transient private EntityContext ctx;

      public void ejbPostCreate(String name) {
      }

      public void setEntityContext(EntityContext ctx) {
         this.ctx = ctx;
      }

      public void unsetEntityContext() {
         this.ctx = null;
      }

      public void ejbActivate() {
      }

      public void ejbPassivate() {
      }

      public void ejbLoad() {
      }

      public void ejbStore() {
      }

      public void ejbRemove() {
      }
   }
}