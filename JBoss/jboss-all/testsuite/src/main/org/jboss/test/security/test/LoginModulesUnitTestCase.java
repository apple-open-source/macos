/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.security.test;

import java.lang.reflect.Method;
import java.io.Serializable;
import java.security.MessageDigest;
import java.security.acl.Group;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Set;
import javax.naming.InitialContext;
import javax.security.auth.Subject;
import javax.security.auth.login.AppConfigurationEntry;
import javax.security.auth.login.Configuration;
import javax.security.auth.login.LoginContext;
import javax.security.auth.login.LoginException;
import javax.sql.DataSource;
import javax.resource.spi.security.PasswordCredential;
import javax.management.MBeanServerFactory;
import javax.management.MBeanServer;

import junit.framework.TestSuite;

import org.apache.log4j.Logger;

import org.jboss.logging.XLevel;
import org.jboss.security.SimpleGroup;
import org.jboss.security.SimplePrincipal;
import org.jboss.security.Util;
import org.jboss.security.SecurityAssociation;
import org.jboss.security.auth.callback.UsernamePasswordHandler;
import org.jboss.security.auth.spi.UsernamePasswordLoginModule;
import org.jboss.test.JBossTestCase;

/** Tests of the LoginModule classes.

 @author Scott.Stark@jboss.org
 @version $Revision: 1.2.2.7 $
 */
public class LoginModulesUnitTestCase extends JBossTestCase
{

   /** Hard coded login configurations for the test cases. The configuration
    name corresponds to the unit test function that uses the configuration.
    */
   static class TestConfig extends Configuration
   {
      public void refresh()
      {
      }

      public AppConfigurationEntry[] getAppConfigurationEntry(String name)
      {
         AppConfigurationEntry[] entry = null;
         try
         {
            Class[] parameterTypes = {};
            Method m = getClass().getDeclaredMethod(name, parameterTypes);
            Object[] args = {};
            entry = (AppConfigurationEntry[]) m.invoke(this, args);
         }
         catch(Exception e)
         {
         }
         return entry;
      }

