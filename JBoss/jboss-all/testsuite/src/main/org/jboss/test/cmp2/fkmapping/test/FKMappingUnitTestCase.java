/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.fkmapping.test;

import javax.ejb.EJBException;
import javax.ejb.EJBLocalObject;
import javax.ejb.RemoveException;
import javax.naming.NamingException;
import javax.naming.InitialContext;
import javax.transaction.UserTransaction;

import junit.framework.Test;
import net.sourceforge.junitejb.EJBTestCase;
import org.jboss.test.JBossTestCase;
import org.jboss.test.cmp2.fkmapping.ejb.DepartmentLocal;
import org.jboss.test.cmp2.fkmapping.ejb.DepartmentLocalHome;
import org.jboss.test.cmp2.fkmapping.ejb.DepartmentUtil;
import org.jboss.test.cmp2.fkmapping.ejb.ExamenationLocal;
import org.jboss.test.cmp2.fkmapping.ejb.ExamenationLocalHome;
import org.jboss.test.cmp2.fkmapping.ejb.ExamenationUtil;
import org.jboss.test.cmp2.fkmapping.ejb.GroupLocal;
import org.jboss.test.cmp2.fkmapping.ejb.GroupLocalHome;
import org.jboss.test.cmp2.fkmapping.ejb.GroupUtil;
import org.jboss.test.cmp2.fkmapping.ejb.InstituteLocal;
import org.jboss.test.cmp2.fkmapping.ejb.InstituteLocalHome;
import org.jboss.test.cmp2.fkmapping.ejb.InstituteUtil;
import org.jboss.test.cmp2.fkmapping.ejb.StudentLocal;
import org.jboss.test.cmp2.fkmapping.ejb.StudentLocalHome;
import org.jboss.test.cmp2.fkmapping.ejb.StudentUtil;
import org.jboss.test.cmp2.fkmapping.ejb.ManagerUtil;
import org.jboss.test.cmp2.fkmapping.ejb.Manager;
import org.jboss.test.cmp2.fkmapping.ejb.ChildUPKUtil;

