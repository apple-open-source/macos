
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.ejbconf.beans.ejb; // Generated package name

import java.rmi.RemoteException;

import javax.ejb.CreateException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;

/**
 * ReadOnlyBean.java
 *
 *
 * Created: Tue Jan 22 17:13:36 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 *
 *
 * @ejb:bean   name="ReadOnly"
 *             jndi-name="ReadOnly"
 *             local-jndi-name="LocalReadOnly"
 *             view-type="both"
 *             type="CMP"
 *             cmp-version="2.x"
 *             primkey-field="id"
 * @ejb:pk class="java.lang.Integer"
 * @ejb:finder signature="java.util.Collection findAll()"
 * @jboss:read-only read-only="true"
 */

public abstract class ReadOnlyBean implements EntityBean  
{
   public ReadOnlyBean ()
   {
      
   }

   /**
    * Describe <code>ejbCreate</code> method here.
    *
    * @param id an <code>Integer</code> value
    * @param value an <code>Integer</code> value
    * @return an <code>Integer</code> value
    * @ejb:create-method
    */
   public Integer ejbCreate(Integer id, Integer value) throws CreateException
   {
      setId(id);
      setValue(value);
      return null;
   }

   public void ejbPostCreate(Integer id, Integer value)
   {
   }

   
   /**
    * Get the value of id.
    * @return value of id.
    * @ejb:persistent-field
    * @ejb:interface-method
    */
   public abstract Integer getId(); 

   
   /**
    * Set the value of id.
    * @param v  Value to assign to id.
    */
   public abstract void setId(Integer id); 
   
   
   /**
    * Get the value of value.
    * @return value of value.
    * @ejb:persistent-field
    * @ejb:interface-method
    */
   public abstract Integer getValue(); 
   
   /**
    * Set the value of value.
    * @param v  Value to assign to value.
    * @ejb:interface-method
    */
   public abstract void setValue(Integer  value); 

   
   public void ejbActivate() throws RemoteException 
   {
   }
   
   public void ejbPassivate() throws RemoteException 
   {
   }
   
   public void ejbLoad() throws RemoteException 
   {
   }
   
   public void ejbStore() throws RemoteException 
   {
   }
   
   public void ejbRemove() throws RemoteException 
   {
   }
   
   public void setEntityContext(EntityContext ctx) throws RemoteException 
   {
   }
   
   public void unsetEntityContext() throws RemoteException 
   {
   }
   
}// ReadOnlyBean
