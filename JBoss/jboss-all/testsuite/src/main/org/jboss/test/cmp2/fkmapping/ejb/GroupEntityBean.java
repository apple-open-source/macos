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
import java.util.Collection;

/**
 * @ejb.bean
 *    name="Group"
 *    type="CMP"
 *    cmp-version="2.x"
 *    view-type="local"
 *    reentrant="false"
 *    local-jndi-name="Group"
 * @ejb.pk generate="true"
 * @ejb.util generate="physical"
 * @ejb.persistence table-name="DEPT_GROUP"
 * @ejb.finder signature="java.util.Collection findAll()"
 *             query="SELECT OBJECT(g) FROM Group g"
 * @jboss.persistence
 *    create-table="true"
 *    remove-table="true"
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public abstract class GroupEntityBean
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
    * @ejb.persistence column-name="GROUP_NUM"
    */
   public abstract long getGroupNumber();
   public abstract void setGroupNumber(long groupNum);

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
    *    name="Department-Group-CompleteFKToPK"
    *    role-name="Group-has-Department"
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
    *    role-name="Group-has-Students"
    */
   public abstract Collection getStudents();
   /**
    * @ejb.interface-method
    */
   public abstract void setStudents(Collection students);

   /**
    * @ejb.interface-method
    * @ejb.relation
    *    name="Group-Exam-FKToCMP"
    *    role-name="Group-has-Exams"
    */
   public abstract Collection getExamenations();
   /**
    * @ejb.interface-method
    */
   public abstract void setExamenations(Collection students);

   // EntityBean implementation ------------------------------------
   /**
    * @ejb.create-method
    */
   public GroupPK ejbCreate(String deptCode, long groupNum, String descr)
      throws CreateException
   {
      setDepartmentCode(deptCode);
      setDepartmentCode2("X"+deptCode);
      setGroupNumber(groupNum);
      setDescription(descr);
      return null;
   }

   public void ejbPostCreate(String deptCode, long groupNum, String descr) {}

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
