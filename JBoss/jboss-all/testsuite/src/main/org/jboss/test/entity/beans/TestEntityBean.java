/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.entity.beans;

import java.sql.Connection;
import java.sql.Statement;

import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.EntityBean;

import javax.naming.InitialContext;

import javax.sql.DataSource;

import org.jboss.test.entity.interfaces.TestEntityValue;

/**
 * An entity.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.2.2.2 $
 *
 * @ejb:bean 
 *      name="TestEntity"
 *      type="CMP"
 *      view-type="both"
 *      jndi-name="test/entity/TestEntity"
 *      local-jndi-name="test/entity/TestEntityLocal"
 *      schema="test"
 *      primkey-field="entityID"
 * @ejb:pk 
 *      class="java.lang.String"
 * @ejb:transaction
 *      type="Required"
 * @ejb:value-object
 *
 * @jboss:container-configuration
 *        name="TestEntity Container Configuration"
 * @jboss:table-name
 *        table-name="test_entity_testentity"
 * @jboss:create-table
 *        create="true"
 * @jboss:remove-table
 *        remove="true"
 */
public abstract class TestEntityBean
   implements EntityBean
{
   /**
    * @ejb:create-method
    */
   public String ejbCreate(TestEntityValue value)
     throws CreateException
   {
      setEntityID(value.getEntityID());
      setValue1(value.getValue1());
      return null;
   }

   public void ejbPostCreate(TestEntityValue value)
      throws CreateException
   {
   }

   /**
    * @ejb:interface-method
    * @ejb:persistent-field
    */
   public abstract String getEntityID();
   public abstract void setEntityID(String entityID);

   /**
    * @ejb:interface-method
    * @ejb:persistent-field
    * @jboss:method-attributes read-only="true"
    */
   public abstract String getValue1();

   /**
    * @ejb:interface-method
    */
   public abstract void setValue1(String value1);

   /**
    * @ejb:home-method
    */
   public void ejbHomeRemoveExternal(String entityID)
   {
      Connection connection = null;
      Statement statement = null;
      try
      {
         DataSource dataSource = (DataSource) new InitialContext().lookup("java:/DefaultDS");
         connection = dataSource.getConnection();
         statement = connection.createStatement();
         int rows = statement.executeUpdate("delete from test_entity_testentity " 
                                            + "where entityID = '" + entityID + "'");
         if (rows != 1)
            throw new Exception("Wrong number of rows deleted: " + rows);
      }
      catch (Exception e)
      {
         throw new EJBException(e);
      }
      finally
      {
         try
         {
            if (statement != null)
               statement.close();
            if (connection != null)
               connection.close();
         }
         catch (Exception e)
         {
            throw new EJBException(e);
         }
      }
   }

   /**
    * @ejb:home-method
    */
   public void ejbHomeChangeValue1(String entityID, String value1)
   {
      Connection connection = null;
      Statement statement = null;
      try
      {
         DataSource dataSource = (DataSource) new InitialContext().lookup("java:/DefaultDS");
         connection = dataSource.getConnection();
         statement = connection.createStatement();
         int rows = statement.executeUpdate("update test_entity_testentity set value1 = '" + value1 +  
                                            "' where entityID = '" + entityID + "'");
         if (rows != 1)
            throw new Exception("Wrong number of rows updated: " + rows);
      }
      catch (Exception e)
      {
         throw new EJBException(e);
      }
      finally
      {
         try
         {
            if (statement != null)
               statement.close();
            if (connection != null)
               connection.close();
         }
         catch (Exception e)
         {
            throw new EJBException(e);
         }
      }
   }
}