      AppConfigurationEntry[] testIdentity()
      {
         String name = "org.jboss.security.auth.spi.IdentityLoginModule";
         HashMap options = new HashMap();
         options.put("principal", "stark");
         options.put("roles", "Role3,Role4");
         AppConfigurationEntry ace = new AppConfigurationEntry(name,
         AppConfigurationEntry.LoginModuleControlFlag.REQUIRED, options);
         AppConfigurationEntry[] entry = {ace};
         return entry;
      }
      AppConfigurationEntry[] testJdbc()
      {
         String name = "org.jboss.security.auth.spi.DatabaseServerLoginModule";
         HashMap options = new HashMap();
         options.put("dsJndiName", "testJdbc");
         options.put("principalsQuery", "select Password from Principals where PrincipalID=?");
         options.put("rolesQuery", "select Role, RoleGroup from Roles where PrincipalID=?");
         AppConfigurationEntry ace = new AppConfigurationEntry(name,
         AppConfigurationEntry.LoginModuleControlFlag.REQUIRED, options);
         AppConfigurationEntry[] entry = {ace};
         return entry;
      }
      AppConfigurationEntry[] testSimple()
      {
         String name = "org.jboss.security.auth.spi.SimpleServerLoginModule";
         AppConfigurationEntry ace = new AppConfigurationEntry(name,
         AppConfigurationEntry.LoginModuleControlFlag.REQUIRED, new HashMap());
         AppConfigurationEntry[] entry = {ace};
         return entry;
      }
      AppConfigurationEntry[] testUsernamePassword()
      {
         return other();
      }
      AppConfigurationEntry[] testUsernamePasswordHash()
      {
         HashMap options = new HashMap();
         options.put("hashAlgorithm", "MD5");
         options.put("hashEncoding", "base64");
         AppConfigurationEntry ace = new AppConfigurationEntry(HashTestLoginModule.class.getName(),
            AppConfigurationEntry.LoginModuleControlFlag.REQUIRED, options);
         AppConfigurationEntry[] entry = {ace};
         return entry;
      }
      AppConfigurationEntry[] testAnon()
      {
         String name = "org.jboss.security.auth.spi.AnonLoginModule";
         HashMap options = new HashMap();
         options.put("unauthenticatedIdentity", "nobody");
         AppConfigurationEntry ace = new AppConfigurationEntry(name,
            AppConfigurationEntry.LoginModuleControlFlag.REQUIRED, options);
         AppConfigurationEntry[] entry = {ace};
         return entry;
      }
      AppConfigurationEntry[] testNull()
      {
         String name = "org.jboss.security.auth.spi.AnonLoginModule";
         HashMap options = new HashMap();
         AppConfigurationEntry ace = new AppConfigurationEntry(name,
         AppConfigurationEntry.LoginModuleControlFlag.REQUIRED, options);
         AppConfigurationEntry[] entry = {ace};
         return entry;
      }
      AppConfigurationEntry[] testUsersRoles()
      {
         String name = "org.jboss.security.auth.spi.UsersRolesLoginModule";
         AppConfigurationEntry ace = new AppConfigurationEntry(name,
         AppConfigurationEntry.LoginModuleControlFlag.REQUIRED, new HashMap());
         AppConfigurationEntry[] entry = {ace};
         return entry;
      }
      AppConfigurationEntry[] testUsersRolesHash()
      {
         String name = "org.jboss.security.auth.spi.UsersRolesLoginModule";
         HashMap options = new HashMap();
         options.put("usersProperties", "usersb64.properties");
         options.put("hashAlgorithm", "MD5");
         options.put("hashEncoding", "base64");
         AppConfigurationEntry ace = new AppConfigurationEntry(name,
         AppConfigurationEntry.LoginModuleControlFlag.REQUIRED, options);
         AppConfigurationEntry[] entry = {ace};
         return entry;
      }
      AppConfigurationEntry[] testAnonUsersRoles()
      {
         String name = "org.jboss.security.auth.spi.UsersRolesLoginModule";
         HashMap options = new HashMap();
         options.put("unauthenticatedIdentity", "nobody");
         AppConfigurationEntry ace = new AppConfigurationEntry(name,
         AppConfigurationEntry.LoginModuleControlFlag.REQUIRED, options);
         AppConfigurationEntry[] entry = {ace};
         return entry;
      }
      AppConfigurationEntry[] testControlFlags()
      {
         String name1 = "org.jboss.security.auth.spi.UsersRolesLoginModule";
         AppConfigurationEntry ace1 = new AppConfigurationEntry(name1,
            AppConfigurationEntry.LoginModuleControlFlag.SUFFICIENT, new HashMap());

         String name2 = "org.jboss.security.auth.spi.DatabaseServerLoginModule";
         HashMap options = new HashMap();
         options.put("dsJndiName", "testJdbc");
         options.put("principalsQuery", "select Password from Principals where PrincipalID=?");
         options.put("rolesQuery", "select Role, RoleGroup from Roles where PrincipalID=?");
         AppConfigurationEntry ace2 = new AppConfigurationEntry(name2,
            AppConfigurationEntry.LoginModuleControlFlag.SUFFICIENT, options);

         AppConfigurationEntry[] entry = {ace1, ace2};
         return entry;
      }
      AppConfigurationEntry[] testJCACallerIdentity()
      {
         String name = "org.jboss.resource.security.CallerIdentityLoginModule";
         HashMap options = new HashMap();
         options.put("userName", "jduke");
         options.put("password", "theduke");
         options.put("managedConnectionFactoryName", "jboss:name=fakeMCF");
         options.put("ignoreMissigingMCF", Boolean.TRUE);
         AppConfigurationEntry ace = new AppConfigurationEntry(name,
            AppConfigurationEntry.LoginModuleControlFlag.REQUIRED, options);
         AppConfigurationEntry[] entry = {ace};
         return entry;
      }
      AppConfigurationEntry[] other()
      {
         AppConfigurationEntry ace = new AppConfigurationEntry(TestLoginModule.class.getName(),
         AppConfigurationEntry.LoginModuleControlFlag.REQUIRED, new HashMap());
         AppConfigurationEntry[] entry = {ace};
         return entry;
      }
   }

