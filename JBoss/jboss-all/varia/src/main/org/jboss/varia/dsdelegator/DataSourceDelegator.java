/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.varia.dsdelegator;

import org.jboss.logging.Logger;
import org.jboss.naming.NonSerializableFactory;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.system.ServiceMBean;

import javax.naming.NamingException;
import javax.naming.InitialContext;
import javax.naming.Name;
import javax.sql.DataSource;
import java.io.PrintWriter;
import java.sql.Connection;
import java.sql.SQLException;

/**
 * DataSource delegator service. The goal is to dynamically change the datasource, for example,
 * for entity beans at runtime.
 * The service implements javax.sql.DataSource interface and is bound in the JNDI.
 * The target datasource can be changed with setTargetName(String jndiName) passing in
 * the JNDI name of the target datasource.
 *
 * @jmx:mbean
 *    name="jboss.varia:name=DataSourceDelegator"
 *    extends="org.jboss.system.ServiceMBean"
 *
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 */
public class DataSourceDelegator
   extends ServiceMBeanSupport
   implements DataSource, DataSourceDelegatorMBean
{
   private static final Logger log = Logger.getLogger(DataSourceDelegator.class);

   /** JNDI name the service will be bound under */
   private String bindName;
   /** target DataSource JNDI name */
   private String targetName;
   /** target DataSource */
   private DataSource target;

   /**
    * @jmx.managed-operation
    * @param bindName  the name under which the service will be bound in JNDI.
    * @throws NamingException
    */
   public void setBindName(String bindName)
      throws NamingException
   {
      this.bindName = bindName;
      if(getState() == ServiceMBean.STARTED)
         bind();
   }

   /**
    * @jmx.managed-operation
    * @return the name under which the service is bound in JNDI.
    */
   public String getBindName()
   {
      return bindName;
   }

   /**
    * @jmx.managed-operation
    * @return JNDI name of the target datasource.
    */
   public String getTargetName()
   {
      return targetName;
   }

   /**
    * @jmx.managed-operation
    * @param targetName  the JNDI name of target DataSource.
    * @throws NamingException
    */
   public void setTargetName(String targetName)
      throws NamingException
   {
      this.targetName = targetName;
      if(getState() == ServiceMBean.STARTED)
         updateTarget();
   }

   public void startService()
      throws Exception
   {
      updateTarget();
      bind();
      log.debug("started");
   }

   public void stopService()
      throws Exception
   {
      unbind();
      log.debug("stopped");
   }

   // DataSource implementation

   public Connection getConnection()
      throws SQLException
   {
      return target.getConnection();
   }

   public Connection getConnection(String user, String password)
      throws SQLException
   {
      return target.getConnection(user, password);
   }

   public PrintWriter getLogWriter()
      throws SQLException
   {
      return target.getLogWriter();
   }

   public int getLoginTimeout()
      throws SQLException
   {
      return target.getLoginTimeout();
   }

   public void setLogWriter(PrintWriter printWriter)
      throws SQLException
   {
      target.setLogWriter(printWriter);
   }

   public void setLoginTimeout(int seconds)
      throws SQLException
   {
      target.setLoginTimeout(seconds);
   }

   // Private

   private void bind()
      throws NamingException
   {
      Name name = new InitialContext().getNameParser("").parse(bindName);
      NonSerializableFactory.rebind(name, this, true);
      log.debug("bound to JNDI name " + bindName);
   }

   private void unbind()
      throws NamingException
   {
      new InitialContext().unbind(bindName);
      NonSerializableFactory.unbind(bindName);
   }

   private void updateTarget()
      throws NamingException
   {
      target = (DataSource)new InitialContext().lookup(targetName);
      log.debug("target updated to " + targetName);
   }
}
