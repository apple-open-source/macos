/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.cmrstress.ejb;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.RemoveException;
import javax.naming.NamingException;

import org.apache.log4j.Logger;
import org.jboss.test.cmp2.cmrstress.interfaces.ChildLocal;
import org.jboss.test.cmp2.cmrstress.interfaces.ChildUtil;

/**
 * This class implements the "parent" side of 1..many unidirectional relationship.
 * It doesn't do anything particularly interesting besides provide the CMR getter
 * and a method (@see #getPropertyMap()) that provides a transactional context for
 * iterating over the CMR collection.
 * 
 * This code is based upon the original test case provided by Andrew May.
 *
 * @version <tt>$Revision: 1.1.2.2 $</tt>
 * @author  <a href="mailto:steve@resolvesw.com">Steve Coy</a>.
 *
 * @ejb.bean name="Parent"
 *           type="CMP"
 *           cmp-version="2.x"
 *           view-type="both"
 *           jndi-name="cmrstress/Parent"
 *           primkey-field="id"
 * 
 * @ejb.pk class="java.lang.String"
 *         generate="false"
 * 
 * @ejb.persistence table-name="StressedParent"
 * 
 * @@ejb.home generate="both"
 * @@ejb.interface generate="both"
 * 
 * @ejb.ejb-ref ejb-name="Child"
 *              view-type="local"
 *  
 * @ejb.transaction type="Supports"
 * 
 * @jboss.persistence
 *       create-table="true"
 *       remove-table="true"
 * @jboss.tuned-updates tune="true"
 */
public abstract class ParentBean implements EntityBean
{
   /**
    * CMP get method for Id attribute.
    * @ejb.interface-method view-type="remote"
    * @ejb.persistent-field
    * @jboss.column-name name="id"
    * @jboss.method-attributes read-only="true"
    */
   public abstract String getId();

   /**
    * CMP set method for Id attribute.
    * @ejb.interface-method view-type="remote"
    * @ejb.transaction type="Mandatory"
    */
   public abstract void setId(String id);

   /** 
    * Get Children that apply to this Parent.
    * 
    * @ejb.interface-method view-type="remote" 
    * @ejb.relation name="Parent-Child"
    *               role-name="Parent-has-Children"
    *               cascade-delete="no"
    *               target-ejb="Child"
    *               target-role-name="Child-of-Parent"
    *               target-cascade-delete="yes"
    * @jboss.target-relation related-pk-field="id"
    *                        fk-column="parentid"
    * jboss.method-attributes read-only="true"
    */
   public abstract Set getChildren();

   /** 
    * Set Children.
    * @ejb.interface-method view-type="remote" 
    * @ejb.transaction type="Mandatory"
    */
   public abstract void setChildren(Set children);

   /** 
    * Get a map of Child values.
    * This is the current axis of evil.
    * 
    * @ejb.interface-method view-type="remote" 
    * @ejb.transaction type="Required"
    * @jboss.method-attributes read-only="true"
    */
   public Map getPropertyMap()
   {
      Map result = new HashMap();
      Set children = getChildren();
      for (Iterator i = children.iterator(); i.hasNext(); )
      {
         ChildLocal c = (ChildLocal) i.next();
         result.put(c.getName(), c.getValue());
      }

      return result;
   }

   /**
    * Adds a child bean with the given attributes to this bean.
    * @ejb.interface-method view-type="remote" 
    * @ejb.transaction type="RequiresNew"
    */
   public void addChild(int k, String field1, String field2) throws CreateException
   {
      msLog.debug("Adding child with pk: " + k);
      try
      {
         getChildren().add(ChildUtil.getLocalHome().create(Integer.toString(k), field1, field2));
      }
      catch (NamingException e)
      {
         throw new EJBException(e);
      }
   }
   
   
   /**
    * Create method for Entity.
    * @ejb.create-method view-type="remote"
    * @ejb.transaction type="RequiresNew"
    */
   public String ejbCreate(String id) throws javax.ejb.CreateException
   {
      msLog.debug("Created '" + id + "'");
      setId(id);
      return null;
   }

   public void ejbPostCreate(String id)
   {
   }
   
   /**
    * @see javax.ejb.EntityBean#ejbActivate()
    */
   public void ejbActivate()
   {
   }

   /**
    * @see javax.ejb.EntityBean#ejbLoad()
    */
   public void ejbLoad()
   {
   }

   /**
    * @see javax.ejb.EntityBean#ejbPassivate()
    */
   public void ejbPassivate()
   {
   }

   /**
    * @see javax.ejb.EntityBean#ejbRemove()
    */
   public void ejbRemove() throws RemoveException
   {
      msLog.debug("Removed");
   }

   /**
    * @see javax.ejb.EntityBean#ejbStore()
    */
   public void ejbStore()
   {
   }

   /**
    * @see javax.ejb.EntityBean#setEntityContext(javax.ejb.EntityContext)
    */
   public void setEntityContext(EntityContext arg0)
   {
   }

   /**
    * @see javax.ejb.EntityBean#unsetEntityContext()
    */
   public void unsetEntityContext()
   {
   }

   private static final Logger   msLog = Logger.getLogger(ParentBean.class);

}