   public static class TestLoginModule extends UsernamePasswordLoginModule
   {
      protected Group[] getRoleSets()
      {
         SimpleGroup roles = new SimpleGroup("Roles");
         Group[] roleSets = {roles};
         roles.addMember(new SimplePrincipal("TestRole"));
         roles.addMember(new SimplePrincipal("Role2"));
         return roleSets;
      }
      /** This represents the 'true' password
       */
      protected String getUsersPassword()
      {
         return "secret";
      }
   }
   public static class HashTestLoginModule extends TestLoginModule
   {
      /** This represents the 'true' password in its hashed form
       */
      protected String getUsersPassword()
      {
         MessageDigest md = null;
         try
         {
            md = MessageDigest.getInstance("MD5");
         }
         catch(Exception e)
         {
            e.printStackTrace();
         }
         byte[] passwordBytes = "secret".getBytes();
         byte[] hash = md.digest(passwordBytes);
         String passwordHash = Util.encodeBase64(hash);
         return passwordHash;
      }
   }

   /** A pseudo DataSource that is used to provide Hypersonic db
    connections to the DatabaseServerLoginModule.
    */
   static class TestDS implements DataSource, Serializable
   {
      public java.sql.Connection getConnection() throws java.sql.SQLException
      {
         return getConnection("sa", "");
      }
      public java.sql.Connection getConnection(String user, String pass) throws java.sql.SQLException
      {
			java.sql.Connection con = null;
			String jdbcURL = "";
			try
         {
         	jdbcURL = "jdbc:hsqldb:hsql://localhost:1701";
         	con = DriverManager.getConnection(jdbcURL, user, pass);
			}
         catch(java.sql.SQLException sqle)
         {
				jdbcURL = "jdbc:hsqldb:hsql:.";
         	con = DriverManager.getConnection(jdbcURL, user, pass);
			}
         return con;
      }
      public java.io.PrintWriter getLogWriter() throws java.sql.SQLException
      {
         return null;
      }
      public void setLogWriter(java.io.PrintWriter out)
         throws java.sql.SQLException
      {
      }
      public int getLoginTimeout() throws java.sql.SQLException
      {
         return 0;
      }
      public void setLoginTimeout(int seconds) throws java.sql.SQLException
      {
      }
   }

   public LoginModulesUnitTestCase(String testName)
   {
      super(testName);
   }

   protected void setUp() throws Exception
   {
      // Install the custom JAAS configuration
      Configuration.setConfiguration(new TestConfig());

      // Turn on trace level logging
      Logger root = Logger.getRootLogger();
      root.setLevel(XLevel.TRACE);
   }

   public void testUsernamePassword() throws Exception
   {
      getLog().info("testUsernamePassword");
      UsernamePasswordHandler handler = new UsernamePasswordHandler("scott", "secret".toCharArray());
      LoginContext lc = new LoginContext("testUsernamePassword", handler);
      lc.login();
      Subject subject = lc.getSubject();
      Set groups = subject.getPrincipals(Group.class);
      assertTrue("Principals contains scott", subject.getPrincipals().contains(new SimplePrincipal("scott")));
      assertTrue("Principals contains Roles", groups.contains(new SimplePrincipal("Roles")));
      Group roles = (Group) groups.iterator().next();
      assertTrue("TestRole is a role", roles.isMember(new SimplePrincipal("TestRole")));
      assertTrue("Role2 is a role", roles.isMember(new SimplePrincipal("Role2")));

      lc.logout();
   }
   public void testUsernamePasswordHash() throws Exception
   {
      getLog().info("testUsernamePasswordHash");
      UsernamePasswordHandler handler = new UsernamePasswordHandler("scott", "secret".toCharArray());
      LoginContext lc = new LoginContext("testUsernamePasswordHash", handler);
      lc.login();
      Subject subject = lc.getSubject();
      Set groups = subject.getPrincipals(Group.class);
      assertTrue("Principals contains scott", subject.getPrincipals().contains(new SimplePrincipal("scott")));
      assertTrue("Principals contains Roles", groups.contains(new SimplePrincipal("Roles")));
      Group roles = (Group) groups.iterator().next();
      assertTrue("TestRole is a role", roles.isMember(new SimplePrincipal("TestRole")));
      assertTrue("Role2 is a role", roles.isMember(new SimplePrincipal("Role2")));

      lc.logout();
   }

