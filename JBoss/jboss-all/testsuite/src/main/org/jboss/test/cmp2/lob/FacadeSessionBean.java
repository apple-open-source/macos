/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.lob;


import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.naming.InitialContext;
import javax.rmi.PortableRemoteObject;
import java.util.Set;
import java.util.Map;
import java.util.List;

/**
 *
 * @author  <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 */
public class FacadeSessionBean
   implements SessionBean
{
   private LOBHome lobHome;

   // Business methods

   public void createLOB(Integer id) throws Exception
   {
      getLOBHome().create(id);
   }

   public void removeLOB(Integer id) throws Exception
   {
      getLOBHome().remove(id);
   }

   public void addMapEntry(Integer id, Object key, Object value) throws Exception
   {
      getLOBHome().findByPrimaryKey(id).getMapField().put(key, value);
   }

   public Map getMapField(Integer id) throws Exception
   {
      return getLOBHome().findByPrimaryKey(id).getMapField();
   }

   public void addSetElement(Integer id, Object value) throws Exception
   {
      getLOBHome().findByPrimaryKey(id).getSetField().add(value);
   }

   public Set getSetField(Integer id) throws Exception
   {
      return getLOBHome().findByPrimaryKey(id).getSetField();
   }

   public void addListElement(Integer id, Object value) throws Exception
   {
      getLOBHome().findByPrimaryKey(id).getListField().add(value);
   }

   public List getListField(Integer id) throws Exception
   {
      return getLOBHome().findByPrimaryKey(id).getListField();
   }

   public void setBinaryData(Integer id, byte[] value) throws Exception
   {
      getLOBHome().findByPrimaryKey(id).setBinaryData(value);
   }

   public void setBinaryDataElement(Integer id, int index, byte value)
      throws Exception
   {
      getLOBHome().findByPrimaryKey(id).getBinaryData()[index] = value;
   }

   public byte getBinaryDataElement(Integer id, int index)
      throws Exception
   {
      return getLOBHome().findByPrimaryKey(id).getBinaryData()[index];
   }

   public void setValueHolderValue(Integer id, String value)
      throws Exception
   {
      getLOBHome().findByPrimaryKey(id).getValueHolder().setValue(value);
   }

   public String getValueHolderValue(Integer id)
      throws Exception
   {
      return getLOBHome().findByPrimaryKey(id).getValueHolder().getValue();
   }

   public void setCleanGetValueHolderValue(Integer id, String value)
      throws Exception
   {
      getLOBHome().findByPrimaryKey(id).setCleanGetValueHolder(new ValueHolder(value));
   }

   public void modifyCleanGetValueHolderValue(Integer id, String value)
      throws Exception
   {
      getLOBHome().findByPrimaryKey(id).getCleanGetValueHolder().setValue(value);
   }

   public String getCleanGetValueHolderValue(Integer id)
      throws Exception
   {
      return getLOBHome().findByPrimaryKey(id).getCleanGetValueHolder().getValue();
   }

   public String getStateFactoryValueHolderValue(Integer id)
      throws Exception
   {
      return getLOBHome().findByPrimaryKey(id).getStateFactoryValueHolder().getValue();
   }

   public void modifyStateFactoryValueHolderValue(Integer id, String value)
      throws Exception
   {
      getLOBHome().findByPrimaryKey(id).getStateFactoryValueHolder().setValue(value);
   }

   public void setStateFactoryValueHolderValue(Integer id, String value)
      throws Exception
   {
      ValueHolder holder = getLOBHome().findByPrimaryKey(id).getStateFactoryValueHolder();
      holder.setValue(value);
      holder.setDirty(true);
   }

   // SessionBean implementation

   /**
    * @exception  CreateException Description of Exception
    * @ejb.create-method
    */
   public void ejbCreate() throws CreateException
   {
   }

   public void ejbActivate()
   {
   }

   public void ejbPassivate()
   {
   }

   public void ejbRemove()
   {
   }

   public void setSessionContext(SessionContext ctx)
   {
   }

   // Private

   private LOBHome getLOBHome()
   {
      if(lobHome == null)
      {
         try
         {
            InitialContext initialContext = new InitialContext();
            Object home = initialContext.lookup(LOBHome.LOB_HOME_CONTEXT);
            lobHome = (LOBHome)PortableRemoteObject.narrow(home, LOBHome.class);
         }
         catch(Exception e)
         {
            throw new EJBException("Could not lookup " + LOBHome.LOB_HOME_CONTEXT);
         }
      }
      return lobHome;
   }
}
