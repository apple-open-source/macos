/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.fkmapping.ejb;

import org.apache.log4j.Category;

import javax.ejb.SessionBean;
import javax.ejb.EJBException;
import javax.ejb.SessionContext;
import javax.ejb.CreateException;
import javax.ejb.RemoveException;
import javax.ejb.EJBLocalObject;
import javax.naming.NamingException;
import java.rmi.RemoteException;


/**
 * @ejb.bean
 *    type="Stateless"
 *    name="Manager"
 *    view-type="remote"
 * @ejb.util generate="physical"
 * @ejb.transaction type="Required"
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public class ManagerSessionBean
   implements SessionBean
{
   // Attributes ------------------------------------------------
   static Category log = Category.getInstance(ManagerSessionBean.class);
   private InstituteLocalHome instituteHome;
   private DepartmentLocalHome departmentHome;
   private GroupLocalHome groupHome;
   private StudentLocalHome studentHome;
   private ExamenationLocalHome examHome;

   // Scenarious
   /**
    * @ejb.interface-method
    */
   public void testStandaloneFKMapping()
   {
      InstituteLocal institute = null;
      DepartmentLocal department = null;
      try
      {
         institute = getInstituteHome().create("NTUUKPI", "Natinal Technical University Of The Ukraine KPI");
         department = getDepartmentHome().create("KV", "Specialized Computer Systems");
         institute.getDepartments().add(department);
         assertTrue("department.getInstitute().isIdentical(institute)", department.getInstitute().isIdentical(institute));
         department.setInstitute(null);
         assertTrue("institute.getDepartments().isEmpty()", institute.getDepartments().isEmpty());
      }
      catch(EJBException ejbe)
      {
         throw ejbe;
      }
      catch(Exception e)
      {
         throw new EJBException(e);
      }
      finally
      {
         if(institute != null)
            removeEntity(institute);
         if(department != null)
            removeEntity(department);
      }
   }

   /**
    * @ejb.interface-method
    */
   public void testCompleteFKToPKMapping()
   {
      DepartmentLocal department = null;
      GroupLocal kv62Group = null;
      try
      {
         // one-side instance created before many-side instance
         department = getDepartmentHome().create("KV", "Specialized Computer Systems");
         assertTrue("department.getGroups().isEmpty()", department.getGroups().isEmpty());

         kv62Group = getGroupHome().create("KV", 62, "KV-62");
         assertTrue("department.getGroups().contains(kv62Group)", department.getGroups().contains(kv62Group));
         assertTrue("kv62Group.getDepartment().isIdentical(department)", kv62Group.getDepartment().isIdentical(department));

         kv62Group.remove();
         assertTrue("department.getGroups().isEmpty()", department.getGroups().isEmpty());

         // many-side instance created before one-side instance
         department.remove();
         kv62Group = getGroupHome().create("KV", 62, "KV-62");
         assertTrue("kv62Group.getDepartment() == null", kv62Group.getDepartment() == null);

         department = getDepartmentHome().create("KV", "Specialized Computer Systems");
         assertTrue("kv62Group.getDepartment().isIdentical(department)", kv62Group.getDepartment().isIdentical(department));
         assertTrue("department.getGroups().contains(kv62Group)", department.getGroups().contains(kv62Group));

         department.remove();
         department = null;
         assertTrue("kv62Group.getDepartment() == null", kv62Group.getDepartment() == null);
      }
      catch(EJBException ejbe)
      {
         throw ejbe;
      }
      catch(Exception e)
      {
         throw new EJBException(e);
      }
      finally
      {
         if(department != null)
            removeEntity(department);
         if(kv62Group != null)
            removeEntity(kv62Group);
      }
   }

   /**
    * @ejb.interface-method
    */
   public void testPartialFKToPKMapping()
   {
      StudentLocal petrovStudent = null;
      StudentLocal sidorovStudent = null;
      GroupLocal group = null;
      try
      {
         petrovStudent = getStudentHome().create("KV", "Petrov", "Petrov works on KV department.");
         group = getGroupHome().create("KV", 62, "KV-62");
         assertTrue("petrovStudent.getGroup() == null", petrovStudent.getGroup() == null);

         petrovStudent.setGroup(group);
         assertTrue("group.isIdentical(petrovStudent.getGroup())", group.isIdentical(petrovStudent.getGroup()));
         assertTrue("group.getStudents().contains(petrovStudent)", group.getStudents().contains(petrovStudent));

         sidorovStudent = getStudentHome().create("KV", "Sidorov", "Sidorov works on KV department.");
         group.getStudents().add(sidorovStudent);
         assertTrue("sidorovStudent.getGroup().isIdentical(group)", sidorovStudent.getGroup().isIdentical(group));
         assertTrue("group.getStudents().contains(petrovStudent)", group.getStudents().contains(petrovStudent));
         assertTrue("group.getStudents().contains(sidorovStudent)", group.getStudents().contains(sidorovStudent));

         group.remove();
         group = null;
         assertTrue("petrovStudent.getGroup() == null", petrovStudent.getGroup() == null);
         assertTrue("sidorovStudent.getGroup() == null", sidorovStudent.getGroup() == null);

         /*
         group = getGroupHome().create("KV", 62, "KV-62");
         assertTrue("group.getStudents().contains(petrovStudent)", group.getStudents().contains(petrovStudent));
         assertTrue("group.isIdentical(petrovStudent.getGroup())", group.isIdentical(petrovStudent.getGroup()));
         */
      }
      catch(EJBException ejbe)
      {
         throw ejbe;
      }
      catch(Exception e)
      {
         throw new EJBException(e);
      }
      finally
      {
         if(petrovStudent != null)
            removeEntity(petrovStudent);
         if(sidorovStudent != null)
            removeEntity(sidorovStudent);
         if(group != null)
            removeEntity(group);
      }
   }

   /**
    * @ejb.interface-method
    */
   public void testFKToCMPMapping()
   {
      GroupLocal kv61Group = null;
      GroupLocal kv62Group = null;
      ExamenationLocal exam = null;
      try
      {
         kv62Group = getGroupHome().create("KV", 62, "KV-62");
         exam = getExamHome().create("kv61-1", "Math", "KV", 62);
         assertTrue("kv62Group.isIdentical(exam.getGroup())", kv62Group.isIdentical(exam.getGroup()));
         assertTrue("kv62Group.getExamenations().contains(exam)", kv62Group.getExamenations().contains(exam));

         kv61Group = getGroupHome().create("KV", 61, "KV-61");
         exam.setGroup(kv61Group);
         assertTrue("expected: exam.getGroupNumber() == 61;"
            + " got: exam.getGroupNumber() == " + exam.getGroupNumber(),
            exam.getGroupNumber() == 61);

         exam.setGroupNumber(62);
         assertTrue("kv62Group.isIdentical(exam.getGroup())", kv62Group.isIdentical(exam.getGroup()));
         assertTrue("kv62Group.getExamenations().contains(exam);", kv62Group.getExamenations().contains(exam));
         assertTrue("kv61Group.getExamenations().isEmpty();", kv61Group.getExamenations().isEmpty());

         exam.setDepartmentCode("KM");
         assertTrue("exam.getGroup() == null", exam.getGroup() == null);
         assertTrue("kv62Group.getExamenations().isEmpty();", kv62Group.getExamenations().isEmpty());

         exam.setDepartmentCode("KV");
         assertTrue("kv62Group.isIdentical(exam.getGroup())", kv62Group.isIdentical(exam.getGroup()));
         assertTrue("kv62Group.getExamenations().contains(exam);", kv62Group.getExamenations().contains(exam));
      }
      catch(EJBException ejbe)
      {
         throw ejbe;
      }
      catch(Exception e)
      {
         throw new EJBException(e);
      }
      finally
      {
         if(exam != null)
            removeEntity(exam);
         if(kv61Group != null)
            removeEntity(kv61Group);
         if(kv62Group != null)
            removeEntity(kv62Group);
      }
   }

   /**
    * @ejb.interface-method
    * @ejb.transaction type="RequiresNew"
    */
   public void createParent(Long id, String firstName)
      throws Exception
   {
      ParentUtil.getLocalHome().create(id, firstName);
   }

   /**
    * @ejb.interface-method
    * @ejb.transaction type="RequiresNew"
    */
   public void createChild(long id, String firstName)
      throws Exception
   {
      ChildUtil.getLocalHome().create(id, firstName);
   }

   /**
    * @ejb.interface-method
    * @ejb.transaction type="RequiresNew"
    */
   public void createChild(long id, String firstName, Long parentId, String parentName)
      throws Exception
   {
      ChildUtil.getLocalHome().create(id, firstName, parentId, parentName);
   }

   /**
    * @ejb.interface-method
    * @ejb.transaction type="RequiresNew"
    */
   public void assertChildHasMother(long childId, Long parentId, String parentName)
      throws Exception
   {
      ChildLocal child = ChildUtil.getLocalHome().findByPrimaryKey(new ChildPK(childId));
      ParentLocal parent = child.getMother();
      if(parent == null)
         throw new EJBException("No parent assigned to child: expected parentId=" + parentId);
      ParentPK parentPK = new ParentPK(parentId, parentName);
      if(!parent.getPrimaryKey().equals(parentPK))
         throw new EJBException("Wrong parent: expected parentPK=" + parentPK
            + ", got " + parent.getPrimaryKey());
   }

   /**
    * @ejb.interface-method
    * @ejb.transaction type="RequiresNew"
    */
   public void assertParentHasNoChild(Long parentId, String parentName)
      throws Exception
   {
      ParentLocal parent = ParentUtil.getLocalHome()
         .findByPrimaryKey(new ParentPK(parentId, parentName));
      if(parent.getChild() != null)
         throw new EJBException("parent.getChild() != null");
   }

   /**
    * @ejb.interface-method
    * @ejb.transaction type="RequiresNew"
    */
   public Object createChildUPKWithMother() throws Exception
   {
      ChildUPKLocal child = ChildUPKUtil.getLocalHome().create("Avoka");
      ParentLocal mother = ParentUtil.getLocalHome().create(new Long(11), "Irene");
      child.setMother(mother);
      return child.getPrimaryKey();
   }
   /**
    * @ejb.interface-method
    * @ejb.transaction type="RequiresNew"
    */
   public void loadChildUPKWithMother(Object pk) throws Exception
   {
      // find
      ChildUPKLocal child = ChildUPKUtil.getLocalHome().findByPrimaryKey(pk);
      // load child and check its mother
      assertTrue("child.getMother().getFirstName() is Irene",
         "Irene".equals(child.getMother().getFirstName()));
   }

   /**
    * @ejb.interface-method
    * @ejb.transaction type="RequiresNew"
    */
   public Object createChildUPKWithFather() throws Exception
   {
      ChildUPKLocal child = ChildUPKUtil.getLocalHome().create("Avoka");
      ParentLocal father = ParentUtil.getLocalHome().create(new Long(12), "Gregory");
      child.setFather(father);
      return child.getPrimaryKey();
   }

   /**
    * @ejb.interface-method
    * @ejb.transaction type="RequiresNew"
    */
   public void loadChildUPKWithFather(Object pk) throws Exception
   {
      log.debug("loadChildUPK");
      // find
      ChildUPKLocal child = ChildUPKUtil.getLocalHome().findByPrimaryKey(pk);
      // load child and check its mother
      assertTrue("child.getFather().getFirstName() is Gregory",
         "Gregory".equals(child.getFather().getFirstName()));
   }

   // Private ---------------------------------------------------
   private void assertTrue(String message, boolean expression)
   {
      if(!expression)
         throw new AssertionException(message);
   }

   private void removeEntity(EJBLocalObject localEntity)
   {
      try
      {
         localEntity.remove();
      }
      catch(RemoveException re)
      {
         throw new EJBException("Couldn't remove local entity " + localEntity.getPrimaryKey());
      }
   }

   private StudentLocalHome getStudentHome()
      throws NamingException
   {
      if(studentHome == null)
         studentHome = StudentUtil.getLocalHome();
      return studentHome;
   }

   private ExamenationLocalHome getExamHome()
      throws NamingException
   {
      if(examHome == null)
         examHome = ExamenationUtil.getLocalHome();
      return examHome;
   }

   private InstituteLocalHome getInstituteHome()
      throws NamingException
   {
      if(instituteHome == null)
         instituteHome = InstituteUtil.getLocalHome();
      return instituteHome;
   }

   private DepartmentLocalHome getDepartmentHome()
      throws NamingException
   {
      if(departmentHome == null)
         departmentHome = DepartmentUtil.getLocalHome();
      return departmentHome;
   }

   private GroupLocalHome getGroupHome()
      throws NamingException
   {
      if(groupHome == null)
         groupHome = GroupUtil.getLocalHome();
      return groupHome;
   }

   // SessionBean implementation --------------------------------
   /**
    * @ejb.create-method
    */
   public void ejbCreate() throws CreateException {}
   public void ejbActivate() throws EJBException, RemoteException {}
   public void ejbPassivate() throws EJBException, RemoteException {}
   public void ejbRemove() throws EJBException, RemoteException {}
   public void setSessionContext(SessionContext sessionContext) throws EJBException, RemoteException {}
}