   public void testUsersRoles() throws Exception
   {
      getLog().info("testUsersRoles");
      UsernamePasswordHandler handler = new UsernamePasswordHandler("scott", "echoman".toCharArray());
      LoginContext lc = new LoginContext("testUsersRoles", handler);
      lc.login();
      Subject subject = lc.getSubject();
      Set groups = subject.getPrincipals(Group.class);
      assertTrue("Principals contains scott", subject.getPrincipals().contains(new SimplePrincipal("scott")));
      assertTrue("Principals contains Roles", groups.contains(new SimplePrincipal("Roles")));
      assertTrue("Principals contains CallerPrincipal", groups.contains(new SimplePrincipal("CallerPrincipal")));
      Group roles = (Group) groups.iterator().next();
      Iterator groupsIter = groups.iterator();
      while( groupsIter.hasNext() )
      {
         roles = (Group) groupsIter.next();
         if( roles.getName().equals("Roles") )
         {
            assertTrue("Echo is a role", roles.isMember(new SimplePrincipal("Echo")));
            assertTrue("Java is NOT a role", roles.isMember(new SimplePrincipal("Java")) == false);
            assertTrue("Coder is NOT a role", roles.isMember(new SimplePrincipal("Coder")) == false);
         }
         else if( roles.getName().equals("CallerPrincipal") )
         {
            getLog().info("CallerPrincipal is "+roles.members().nextElement());
            boolean isMember = roles.isMember(new SimplePrincipal("callerScott"));
            assertTrue("CallerPrincipal is callerScott", isMember);
         }
      }
      lc.logout();

      handler = new UsernamePasswordHandler("stark", "javaman".toCharArray());
      lc = new LoginContext("testUsersRoles", handler);
      lc.login();
      subject = lc.getSubject();
      groups = subject.getPrincipals(Group.class);
      assertTrue("Principals contains stark", subject.getPrincipals().contains(new SimplePrincipal("stark")));
      assertTrue("Principals contains Roles", groups.contains(new SimplePrincipal("Roles")));
      assertTrue("Principals contains CallerPrincipal", groups.contains(new SimplePrincipal("CallerPrincipal")));
      groupsIter = groups.iterator();
      while( groupsIter.hasNext() )
      {
         roles = (Group) groupsIter.next();
         if( roles.getName().equals("Roles") )
         {
            assertTrue("Echo is NOT a role", roles.isMember(new SimplePrincipal("Echo")) == false);
            assertTrue("Java is a role", roles.isMember(new SimplePrincipal("Java")));
            assertTrue("Coder is a role", roles.isMember(new SimplePrincipal("Coder")));
         }
         else if( roles.getName().equals("CallerPrincipal") )
         {
            getLog().info("CallerPrincipal is "+roles.members().nextElement());
            boolean isMember = roles.isMember(new SimplePrincipal("callerStark"));
            assertTrue("CallerPrincipal is callerStark", isMember);
         }
      }
      lc.logout();

      // Test the usernames with common prefix
      getLog().info("Testing similar usernames");
      handler = new UsernamePasswordHandler("jdukeman", "anotherduke".toCharArray());
      lc = new LoginContext("testUsersRoles", handler);
      lc.login();
      subject = lc.getSubject();
      groups = subject.getPrincipals(Group.class);
      assertTrue("Principals contains jdukeman", subject.getPrincipals().contains(new SimplePrincipal("jdukeman")));
      assertTrue("Principals contains Roles", groups.contains(new SimplePrincipal("Roles")));
      assertTrue("Principals contains CallerPrincipal", groups.contains(new SimplePrincipal("CallerPrincipal")));
      groupsIter = groups.iterator();
      while( groupsIter.hasNext() )
      {
         roles = (Group) groupsIter.next();
         if( roles.getName().equals("Roles") )
         {
            assertTrue("Role1 is NOT a role", roles.isMember(new SimplePrincipal("Role1")) == false);
            assertTrue("Role2 is a role", roles.isMember(new SimplePrincipal("Role2")));
            assertTrue("Role3 is a role", roles.isMember(new SimplePrincipal("Role3")));
         }
         else if( roles.getName().equals("CallerPrincipal") )
         {
            getLog().info("CallerPrincipal is "+roles.members().nextElement());
            boolean isMember = roles.isMember(new SimplePrincipal("callerJdukeman"));
            assertTrue("CallerPrincipal is callerJdukeman", isMember);
         }
      }
      lc.logout();
   }

