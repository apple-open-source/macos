/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test;

import java.lang.reflect.Method;
import java.io.File;
import java.io.Serializable;
import java.security.MessageDigest;
import java.security.acl.Group;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Set;
import java.util.Properties;
import javax.naming.InitialContext;
import javax.naming.NameAlreadyBoundException;
import javax.security.auth.Subject;
import javax.security.auth.login.AppConfigurationEntry;
import javax.security.auth.login.Configuration;
import javax.security.auth.login.LoginContext;
import javax.security.auth.login.LoginException;
import javax.sql.DataSource;

import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.apache.log4j.Category;
import org.apache.log4j.FileAppender;
import org.apache.log4j.PatternLayout;

import org.jboss.logging.XLevel;
import org.jboss.security.SimpleGroup;
import org.jboss.security.SimplePrincipal;
import org.jboss.security.Util;
import org.jboss.security.auth.callback.UsernamePasswordHandler;
import org.jboss.security.auth.spi.UsernamePasswordLoginModule;

/** Tests of the LoginModule classes.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.3.2.3 $
 */
public class LoginModulesTestCase extends TestCase
{
   static
   {
      try
      {
         Configuration.setConfiguration(new TestConfig());
         System.out.println("Installed TestConfig as JAAS Configuration");
      }
      catch(Exception e)
      {
         e.printStackTrace();
      }
   }
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
      AppConfigurationEntry[] testLdap()
      {
         String name = "org.jboss.security.auth.spi.LdapLoginModule";
         HashMap options = new HashMap();
         options.put("java.naming.factory.initial", "com.sun.jndi.ldap.LdapCtxFactory");
         options.put("java.naming.provider.url", "ldap://siren/");
         options.put("java.naming.security.authentication", "simple");
         options.put("principalDNPrefix", "uid=");
         options.put("principalDNSuffix", ",ou=People,o=starkinternational.com");
         options.put("rolesCtxDN", "cn=JBossSX Test Roles,ou=Roles,o=starkinternational.com");
         options.put("uidAttributeID", "userid");
         options.put("roleAttributeID", "roleName");
         AppConfigurationEntry ace = new AppConfigurationEntry(name,
         AppConfigurationEntry.LoginModuleControlFlag.REQUIRED, options);
         AppConfigurationEntry[] entry = {ace};
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
   static class TestDS implements DataSource, Serializable
   {
      public java.sql.Connection getConnection() throws java.sql.SQLException
      {
         return getConnection("sa", "");
      }
      public java.sql.Connection getConnection(String user, String pass) throws java.sql.SQLException
      {
         String jdbcURL = "jdbc:hsqldb:hsql://localhost:1701";
         java.sql.Connection con = DriverManager.getConnection(jdbcURL, user, pass);
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

   public LoginModulesTestCase(String testName)
   {
      super(testName);
   }
   
   protected void setUp() throws Exception
   {
      // Set up a simple configuration that logs to LoginModulesTestCase.log
      Category root = Category.getRoot();
      root.setLevel(XLevel.TRACE);
      FileAppender appender = new FileAppender(new PatternLayout("%x%m%n"), "LoginModulesTestCase.log");
      root.addAppender(appender);
   }

   public void testUsernamePassword() throws Exception
   {
      System.out.println("testUsernamePassword");
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
      System.out.println("testUsernamePasswordHash");
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
      System.out.println("testUsersRoles");
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
            System.out.println("CallerPrincipal is "+roles.members().nextElement());
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
            System.out.println("CallerPrincipal is "+roles.members().nextElement());
            boolean isMember = roles.isMember(new SimplePrincipal("callerStark"));
            assertTrue("CallerPrincipal is callerStark", isMember);
         }
      }
      lc.logout();

      // Test the usernames with common prefix
      System.out.println("Testing similar usernames");
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
            System.out.println("CallerPrincipal is "+roles.members().nextElement());
            boolean isMember = roles.isMember(new SimplePrincipal("callerJdukeman"));
            assertTrue("CallerPrincipal is callerJdukeman", isMember);
         }
      }
      lc.logout();
   }

   public void testUsersRolesHash() throws Exception
   {
      System.out.println("testUsersRolesHash");
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
            System.out.println("CallerPrincipal is "+roles.members().nextElement());
            boolean isMember = roles.isMember(new SimplePrincipal("callerScott"));
            assertTrue("CallerPrincipal is callerScott", isMember);
         }
      }
      lc.logout();
   }

   public void testAnonUsersRoles() throws Exception
   {
      System.out.println("testAnonUsersRoles");
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
      System.out.println("testAnon");
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
      System.out.println("testNull");
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
      System.out.println("testIdentity");
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
   public void testSimple() throws Exception
   {
      System.out.println("testSimple");
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
   public void testLdap() throws Exception
   {
      System.out.println("testLdap");
      UsernamePasswordHandler handler = new UsernamePasswordHandler("jduke", "theduke".toCharArray());
      LoginContext lc = new LoginContext("testLdap", handler);
      lc.login();
      Subject subject = lc.getSubject();
      Set groups = subject.getPrincipals(Group.class);
      assertTrue("Principals contains jduke", subject.getPrincipals().contains(new SimplePrincipal("jduke")));
      assertTrue("Principals contains Roles", groups.contains(new SimplePrincipal("Roles")));
      Group roles = (Group) groups.iterator().next();
      assertTrue("Echo is a role", roles.isMember(new SimplePrincipal("Echo")));
      assertTrue("TheDuke is a role", roles.isMember(new SimplePrincipal("TheDuke")));
      
      lc.logout();
   }
   public void testLdapNullPassword() throws Exception
   {
      System.out.println("testLdapNullPassword");
      UsernamePasswordHandler handler = new UsernamePasswordHandler("jduke", null);
      LoginContext lc = new LoginContext("testLdap", handler);
      try
      {
         // This login should fail
         lc.login();
         fail("Was able to login with null password");
      }
      catch(LoginException ok)
      {
      }
   }

   /** Use this InstantDB script to setup tables:
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
      System.out.println("testJdbc");
      try
      {
         Class.forName("org.hsqldb.jdbcDriver");
      }
      catch(ClassNotFoundException e)
      {   // Skip the test
         System.out.println("Skipping test because org.hsqldb.jdbcDriver was not found");
         return;
      }
      // Create a DataSource binding
      DataSource ds = new TestDS();
      Properties env = new Properties();
      org.jnp.server.Main naming = new org.jnp.server.Main();
      naming.start();
      System.setProperty("java.naming.factory.initial", "org.jnp.interfaces.NamingContextFactory");
      System.setProperty("java.naming.provider.url", "localhost");
      InitialContext ctx = new InitialContext(System.getProperties());
      try
      {
         ctx.bind("testJdbc", ds);
      }
      catch(NameAlreadyBoundException e)
      {
         // Ignore
      }
      
      // Start database and setup tables
      startHsql();
      Connection conn = ds.getConnection("sa", "");
      Statement statement = conn.createStatement();
      createPrincipalsTable(statement);
      createRolesTable(statement);
      statement.close();
      conn.close();
      
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
   
   static void startHsql()
   {
      // Start DB in new thread, or else it will block us
      Thread runner = new Thread(new Runnable()
      {
         public void run()
         {
            File dbDir = new File("hypersonic");
            dbDir.mkdir();
            File dbName = new File(dbDir, "DBLogin");
            // Create startup arguments
            String[] args = new String[]
            {
               "-database", dbName.toString(),
               "-port", "1701",
               "-silent", "true",
               "-trace", "false"
            };
            // Start server
            org.hsqldb.Server.main(args);
         }
      });
      
      runner.start();
      System.out.println("HSQL database started");
   }
   
   static void createPrincipalsTable(Statement statement) throws SQLException
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
      System.out.println("Created Principals table, result="+result);
      result = statement.execute("INSERT INTO Principals VALUES ('scott', 'echoman')");
      System.out.println("INSERT INTO Principals VALUES ('scott', 'echoman'), result="+result);
      result = statement.execute("INSERT INTO Principals VALUES ('stark', 'javaman')");
      System.out.println("INSERT INTO Principals VALUES ('stark', 'javaman'), result="+result);
   }
   
   static void createRolesTable(Statement statement) throws SQLException
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
      System.out.println("Created Roles table, result="+result);
      result = statement.execute("INSERT INTO Roles VALUES ('scott', 'Echo', 'Roles')");
      System.out.println("INSERT INTO Roles VALUES ('scott', 'Echo', 'Roles'), result="+result);
      result = statement.execute("INSERT INTO Roles VALUES ('scott', 'callerScott', 'CallerPrincipal')");
      System.out.println("INSERT INTO Roles VALUES ('scott', 'callerScott', 'CallerPrincipal'), result="+result);
      result = statement.execute("INSERT INTO Roles VALUES ('stark', 'Java', 'Roles')");
      System.out.println("INSERT INTO Roles VALUES ('stark', 'Java', 'Roles'), result="+result);
      result = statement.execute("INSERT INTO Roles VALUES ('stark', 'Coder', 'Roles')");
      System.out.println("INSERT INTO Roles VALUES ('stark', 'Coder', 'Roles'), result="+result);
      result = statement.execute("INSERT INTO Roles VALUES ('stark', 'callerStark', 'CallerPrincipal')");
      System.out.println("INSERT INTO Roles VALUES ('stark', 'callerStark', 'CallerPrincipal'), result="+result);
   }
   public static void main(java.lang.String[] args)
   {
      System.setErr(System.out);
      // Print the location of the users.properties resource
      java.net.URL users = LoginModulesTestCase.class.getResource("/users.properties");
      System.out.println("users.properties is here: "+users);
      TestSuite suite = new TestSuite(LoginModulesTestCase.class);
      junit.textui.TestRunner.run(suite);
   }
   
}