/**
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public class FKMappingUnitTestCase extends EJBTestCase
{
   private InstituteLocalHome instituteHome;
   private DepartmentLocalHome departmentHome;
   private GroupLocalHome groupHome;
   private StudentLocalHome studentHome;
   private ExamenationLocalHome examHome;

   // Suite ---------------------------------------------------
   public static Test suite() throws Exception
   {
      return JBossTestCase.getDeploySetup(FKMappingUnitTestCase.class, "cmp2-fkmapping.jar");
   }

   // Constructor ---------------------------------------------
   public FKMappingUnitTestCase(String name)
   {
      super(name);
   }

   // Tests ---------------------------------------------------
   public void testStandaloneFKMapping() throws Exception
   {
      InstituteLocal institute = null;
      DepartmentLocal department = null;
      try {
         institute = getInstituteHome().create("NTUUKPI", "Natinal Technical University Of The Ukraine KPI");
         department = getDepartmentHome().create("KV", "Specialized Computer Systems");
         institute.getDepartments().add(department);
         assertTrue("department.getInstitute().isIdentical(institute)", department.getInstitute().isIdentical(institute));
         department.setInstitute(null);
         assertTrue("institute.getDepartments().isEmpty()", institute.getDepartments().isEmpty());
      } finally {
         removeEntity(institute);
         removeEntity(department);
      }
   }

   public void testJoing() throws Exception {
      DepartmentLocal department = null;
      GroupLocal kv62Group = null;
      GroupLocal kv63Group = null;
      try {
         System.out.println("testJoin()");
         department = getDepartmentHome().create("KV", "Specialized Computer Systems");
         kv62Group = getGroupHome().create("KV", 62, "KV-62");
         kv63Group = getGroupHome().create("KV", 63, "KV-63");
         commit();
         System.out.println("testJoin() 2");
         getGroupHome().findAll();
         kv62Group.getDepartment();
         System.out.println("testJoin() 3");
      } finally {
         removeEntity(kv63Group);
         removeEntity(kv62Group);
         removeEntity(department);
      }
   }

   public void testCompleteFKToPKMapping()
         throws Exception
   {
      DepartmentLocal department = null;
      GroupLocal kv62Group = null;
      boolean exceptionThrown = false;
      try {
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
      catch (Exception ex)
      {
         exceptionThrown = true;
         throw ex;
      }
      finally {
         if (!exceptionThrown)
         {
            removeEntity(department);
            removeEntity(kv62Group);
         }
      }
   }

   public void testPartialFKToPKMapping()
         throws Exception
   {
      StudentLocal petrovStudent = null;
      StudentLocal sidorovStudent = null;
      GroupLocal group = null;
      try {
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
      } finally {
         removeEntity(petrovStudent);
         removeEntity(sidorovStudent);
         removeEntity(group);
      }
   }

   public void testFKToCMPMapping()
         throws Exception
   {
      GroupLocal kv61Group = null;
      GroupLocal kv62Group = null;
      ExamenationLocal exam = null;
      try {
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
         exam.setDepartmentCode2("XKM");
         assertTrue("exam.getGroup() == null", exam.getGroup() == null);
         assertTrue("kv62Group.getExamenations().isEmpty();", kv62Group.getExamenations().isEmpty());

         exam.setDepartmentCode("KV");
         exam.setDepartmentCode2("XKV");
         assertTrue("kv62Group.isIdentical(exam.getGroup())", kv62Group.isIdentical(exam.getGroup()));
         assertTrue("kv62Group.getExamenations().contains(exam);", kv62Group.getExamenations().contains(exam));
      } finally {
         removeEntity(exam);
         removeEntity(kv61Group);
         removeEntity(kv62Group);
      }
   }

   public void testInsertAfterEjbPostCreate()
      throws Exception
   {
      String avoka = "Avoka";
      String irene = "Irene";
      Manager manager = ManagerUtil.getHome().create();
      Long long1 = new Long(1);
      manager.createParent(long1, irene);

      try
      {
         manager.createChild(1, avoka);
         fail("Should have filed as the foreign key field can't be null.");
      }
      catch(Exception expected){}

      manager.createChild(1, avoka, long1, irene);
      manager.createChild(2, "Ataka", long1, irene);

      manager.assertChildHasMother(1, long1, irene);
      manager.assertChildHasMother(2, long1, irene);
   }

   public void testGeneratedPKWithInsertAfterPostCreate()
      throws Exception
   {
      // this will fail unless pk value is generated and set in ejbCreate
      ChildUPKUtil.getLocalHome().create(null);
   }

   /**
    * Tests complete foreign key load.
    * The bug was that, when null value was loaded for a foreign key field,
    * consequent reading of foreign key fields for current CMR was stopped and incorrect
    * result index was returned. Further loaded fields contained messed up values.
    * NOTE: to reproduce the bug, foreign key must be a composite key. When loading, foreign key
    * must be null and after loading results for foreign key there should be results for other fields
    * loaded from the same SQL SELECT.
    */
   public void testCompleteFKReadOnLoad() throws Exception
   {
      Manager manager = ManagerUtil.getHome().create();

      // one of the following will fail (with NPE) if there is the bug
      Object childPK = manager.createChildUPKWithMother();
      manager.loadChildUPKWithMother(childPK);

      childPK = manager.createChildUPKWithFather();
      manager.loadChildUPKWithFather(childPK);
   }

   /**
    * In case when related entity has custom primary key type which consists
    * of primitive fields, foreign key fields are initialized to default primitive values,
    * thus, creating a fake foreign key value.
    */
   public void testInitFKToNull() throws Exception
   {
      Manager manager = ManagerUtil.getHome().create();
      Long parentId = new Long(1);
      String name = "Avoka";
      manager.createParent(parentId, name);
      manager.assertParentHasNoChild(parentId, name);
   }

   // Private

   private StudentLocalHome getStudentHome()
         throws NamingException
   {
      if (studentHome == null)
         studentHome = StudentUtil.getLocalHome();
      return studentHome;
   }

   private ExamenationLocalHome getExamHome()
         throws NamingException
   {
      if (examHome == null)
         examHome = ExamenationUtil.getLocalHome();
      return examHome;
   }

   private InstituteLocalHome getInstituteHome()
         throws NamingException
   {
      if (instituteHome == null)
         instituteHome = InstituteUtil.getLocalHome();
      return instituteHome;
   }

   private DepartmentLocalHome getDepartmentHome()
         throws NamingException
   {
      if (departmentHome == null)
         departmentHome = DepartmentUtil.getLocalHome();
      return departmentHome;
   }

   private GroupLocalHome getGroupHome()
         throws NamingException
   {
      if (groupHome == null)
         groupHome = GroupUtil.getLocalHome();
      return groupHome;
   }

   private void removeEntity(EJBLocalObject localEntity)
   {
      if (localEntity != null) {
         try {
            localEntity.remove();
         } catch (RemoveException re) {
            throw new EJBException("Couldn't remove local entity " + localEntity.getPrimaryKey());
         }
      }
   }

   private void commit() throws Exception {
      UserTransaction tx = (UserTransaction) new InitialContext().lookup("java:comp/UserTransaction");
      tx.commit();
      tx.begin();
   }
}
