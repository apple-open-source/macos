/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.lob;

import java.rmi.RemoteException;
import java.util.Map;
import java.util.Set;
import java.util.List;

/**
 *
 * @author  <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 */
public interface Facade
   extends javax.ejb.EJBObject
{
   public void createLOB(Integer id)
      throws Exception, RemoteException;
   public void removeLOB(Integer id)
      throws Exception, RemoteException;
   public void addMapEntry(Integer id, Object key, Object value)
      throws Exception, RemoteException;
   public Map getMapField(Integer id)
      throws Exception, RemoteException;
   public void addSetElement(Integer id, Object value)
      throws Exception, RemoteException;
   public Set getSetField(Integer id)
      throws Exception, RemoteException;
   public void addListElement(Integer id, Object value)
      throws Exception, RemoteException;
   public List getListField(Integer id)
      throws Exception, RemoteException;
   public void setBinaryData(Integer id, byte[] value)
      throws Exception, RemoteException;
   public void setBinaryDataElement(Integer id, int index, byte value)
      throws Exception, RemoteException;
   public byte getBinaryDataElement(Integer id, int index)
      throws Exception, RemoteException;
   public void setValueHolderValue(Integer id, String value)
      throws Exception, RemoteException;
   public String getValueHolderValue(Integer id)
      throws Exception, RemoteException;
   public void setCleanGetValueHolderValue(Integer id, String value)
      throws Exception, RemoteException;
   public void modifyCleanGetValueHolderValue(Integer id, String value)
      throws Exception, RemoteException;
   public String getCleanGetValueHolderValue(Integer id)
      throws Exception, RemoteException;
   public String getStateFactoryValueHolderValue(Integer id)
      throws Exception, RemoteException;
   public void setStateFactoryValueHolderValue(Integer id, String value)
      throws Exception, RemoteException;
   public void modifyStateFactoryValueHolderValue(Integer id, String value)
      throws Exception, RemoteException;
}
