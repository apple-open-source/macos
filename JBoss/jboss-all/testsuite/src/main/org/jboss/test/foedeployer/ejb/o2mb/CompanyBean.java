/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.test.foedeployer.ejb.o2mb;

import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.FinderException;
import javax.ejb.NoSuchEntityException;
import javax.ejb.ObjectNotFoundException;
import javax.ejb.RemoveException;
import javax.ejb.CreateException;
import javax.ejb.DuplicateKeyException;
import javax.ejb.EJBException;

import javax.sql.DataSource;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import java.sql.Connection;
import java.sql.Statement;
import java.sql.ResultSet;
import java.sql.SQLException;

import java.util.Set;

import org.apache.log4j.Category;

/**
 * Models a company.
 *
 * @ejb.bean
 *    name="Company"
 *    generate="true"
 *    view-type="local"
 *    type="CMP"
 *    local-jndi-name="CompanyEJB.CompanyHome"
 *    reentrant="False"
 *    cmp-version="2.x"
 *    primkey-field="name"
 *
 * @ejb.pk
 *    class="java.lang.String"
 *    generate="false"
 *
 * @@ejb.finder signature="Collection findAll()"
 *
 * @ejb.transaction type="Required"
 *
 * @@ejb.persistence table-name="company"
 * @weblogic:table-name company
 *
 * @jboss.create-table "${jboss.create.table}"
 * @jboss.remove-table "${jboss.remove.table}"
 *
 * @author <a href="mailto:loubyansky@hotmail.com">Alex Loubyansky</a>
 */
public abstract class CompanyBean
   implements EntityBean
{
   // Constants -----------------------------------------------------
   static Category log = Category.getInstance( CompanyBean.class );

   // Attributes ----------------------------------------------------
   private EntityContext ctx;

   // CMP
   /**
    * Company's name: primary key field
    *
    * @ejb.pk-field
    * @ejb.persistent-field
    * @ejb.interface-method
    *
    * xdoclet needs to be updated
    * @@ejb.persistence
    *    column-name="name"
    *    jdbc-type="VARCHAR"
    *    sql-type="VARCHAR(50)"
    *
    * @weblogic:dbms-column name
    */
   public abstract String getName();
   public abstract void setName(String name);

   
   // CMR

   /**
    * Employees: bidirectional CMR
    *
    * @ejb.relation
    *    name="Company-Employee"
    *    role-name="Company-Has-Employees"
    * @ejb.interface-method
    *
    * @weblogic.column-map
    *    foreign-key-column="company_name"
    *    key-column="name"
    */
   public abstract Set getEmployees();
   /**
    * @ejb.interface-method
    */
   public abstract void setEmployees(Set employees);

   // EntityBean Implementation -------------------------------------
   /**
    * @ejb.create-method
    */
   public String ejbCreate( String name )
      throws CreateException
   {
      setName(name);
      return null; // See 9.4.2 of the EJB 1.1 specification
   }

   public void ejbPostCreate( String name ) { }

   public void ejbRemove()
      throws RemoveException
   {
      log.debug( "removed: " + ctx.getPrimaryKey() );
   }
   
   public void setEntityContext(EntityContext ctx)
   {
      this.ctx = ctx;
   }
   
   public void unsetEntityContext()
   {
      ctx = null;
   }
   
   public void ejbActivate() { }
   public void ejbPassivate() { }
   public void ejbLoad() { }   
   public void ejbStore() { }
}
