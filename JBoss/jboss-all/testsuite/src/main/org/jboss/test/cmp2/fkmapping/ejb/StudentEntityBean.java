/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.fkmapping.ejb;

import javax.ejb.EntityContext;
import javax.ejb.EntityBean;
import javax.ejb.EJBException;
import javax.ejb.RemoveException;
import javax.ejb.CreateException;
import java.rmi.RemoteException;

/**
 * @ejb.bean
 *    name="Student"
 *    type="CMP"
 *    cmp-version="2.x"
 *    view-type="local"
 *    reentrant="false"
 *    local-jndi-name="Student"
 * @ejb.pk generate="true"
 * @ejb.util generate="physical"
 * @ejb.persistence table-name="STUDENT"
 * @jboss.persistence
 *    create-table="true"
 *    remove-table="true"
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public abstract class StudentEntityBean
   implements EntityBean
{
   // Attributes ---------------------------------------------------
   private EntityContext ctx;

   // CMP accessors
   /**
    * @ejb.pk-field
    * @ejb.persistent-field
    * @ejb.interface-method
    * @ejb.persistence column-name="DEPT_CODE"
    */
   public abstract String getDepartmentCode();
   public abstract void setDepartmentCode(String deptCode);

   /**
    * @ejb.pk-field
    * @ejb.persistent-field
    * @ejb.interface-method
    * @ejb.persistence column-name="DEPT_CODE2"
    */
   public abstract String getDepartmentCode2();
   public abstract void setDepartmentCode2(String deptCode);

   /**
    * @ejb.pk-field
    * @ejb.persistent-field
    * @ejb.interface-method
    * @ejb.persistence column-name="LAST_NAME"
    */
   public abstract String getLastName();
   public abstract void setLastName(String lastName);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    * @ejb.persistence column-name="DESCR"
    */
   public abstract String getDescription();
   public abstract void setDescription(String description);

   // CMR accessors
   /**
    * @ejb.interface-method
    * @ejb.relation
    *    name="Department-Student-CompleteFKToPK"
    *    role-name="Student-has-Department"
    * @jboss.relation
    *    fk-column="DEPT_CODE"
    *    related-pk-field="departmentCode"
    * @jboss.relation
    *    fk-column="DEPT_CODE2"
    *    related-pk-field="departmentCode2"
    */
   public abstract DepartmentLocal getDepartment();
   /**
    * @ejb.interface-method
    */
   public abstract void setDepartment(DepartmentLocal department);

   /**
    * @ejb.interface-method
    * @ejb.relation
    *    name="Group-Student-PartialFKToPK"
    *    role-name="Student-has-Group"
    * @jboss.relation
    *    fk-column="DEPT_CODE"
    *    related-pk-field="departmentCode"
    * @jboss.relation
    *    fk-column="DEPT_CODE2"
    *    related-pk-field="departmentCode2"
    * @jboss.relation
    *    fk-column="GROUP_NUM_FK"
    *    related-pk-field="groupNumber"
    */
    public abstract GroupLocal getGroup();
   /**
    * @ejb.interface-method
    */
   public abstract void setGroup(GroupLocal group);

   // EntityBean implementation ------------------------------------
   /**
    * @ejb.create-method
    */
   public StudentPK ejbCreate(String deptCode, String lastName, String descr)
      throws CreateException
   {
      setDepartmentCode(deptCode);
      setDepartmentCode2("X"+deptCode);
      setLastName(lastName);
      setDescription(descr);
      return null;
   }

   public void ejbPostCreate(String deptCode, String lastName, String descr) {}

   public void ejbActivate() throws EJBException, RemoteException {}
   public void ejbLoad() throws EJBException, RemoteException {}
   public void ejbPassivate() throws EJBException, RemoteException {}
   public void ejbRemove() throws RemoveException, EJBException, RemoteException {}
   public void ejbStore() throws EJBException, RemoteException {}
   public void setEntityContext(EntityContext ctx) throws EJBException, RemoteException
   {
      this.ctx = ctx;
   }
   public void unsetEntityContext() throws EJBException, RemoteException
   {
      this.ctx = null;
   }
}
