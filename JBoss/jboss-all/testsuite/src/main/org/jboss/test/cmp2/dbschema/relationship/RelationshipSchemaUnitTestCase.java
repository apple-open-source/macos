/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.dbschema.relationship;

import junit.framework.Test;
import org.jboss.test.JBossTestCase;
import org.jboss.test.cmp2.dbschema.util.DBSchemaHelper;
import org.jboss.test.cmp2.dbschema.util.AbstractDBSchemaTestCase;
import org.jboss.test.cmp2.dbschema.util.Column;
import org.jboss.test.cmp2.dbschema.util.Table;

import java.sql.Connection;
import java.sql.DatabaseMetaData;
import java.sql.Types;


/**
 * The tests for generated database schema for entity beans from cmp2/relationship.
 * Each test method is named by the pattern: test${ejb-relationship-name}
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public class RelationshipSchemaUnitTestCase
   extends AbstractDBSchemaTestCase
{
   public static Test suite() throws Exception
   {
      return JBossTestCase.getDeploySetup(RelationshipSchemaUnitTestCase.class, "cmp2-dbschema.jar");
   }

   public RelationshipSchemaUnitTestCase(String s)
   {
      super(s);
   }

   public void testAB_OneToOne_Bi_Table() throws Exception
   {
      assertTableMapping(
         "A_OneToOne_Bi_Table_EJB".toUpperCase(),
         "B_OneToOne_Bi_Table_EJB".toUpperCase(),
         "AB_OneToOneBi".toUpperCase()
      );
   }

   public void testAB_OneToOne_Bi_FK() throws Exception
   {
      final String aTableName = "A_OneToOne_Bi_FK_EJB".toUpperCase();
      final String bTableName = "B_OneToOne_Bi_FK_EJB".toUpperCase();
      final String aFKName = "A";
      final String bFKName = "B";

      Connection con = null;
      try
      {
         con = getConnection();
         DatabaseMetaData dbMD = con.getMetaData();

         Table A = DBSchemaHelper.getTable(dbMD, aTableName);
         assertEquals(2, A.getColumnsNumber());
         Column aId = A.getColumn("ID");
         aId.assertTypeNotNull(Types.INTEGER, true);
         Column aB = A.getColumn(bFKName);
         aB.assertTypeNotNull(Types.INTEGER, false);

         Table B = DBSchemaHelper.getTable(dbMD, bTableName);
         assertEquals(2, B.getColumnsNumber());
         Column bId = B.getColumn("ID");
         bId.assertTypeNotNull(Types.INTEGER, true);
         Column bA = B.getColumn(aFKName);
         bA.assertTypeNotNull(Types.INTEGER, false);
      }
      finally
      {
         DBSchemaHelper.safeClose(con);
      }
   }

   public void testAB_OneToOne_Uni_Table() throws Exception
   {
      assertTableMapping(
         "A_OneToOne_Uni_Table_EJB".toUpperCase(),
         "B_OneToOne_Uni_Table_EJB".toUpperCase(),
         "AB_OneToOneUni".toUpperCase()
      );
   }

   public void testAB_OneToOne_Uni_FK() throws Exception
   {
      assertFKMapping(
         "B_OneToOne_Uni_FK_EJB".toUpperCase(),
         "A_OneToOne_Uni_FK_EJB".toUpperCase(),
         "B".toUpperCase()
      );
   }

   public void testAB_OneToMany_Bi_Table() throws Exception
   {
      assertTableMapping(
         "A_OneToMany_Bi_Table_EJB".toUpperCase(),
         "B_OneToMany_Bi_Table_EJB".toUpperCase(),
         "AB_OneToManyBi".toUpperCase()
      );
   }

   public void testAB_OneToMany_Bi_FK() throws Exception
   {
      assertFKMapping(
         "A_OneToMany_Bi_FK_EJB".toUpperCase(),
         "B_OneToMany_Bi_FK_EJB".toUpperCase(),
         "A"
      );
   }

   public void testAB_OneToMany_Uni_Table() throws Exception
   {
      assertTableMapping(
         "A_OneToMany_Uni_Table_EJB".toUpperCase(),
         "B_OneToMany_Uni_Table_EJB".toUpperCase(),
         "AB_OneToManyUni".toUpperCase()
      );
   }

   public void testAB_OneToMany_Uni_FK() throws Exception
   {
      assertFKMapping(
         "A_OneToMany_Uni_FK_EJB".toUpperCase(),
         "B_OneToMany_Uni_FK_EJB".toUpperCase(),
         "A_OneToMany_Uni_FK_EJB_b".toUpperCase()
      );
   }

   public void testAB_ManyToOne_Uni_Table() throws Exception
   {
      assertTableMapping(
         "A_ManyToOne_Uni_Table_EJB".toUpperCase(),
         "B_ManyToOne_Uni_Table_EJB".toUpperCase(),
         "AB_ManyToOneUni".toUpperCase()
      );
   }

   public void testAB_ManyToOne_Uni_FK() throws Exception
   {
      assertFKMapping(
         "A_ManyToOne_Uni_FK_EJB".toUpperCase(),
         "B_ManyToOne_Uni_FK_EJB".toUpperCase(),
         "A"
      );
   }

   public void testAB_ManyToMany_Bi() throws Exception
   {
      assertTableMapping(
         "A_ManyToMany_Bi_EJB".toUpperCase(),
         "B_ManyToMany_Bi_EJB".toUpperCase(),
         "AB_ManyToManyBi".toUpperCase()
      );
   }

   public void testAB_ManyToMany_Uni() throws Exception
   {
      assertTableMapping(
         "A_ManyToMany_Uni_EJB".toUpperCase(),
         "B_ManyToMany_Uni_EJB".toUpperCase(),
         "AB_ManyToManyUni".toUpperCase()
      );
   }

   // Private

   /**
    * Tests default schema generation for relationships with relation table
    */
   private void assertTableMapping(String aTableName, String bTableName, String abTableName)
      throws Exception
   {
      Connection con = null;
      try
      {
         con = getConnection();
         DatabaseMetaData dbMD = con.getMetaData();

         Table A = DBSchemaHelper.getTable(dbMD, aTableName);
         assertEquals(1, A.getColumnsNumber());
         Column aId = A.getColumn("ID");
         aId.assertTypeNotNull(Types.INTEGER, true);

         Table B = DBSchemaHelper.getTable(dbMD, bTableName);
         assertEquals(1, B.getColumnsNumber());
         Column bId = B.getColumn("ID");
         bId.assertTypeNotNull(Types.INTEGER, true);

         Table AB = DBSchemaHelper.getTable(dbMD, abTableName);
         assertEquals(AB.getColumnsNumber(), 2);
         Column aFk = AB.getColumn(aTableName);
         aFk.assertTypeNotNull(Types.INTEGER, true);
         Column bFk = AB.getColumn(bTableName);
         bFk.assertTypeNotNull(Types.INTEGER, true);
      }
      finally
      {
         DBSchemaHelper.safeClose(con);
      }
   }

   /**
    * Tests default schema generation for relationships with foreign key mapping
    */
   private void assertFKMapping(final String aTableName, final String bTableName, final String aFKName)
      throws Exception
   {
      Connection con = null;
      try
      {
         con = getConnection();
         DatabaseMetaData dbMD = con.getMetaData();

         Table A = DBSchemaHelper.getTable(dbMD, aTableName);
         assertEquals(1, A.getColumnsNumber());
         Column aId = A.getColumn("ID");
         aId.assertTypeNotNull(Types.INTEGER, true);

         Table B = DBSchemaHelper.getTable(dbMD, bTableName);
         assertEquals(2, B.getColumnsNumber());
         Column bId = B.getColumn("ID");
         bId.assertTypeNotNull(Types.INTEGER, true);
         Column bA = B.getColumn(aFKName);
         bA.assertTypeNotNull(Types.INTEGER, false);
      }
      finally
      {
         DBSchemaHelper.safeClose(con);
      }
   }
}
