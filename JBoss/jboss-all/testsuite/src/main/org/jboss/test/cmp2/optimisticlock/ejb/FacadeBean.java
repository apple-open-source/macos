/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.cmp2.optimisticlock.ejb;

import org.jboss.test.cmp2.optimisticlock.interfaces.CmpEntityLocal;
import org.jboss.test.cmp2.optimisticlock.interfaces.CmpEntityLocalHome;
import org.apache.log4j.Category;

import javax.ejb.SessionBean;
import javax.ejb.CreateException;
import javax.ejb.SessionContext;
import javax.ejb.EJBException;
import javax.ejb.FinderException;
import javax.naming.NamingException;
import javax.naming.InitialContext;

/**
 * @ejb.bean
 *    name="Facade"
 *    type="Stateless"
 *    jndi-name="FacadeBean"
 *    view-type="remote"
 *
 * @author <a href="mailto:aloubyansky@hotmail.com">Alex Loubyansky</a>
 */
public class FacadeBean
   implements SessionBean
{
   // Attributes --------------------------------------------
   static Category log = Category.getInstance(FacadeBean.class);

   // Business methods --------------------------------------
   /**
    * @ejb.interface-method
    */
   public void createCmpEntity(String entityJndiName,
                               Integer integerField,
                               String stringField,
                               java.util.Date dateField)
   {
      log.debug("createCmpEntity> entityJndiName=" + entityJndiName
         + "; integerField=" + integerField
         + "; stringField=" + stringField
         + "; dateField=" + dateField);

      try
      {
         CmpEntityLocalHome entityHome = getCmpEntityHome(entityJndiName);
         entityHome.create(integerField, stringField, dateField);
      }
      catch(NamingException ne)
      {
         throw new EJBException("Failed to lookup CmpEntityBeanLocalHome", ne);
      }
      catch(CreateException ce)
      {
         throw new EJBException("Failed to create [" + integerField
            + "," + stringField + "," + dateField + "]", ce);
      }
   }

   /**
    * @ejb.interface-method
    */
   public void delayedUpdateStringField(String entityJndiName,
                                        Integer integerField,
                                        String newStringField,
                                        long delay)
   {
      try
      {
         log.debug("delayedUpdateStringField> findByPrimaryKey: "
            + "entityJndiName=" + entityJndiName
            + "; integerField=" + integerField
            + "; stringField=" + newStringField
            + "; delay=" + delay);

         CmpEntityLocal entity = getCmpEntityHome(entityJndiName).
            findByPrimaryKey(integerField);
         // touch the entity, it'll involve it in the tx
         entity.getDateField();

         log.debug("delayedUpdateStringField> sleeping for " + delay);
         try {
            Thread.sleep(delay);
         } catch(InterruptedException ignore) { }

         log.debug("delayedUpdateStringField> setStringField: "
            + "entityJndiName=" + entityJndiName
            + "; integerField=" + integerField
            + "; stringField=" + newStringField
            + "; delay=" + delay);
         entity.setStringField(newStringField);
      }
      catch(NamingException ne)
      {
         throw new EJBException("Failed to lookup CmpEntityBeanLocalHome", ne);
      }
      catch(FinderException fe)
      {
         throw new EJBException("Entity with pk " + integerField + " not found.");
      }
   }

   // SessionBean implementation ----------------------------
   public void setSessionContext(SessionContext ctx) {}
   public void ejbCreate() throws CreateException {}
   public void ejbRemove() {}
   public void ejbActivate() {}
   public void ejbPassivate() {}

   // Private -----------------------------------------------
   private CmpEntityLocalHome getCmpEntityHome(String entityJndiName)
      throws NamingException
   {
      InitialContext ic = new InitialContext();
      CmpEntityLocalHome cmpEntityHome = (CmpEntityLocalHome)
         ic.lookup(entityJndiName);
      return cmpEntityHome;
   }
}