   public void testUsersRolesHash() throws Exception
   {
      getLog().info("testUsersRolesHash");
      UsernamePasswordHandler handler = new UsernamePasswordHandler("scott", "echoman".toCharArray());
      LoginContext lc = new LoginContext("testUsersRolesHash", handler);
      lc.login();
      Subject subject = lc.getSubject();
      Set groups = subject.getPrincipals(Group.class);
      assertTrue("Principals contains scott", subject.getPrincipals().contains(new SimplePrincipal("scott")));
      assertTrue("Principals contains Roles", groups.contains(new SimplePrincipal("Roles")));
      assertTrue("Principals contains CallerPrincipal", groups.contains(new SimplePrincipal("CallerPrincipal")));
      Group roles = (Group) groups.iterator().next();
      Iterator groupsIter = groups.iterator();
      while( groupsIter.hasNext() )
      {
         roles = (Group) groupsIter.next();
         if( roles.getName().equals("Roles") )
         {
            assertTrue("Echo is a role", roles.isMember(new SimplePrincipal("Echo")));
            assertTrue("Java is NOT a role", roles.isMember(new SimplePrincipal("Java")) == false);
            assertTrue("Coder is NOT a role", roles.isMember(new SimplePrincipal("Coder")) == false);
         }
         else if( roles.getName().equals("CallerPrincipal") )
         {
            getLog().info("CallerPrincipal is "+roles.members().nextElement());
            boolean isMember = roles.isMember(new SimplePrincipal("callerScott"));
            assertTrue("CallerPrincipal is callerScott", isMember);
         }
      }
      lc.logout();
   }

   public void testAnonUsersRoles() throws Exception
   {
      getLog().info("testAnonUsersRoles");
      UsernamePasswordHandler handler = new UsernamePasswordHandler(null, null);
      LoginContext lc = new LoginContext("testAnonUsersRoles", handler);
      lc.login();
      Subject subject = lc.getSubject();
      Set groups = subject.getPrincipals(Group.class);
      assertTrue("Principals contains nobody", subject.getPrincipals().contains(new SimplePrincipal("nobody")));
      assertTrue("Principals contains Roles", groups.contains(new SimplePrincipal("Roles")));
      Group roles = (Group) groups.iterator().next();
      assertTrue("Roles has no members", roles.members().hasMoreElements() == false);

      lc.logout();
   }
   public void testAnon() throws Exception
   {
      getLog().info("testAnon");
      UsernamePasswordHandler handler = new UsernamePasswordHandler(null, null);
      LoginContext lc = new LoginContext("testAnon", handler);
      lc.login();
      Subject subject = lc.getSubject();
      Set groups = subject.getPrincipals(Group.class);
      assertTrue("Principals contains nobody", subject.getPrincipals().contains(new SimplePrincipal("nobody")));
      assertTrue("Principals contains Roles", groups.contains(new SimplePrincipal("Roles")));
      Group roles = (Group) groups.iterator().next();
      assertTrue("Roles has no members", roles.members().hasMoreElements() == false);

      lc.logout();
   }
   public void testNull() throws Exception
   {
      getLog().info("testNull");
      UsernamePasswordHandler handler = new UsernamePasswordHandler(null, null);
      LoginContext lc = new LoginContext("testNull", handler);
      try
      {
         lc.login();
         fail("Should not be able to login as null, null");
      }
      catch(LoginException e)
      {
         // Ok
      }
   }

