/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.cmp2.passivation.ejb;

import java.rmi.RemoteException;

import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.RemoveException;
import javax.ejb.EJBObject;
import javax.ejb.EJBLocalObject;

import org.jboss.logging.Logger;

/**
 * An entity bean to test the entity activation/passivation mechanism
 * provided by JBoss.
 * 
 * It is associated with a container configuration which causes it to be passivated shortly
 * after it has been accessed.
 * 
 * It has been designed to expose the bug described at
 * <a href="https://sourceforge.net/tracker/?group_id=22866&atid=376685&func=detail&aid=742197">
 *    Detail:769139 entityCtx.getEJBLocalObject() returns wrong instance</a>
 * and
 * <a href="https://sourceforge.net/tracker/?func=detail&atid=376685&aid=769139&group_id=22866">
 *    Detail:742197 getEJBLocalObject()  bug</a>
 *
 * @ejb.bean
 *    name="RapidlyPassivatedEntity"
 *    jndi-name="ejb/remote/RapidlyPassivatedEntity"
 *    local-jndi-name="ejb/local/RapidlyPassivatedEntity"
 *    view-type="both"
 *    type="CMP"
 *    reentrant="false"
 *    cmp-version="2.x"
 * @ejb.pk
 *    class="java.lang.Object"
 * @jboss.create-table "true"
 * @jboss.remove-table "true"
 * @jboss.unknown-pk
 *    class="java.lang.String"
 * @jboss.entity-command
 *    name="key-generator"
 * @jboss.container-configuration
 *    name="Short lived CMP 2.0 Entity Bean"
 *
 * @author <a href="mailto:steve@resolvesw.com">Steve Coy</a>
 */
public abstract class RapidlyPassivatedEntityBean implements EntityBean
{
   // Attributes ----------------------------------------------------

   private static Logger   log = Logger.getLogger(RapidlyPassivatedEntityBean.class);

   private EntityContext   mEntityContext;

   
   // Entity Attributes ----------------------------------------------------

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    */
   public abstract String getData(); 
   public abstract void setData(String data);
   

   // Business Methods ----------------------------------------------------

   /**
    * Return the pk of the object returned by {@link EntityContext#getEJBLocalObject}
    * @ejb.interface-method
    */
   public Object getIdViaEJBLocalObject()
   {
      Object key = mEntityContext.getPrimaryKey();
      EJBLocalObject local = mEntityContext.getEJBLocalObject();
      Object lkey = local.getPrimaryKey();
      log.info("key: "+key+", lkey: "+lkey+", local: "+local);
      return (Object)mEntityContext.getEJBLocalObject().getPrimaryKey();
   }
  

   /**
    * Return the pk of the object returned by {@link EntityContext#getEJBObject}
    * @ejb.interface-method
    */
   public Object getIdViaEJBObject()
   {
      try
      {
         return (Object)mEntityContext.getEJBObject().getPrimaryKey();
      }
      catch (RemoteException e)
      {
         throw new EJBException(e);
      }
   }
  

   // EJB Implementation ----------------------------------------------------

   /**
    * @ejb.create-method
    */
   public Object ejbCreate(String s)
      throws CreateException
   {
      setData(s);
      return null;   // as required by CMP 2.0 spec
   }
   
   public void ejbPostCreate(String s)
   {
      log.info("ejbPostCreate, ctx:"+mEntityContext
         +", pk:"+mEntityContext.getPrimaryKey()
         +", local:"+mEntityContext.getEJBLocalObject());
   }
   
  
   public void ejbActivate() throws EJBException, RemoteException
   {
      log.info("ejbActivate, ctx:"+mEntityContext
         +", pk:"+mEntityContext.getPrimaryKey()
         +", local:"+mEntityContext.getEJBLocalObject());
   }

   public void ejbLoad() throws EJBException, RemoteException
   {
      log.info("ejbLoad, ctx:"+mEntityContext
         +", pk:"+mEntityContext.getPrimaryKey()
         +", local:"+mEntityContext.getEJBLocalObject());
   }

   public void ejbPassivate() throws EJBException, RemoteException
   {
      log.info("ejbPassivate, ctx:"+mEntityContext
         +", pk:"+mEntityContext.getPrimaryKey()
         +", local:"+mEntityContext.getEJBLocalObject());
   }

   public void ejbRemove()
      throws RemoveException, EJBException, RemoteException
   {
   }

   public void ejbStore() throws EJBException, RemoteException
   {
   }

   public void setEntityContext(EntityContext ctx)
      throws EJBException, RemoteException
   {
      mEntityContext = ctx;
   }

   public void unsetEntityContext() throws EJBException, RemoteException
   {
      mEntityContext = null;
   }

}
