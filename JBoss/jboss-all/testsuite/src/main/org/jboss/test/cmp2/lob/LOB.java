/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.cmp2.lob;

import java.rmi.RemoteException;
import java.util.Map;
import java.util.List;
import java.util.Set;
import javax.ejb.EJBObject;

/**
 * Renote interface for a LOBBean.
 *
 * @see javax.ejb.EJBObject
 *
 * @version <tt>$Revision: 1.1.2.4 $</tt>
 * @author  <a href="mailto:steve@resolvesw.com">Steve Coy</a>
 * @author  <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 */
public interface LOB extends EJBObject
{
   /**
    * Returns the primary key
    * @return Integer
    */
   public Integer getId() throws RemoteException;

   /**
    * Sets the primary key.
    * @param id
    */
   public void setId(Integer id) throws RemoteException;

   /**
    * Returns the large string attribute.
    * @return String
    */
   public String getBigString() throws RemoteException;

   /**
    * Sets the value of the large string attribute.
    * The idea here is to store it in a CLOB object in associated database table
    * so that we can check the container's LOB functionality properly.
    * @param s
    */
   public void setBigString(String s) throws RemoteException;

   /**
    * Returns the content of the large binary object.
    * @return byte[]
    */
   public byte[] getBinaryData() throws RemoteException;

   /**
    * Sets the content of the large binary object.
    * The idea here is to store it in a BLOB objects in the associated database
    * table so that we check the container's LOB functionality properly.
    * @param data
    */
   public void setBinaryData(byte[] data) throws RemoteException;

   Map getMapField() throws RemoteException;
   void setMapField(Map map) throws RemoteException;

   List getListField() throws RemoteException;
   void setListField(List list) throws RemoteException;

   Set getSetField() throws RemoteException;
   void setSetField(Set set) throws RemoteException;

   ValueHolder getValueHolder() throws RemoteException;
   void setValueHolder(ValueHolder valueHolder) throws RemoteException;

   ValueHolder getCleanGetValueHolder() throws RemoteException;
   void setCleanGetValueHolder(ValueHolder valueHolder) throws RemoteException;

   ValueHolder getStateFactoryValueHolder() throws RemoteException;
   void setStateFactoryValueHolder(ValueHolder valueHolder) throws RemoteException;
}
