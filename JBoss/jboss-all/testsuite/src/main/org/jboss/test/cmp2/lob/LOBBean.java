/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.cmp2.lob;

import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.CreateException;
import javax.ejb.RemoveException;

/**
 * Implementaton of a CMP2 entity bean that is intended to demonstrate the
 * storage of large text and binary objects.
 *
 * @see javax.ejb.EntityBean.
 *
 * @version <tt>$Revision: 1.1.2.1 $</tt>
 * @author  <a href="mailto:steve@resolvesw.com">Steve Coy</a>
 *   
 */
public abstract class LOBBean implements EntityBean
{
   // Attributes ----------------------------------------------------

   private EntityContext mEntityContext;

   // Public --------------------------------------------------------

   /**
    * Returns the primary key
    * @return Integer
    */
   public abstract Integer getId();

   /**
    * Sets the primary key.
    * @param id
    */
   public abstract void setId(Integer id);

   /**
    * Returns the large string attribute.
    * @return String
    */
   public abstract String getBigString();

	/**
	 * Sets the value of the large string attribute.
     * The idea here is to store it in a CLOB object in associated database table
     * so that we can check the container's LOB functionality properly.
	 * @param s
	 */
   public abstract void setBigString(String s);

	/**
	 * Returns the content of the large binary object.
	 * @return byte[]
	 */
   public abstract byte[] getBinaryData();

	/**
	 * Sets the content of the large binary object.
	 * The idea here is to store it in a BLOB object in the associated database
	 * table so that we check the container's LOB functionality properly.
	 * @param data
	 */
   public abstract void setBinaryData(byte[] data);

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
    * @see javax.ejb.EntityBean#ejbCreate()
    */
   public Integer ejbCreate(Integer id) throws CreateException
   {
      setId(id);
      return null;
   }

   /**
    * @see javax.ejb.EntityBean#ejbCreate()
    */
   public void ejbPostCreate(Integer id)
   {
   }

   /**
    * @see javax.ejb.EntityBean#ejbRemove()
    */
   public void ejbRemove() throws RemoveException
   {
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
   public void setEntityContext(EntityContext ctx)
   {
      mEntityContext = ctx;
   }

   /**
    * @see javax.ejb.EntityBean#unsetEntityContext()
    */
   public void unsetEntityContext()
   {
      mEntityContext = null;
   }

}