   public void testIdentity() throws Exception
   {
      getLog().info("testIdentity");
      LoginContext lc = new LoginContext("testIdentity");
      lc.login();
      Subject subject = lc.getSubject();
      Set groups = subject.getPrincipals(Group.class);
      assertTrue("Principals contains stark", subject.getPrincipals().contains(new SimplePrincipal("stark")));
      assertTrue("Principals contains Roles", groups.contains(new SimplePrincipal("Roles")));
      Group roles = (Group) groups.iterator().next();
      assertTrue("Role2 is not a role", roles.isMember(new SimplePrincipal("Role2")) == false);
      assertTrue("Role3 is a role", roles.isMember(new SimplePrincipal("Role3")));
      assertTrue("Role4 is a role", roles.isMember(new SimplePrincipal("Role4")));

      lc.logout();
   }
   public void testJCACallerIdentity() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer("jboss");
      getLog().info("testJCACallerIdentity");
      LoginContext lc = new LoginContext("testJCACallerIdentity");
      lc.login();
      Subject subject = lc.getSubject();
      assertTrue("Principals contains jduke", subject.getPrincipals().contains(new SimplePrincipal("jduke")));
      Set creds = subject.getPrivateCredentials(PasswordCredential.class);
      PasswordCredential pc = (PasswordCredential) creds.iterator().next();
      String username = pc.getUserName();
      String password = new String(pc.getPassword());
      assertTrue("PasswordCredential.username = jduke", username.equals("jduke"));
      assertTrue("PasswordCredential.password = theduke", password.equals("theduke"));
      lc.logout();

