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
import java.util.Map;
import java.util.List;
import java.util.Set;
import java.util.HashMap;
import java.util.ArrayList;
import java.util.HashSet;

/**
 * Implementaton of a CMP2 entity bean that is intended to demonstrate the
 * storage of large text and binary objects.
 *
 * @see javax.ejb.EntityBean
 *
 * @version <tt>$Revision: 1.1.2.4 $</tt>
 * @author  <a href="mailto:steve@resolvesw.com">Steve Coy</a>
 * @author  <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 */
public abstract class LOBBean implements EntityBean
{
   private EntityContext mEntityContext;

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

   public abstract Map getMapField();
   public abstract void setMapField(Map map);

   public abstract List getListField();
   public abstract void setListField(List list);

   public abstract Set getSetField();
   public abstract void setSetField(Set set);

   public abstract ValueHolder getValueHolder();
   public abstract void setValueHolder(ValueHolder valueHolder);

   public abstract ValueHolder getCleanGetValueHolder();
   public abstract void setCleanGetValueHolder(ValueHolder valueHolder);

   public abstract ValueHolder getStateFactoryValueHolder();
   public abstract void setStateFactoryValueHolder(ValueHolder valueHolder);

   // EntityBean implementation

   public Integer ejbCreate(Integer id) throws CreateException
   {
      setId(id);
      setMapField(new HashMap());
      setListField(new ArrayList());
      setSetField(new HashSet());
      setValueHolder(new ValueHolder(null));
      setCleanGetValueHolder(new ValueHolder(null));
      setStateFactoryValueHolder(new ValueHolder(null));
      return null;
   }

   public void ejbPostCreate(Integer id) {}

   public void ejbActivate() {}
   public void ejbLoad() {}
   public void ejbPassivate() {}
   public void ejbRemove() throws RemoveException {}
   public void ejbStore() {}

   public void setEntityContext(EntityContext ctx)
   {
      mEntityContext = ctx;
   }

   public void unsetEntityContext()
   {
      mEntityContext = null;
   }
}