      // Test the override of the default identity
      SecurityAssociation.setPrincipal(new SimplePrincipal("jduke2"));
      SecurityAssociation.setCredential("theduke2".toCharArray());
      lc.login();
      subject = lc.getSubject();
      Set principals = subject.getPrincipals();
      assertTrue("Principals contains jduke2", principals.contains(new SimplePrincipal("jduke2")));
      assertTrue("Principals does not contains jduke", principals.contains(new SimplePrincipal("jduke")) == false);
      creds = subject.getPrivateCredentials(PasswordCredential.class);
      pc = (PasswordCredential) creds.iterator().next();
      username = pc.getUserName();
      password = new String(pc.getPassword());
      assertTrue("PasswordCredential.username = jduke2", username.equals("jduke2"));
      assertTrue("PasswordCredential.password = theduke2", password.equals("theduke2"));
      lc.logout();
   }

   public void testSimple() throws Exception
   {
      getLog().info("testSimple");
      UsernamePasswordHandler handler = new UsernamePasswordHandler("jduke", "jduke".toCharArray());
      LoginContext lc = new LoginContext("testSimple", handler);
      lc.login();
      Subject subject = lc.getSubject();
      Set groups = subject.getPrincipals(Group.class);
      assertTrue("Principals contains jduke", subject.getPrincipals().contains(new SimplePrincipal("jduke")));
      assertTrue("Principals contains Roles", groups.contains(new SimplePrincipal("Roles")));
      Group roles = (Group) groups.iterator().next();
      assertTrue("user is a role", roles.isMember(new SimplePrincipal("user")));
      assertTrue("guest is a role", roles.isMember(new SimplePrincipal("guest")));

      lc.logout();
   }

   /** Use this DDL script to setup tables:
    ; First load the JDBC driver and open a database.
    d org.enhydra.instantdb.jdbc.idbDriver;
    o jdbc:idb=/usr/local/src/cvsroot/jBoss/jboss/dist/conf/default/instantdb.properties;

    ; Create the Principal table
    e DROP TABLE Principals ;
    e CREATE TABLE Principals (
    PrincipalID	VARCHAR(64) PRIMARY KEY,
    Password	VARCHAR(64) );

    ; put some initial data in the table
    e INSERT INTO Principals VALUES ("scott", "echoman");
    e INSERT INTO Principals VALUES ("stark", "javaman");

    ; Create the Roles table
    e DROP TABLE Roles;
    e CREATE TABLE Roles (
    PrincipalID	VARCHAR(64) PRIMARY KEY,
    Role	VARCHAR(64),
    RoleGroup VARCHAR(64) );

    ; put some initial data in the table
    e INSERT INTO Roles VALUES ("scott", "Echo", "");
    e INSERT INTO Roles VALUES ("scott", "caller_scott", "CallerPrincipal");
    e INSERT INTO Roles VALUES ("stark", "Java", "");
    e INSERT INTO Roles VALUES ("stark", "Coder", "");
    e INSERT INTO Roles VALUES ("stark", "caller_stark", "CallerPrincipal");

    c close;
    */
   public void testJdbc() throws Exception
   {
      getLog().info("testJdbc");
      setupLoginTables();

      UsernamePasswordHandler handler = new UsernamePasswordHandler("stark", "javaman".toCharArray());
      LoginContext lc = new LoginContext("testJdbc", handler);
      lc.login();
      Subject subject = lc.getSubject();
      Set groups = subject.getPrincipals(Group.class);
      assertTrue("Principals contains stark", subject.getPrincipals().contains(new SimplePrincipal("stark")));
      assertTrue("Principals contains Roles", groups.contains(new SimplePrincipal("Roles")));
      Group roles = (Group) groups.iterator().next();
      assertTrue("Java is a role", roles.isMember(new SimplePrincipal("Java")));
      assertTrue("Coder is a role", roles.isMember(new SimplePrincipal("Coder")));

      lc.logout();
   }

   public void testControlFlags() throws Exception
   {
      getLog().info("testControlFlags");
      Configuration cfg = Configuration.getConfiguration();
      AppConfigurationEntry[] ace = cfg.getAppConfigurationEntry("testControlFlags");
      for(int n = 0; n < ace.length; n ++)
      {
         assertTrue("testControlFlags flag==SUFFICIENT",
            ace[n].getControlFlag() == AppConfigurationEntry.LoginModuleControlFlag.SUFFICIENT);
         getLog().info(ace[n].getControlFlag());
      }

      /* Test that the UsersRolesLoginModule is sufficient to login. Only the
       users.properties file has a jduke=theduke username to password mapping,
       and the DatabaseServerLoginModule will fail.
      */
      UsernamePasswordHandler handler = new UsernamePasswordHandler("jduke", "theduke".toCharArray());
      LoginContext lc = new LoginContext("testControlFlags", handler);
      lc.login();
      Subject subject = lc.getSubject();
      Set groups = subject.getPrincipals(Group.class);
      assertTrue("Principals contains jduke", subject.getPrincipals().contains(new SimplePrincipal("jduke")));
      assertTrue("Principals contains Roles", groups.contains(new SimplePrincipal("Roles")));
      Group roles = (Group) groups.iterator().next();
      // Only the roles from the DatabaseServerLoginModule should exist
      assertTrue("Role1 is a role", roles.isMember(new SimplePrincipal("Role1")));
      assertTrue("Role2 is a role", roles.isMember(new SimplePrincipal("Role2")));
      assertTrue("Role3 is NOT a role", !roles.isMember(new SimplePrincipal("Role3")));
      assertTrue("Role4 is NOT a role", !roles.isMember(new SimplePrincipal("Role4")));
      lc.logout();

      /* Test that the DatabaseServerLoginModule is sufficient to login. Only the
        Principals table has a jduke=jduke username to password mapping, and
        the UsersRolesLoginModule will fail.
      */
      handler = new UsernamePasswordHandler("jduke", "jduke".toCharArray());
      lc = new LoginContext("testControlFlags", handler);
      lc.login();
      subject = lc.getSubject();
      groups = subject.getPrincipals(Group.class);
      assertTrue("Principals contains jduke", subject.getPrincipals().contains(new SimplePrincipal("jduke")));
      assertTrue("Principals contains Roles", groups.contains(new SimplePrincipal("Roles")));
      roles = (Group) groups.iterator().next();
      Enumeration iter = roles.members();
      while( iter.hasMoreElements() )
         getLog().debug(iter.nextElement());
      // Only the roles from the DatabaseServerLoginModule should exist
      assertTrue("Role1 is NOT a role", !roles.isMember(new SimplePrincipal("Role1")));
      assertTrue("Role2 is NOT a role", !roles.isMember(new SimplePrincipal("Role2")));
      assertTrue("Role3 is a role", roles.isMember(new SimplePrincipal("Role3")));
      assertTrue("Role4 is a role", roles.isMember(new SimplePrincipal("Role4")));
      lc.logout();
   }

   private void setupLoginTables() throws Exception
   {
      Class.forName("org.hsqldb.jdbcDriver");
      // Create a DataSource binding
      DataSource ds = new TestDS();
      InitialContext ctx = new InitialContext();
      ctx.rebind("testJdbc", ds);

      // Start database and setup tables
      Connection conn = ds.getConnection("sa", "");
      Statement statement = conn.createStatement();
      createPrincipalsTable(statement);
      createRolesTable(statement);
      statement.close();
      conn.close();
   }
   private void createPrincipalsTable(Statement statement) throws SQLException
   {
      try
      {
         statement.execute("DROP TABLE Principals");
      }
      catch(SQLException e)
      {
         // Ok, assume table does not exist
      }
      boolean result = statement.execute("CREATE TABLE Principals ("
      + "PrincipalID VARCHAR(64) PRIMARY KEY,"
      + "Password VARCHAR(64) )"
      );
      getLog().info("Created Principals table, result="+result);
      result = statement.execute("INSERT INTO Principals VALUES ('scott', 'echoman')");
      getLog().info("INSERT INTO Principals VALUES ('scott', 'echoman'), result="+result);
      result = statement.execute("INSERT INTO Principals VALUES ('stark', 'javaman')");
      getLog().info("INSERT INTO Principals VALUES ('stark', 'javaman'), result="+result);
      // This differs from the users.properties jduke settings
      result = statement.execute("INSERT INTO Principals VALUES ('jduke', 'jduke')");
      getLog().info("INSERT INTO Principals VALUES ('jduke', 'jduke'), result="+result);
   }

   private void createRolesTable(Statement statement) throws SQLException
   {
      try
      {
         statement.execute("DROP TABLE Roles");
      }
      catch(SQLException e)
      {
         // Ok, assume table does not exist
      }
      boolean result = statement.execute("CREATE TABLE Roles ("
      + "PrincipalID	VARCHAR(64),"
      + "Role	VARCHAR(64),"
      + "RoleGroup VARCHAR(64) )"
      );
      getLog().info("Created Roles table, result="+result);
      result = statement.execute("INSERT INTO Roles VALUES ('scott', 'Echo', 'Roles')");
      getLog().info("INSERT INTO Roles VALUES ('scott', 'Echo', 'Roles'), result="+result);
      result = statement.execute("INSERT INTO Roles VALUES ('scott', 'callerScott', 'CallerPrincipal')");
      getLog().info("INSERT INTO Roles VALUES ('scott', 'callerScott', 'CallerPrincipal'), result="+result);
      result = statement.execute("INSERT INTO Roles VALUES ('stark', 'Java', 'Roles')");
      getLog().info("INSERT INTO Roles VALUES ('stark', 'Java', 'Roles'), result="+result);
      result = statement.execute("INSERT INTO Roles VALUES ('stark', 'Coder', 'Roles')");
      getLog().info("INSERT INTO Roles VALUES ('stark', 'Coder', 'Roles'), result="+result);
      result = statement.execute("INSERT INTO Roles VALUES ('stark', 'callerStark', 'CallerPrincipal')");
      getLog().info("INSERT INTO Roles VALUES ('stark', 'callerStark', 'CallerPrincipal'), result="+result);
      result = statement.execute("INSERT INTO Roles VALUES ('jduke', 'Role3', 'Roles')");
      getLog().info("INSERT INTO Roles VALUES ('jduke', 'Role3', 'Roles'), result="+result);
      result = statement.execute("INSERT INTO Roles VALUES ('jduke', 'Role4', 'Roles')");
      getLog().info("INSERT INTO Roles VALUES ('jduke', 'Role4', 'Roles'), result="+result);
   }
   public static void main(java.lang.String[] args)
   {
      // Print the location of the users.properties resource
      java.net.URL users = LoginModulesUnitTestCase.class.getResource("/users.properties");
      System.out.println("users.properties is here: "+users);
      TestSuite suite = new TestSuite(LoginModulesUnitTestCase.class);
      junit.textui.TestRunner.run(suite);
   }

}
