/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.auth.spi;

import java.security.acl.Group;
import java.util.HashMap;
import java.util.Map;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.sql.DataSource;
import javax.security.auth.Subject;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.login.LoginException;
import javax.security.auth.login.FailedLoginException;

import org.jboss.security.SimpleGroup;
import org.jboss.security.SimplePrincipal;
import org.jboss.security.auth.spi.UsernamePasswordLoginModule;

/**
 * A JDBC based login module that supports authentication and role mapping.
 * It is based on two logical tables:
 * <ul>
 * <li>Principals(PrincipalID text, Password text)
 * <li>Roles(PrincipalID text, Role text, RoleGroup text)
 * </ul>
 * <p>
 * LoginModule options:
 * <ul>
 * <li><em>dsJndiName</em>: The name of the DataSource of the database containing the Principals, Roles tables
 * <li><em>principalsQuery</em>: The prepared statement query, equivalent to:
 * <pre>
 *    "select Password from Principals where PrincipalID=?"
 * </pre>
 * <li><em>rolesQuery</em>: The prepared statement query, equivalent to:
 * <pre>
 *    "select Role, RoleGroup from Roles where PrincipalID=?"
 * </pre>
 * </ul>
 *
 * @author <a href="mailto:on@ibis.odessa.ua">Oleg Nitz</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.6.4.1 $
 */
public class DatabaseServerLoginModule extends UsernamePasswordLoginModule
{
   private String dsJndiName;
   private String principalsQuery = "select Password from Principals where PrincipalID=?";
   private String rolesQuery = "select Role, RoleGroup from Roles where PrincipalID=?";
   
   /**
    * Initialize this LoginModule.
    */
   public void initialize(Subject subject, CallbackHandler callbackHandler, Map sharedState, Map options)
   {
      super.initialize(subject, callbackHandler, sharedState, options);
      dsJndiName = (String) options.get("dsJndiName");
      if( dsJndiName == null )
         dsJndiName = "java:/DefaultDS";
      Object tmp = options.get("principalsQuery");
      if( tmp != null )
         principalsQuery = tmp.toString();
      tmp = options.get("rolesQuery");
      if( tmp != null )
         rolesQuery = tmp.toString();
      log.trace("DatabaseServerLoginModule, dsJndiName="+dsJndiName);
      log.trace("principalsQuery="+principalsQuery);
      log.trace("rolesQuery="+rolesQuery);
   }

   /** Get the expected password for the current username available via
    * the getUsername() method. This is called from within the login()
    * method after the CallbackHandler has returned the username and
    * candidate password.
    * @return the valid password String
    */
   protected String getUsersPassword() throws LoginException
   {
      String username = getUsername();
      String password = null;
      Connection conn = null;
      PreparedStatement ps = null;
      
      try
      {
         InitialContext ctx = new InitialContext();
         DataSource ds = (DataSource) ctx.lookup(dsJndiName);
         conn = ds.getConnection();
         // Get the password
         ps = conn.prepareStatement(principalsQuery);
         ps.setString(1, username);
         ResultSet rs = ps.executeQuery();
         if( rs.next() == false )
            throw new FailedLoginException("No matching username found in Principals");
         
         password = rs.getString(1);
         password = convertRawPassword(password);
         rs.close();
      }
      catch(NamingException ex)
      {
         throw new LoginException(ex.toString(true));
      }
      catch(SQLException ex)
      {
         log.error("Query failed", ex);
         throw new LoginException(ex.toString());
      }
      finally
      {
         if( ps != null )
         {
            try
            {
               ps.close();
            }
            catch(SQLException e)
            {}
         }
         if( conn != null )
         {
            try
            {
               conn.close();
            }
            catch (SQLException ex)
            {}
         }
      }
      return password;
   }

   /** Overriden by subclasses to return the Groups that correspond to the
    to the role sets assigned to the user. Subclasses should create at
    least a Group named "Roles" that contains the roles assigned to the user.
    A second common group is "CallerPrincipal" that provides the application
    identity of the user rather than the security domain identity.
    @return Group[] containing the sets of roles
    */
   protected Group[] getRoleSets() throws LoginException
   {
      String username = getUsername();
      Connection conn = null;
      HashMap setsMap = new HashMap();
      PreparedStatement ps = null;

      try
      {
         InitialContext ctx = new InitialContext();
         DataSource ds = (DataSource) ctx.lookup(dsJndiName);
         conn = ds.getConnection();
         // Get the user role names
         ps = conn.prepareStatement(rolesQuery);
         try
         {
            ps.setString(1, username);
         }
         catch(ArrayIndexOutOfBoundsException ignore)
         {
            // The query may not have any parameters so just try it
         }
         ResultSet rs = ps.executeQuery();
         if( rs.next() == false )
         {
            if( getUnauthenticatedIdentity() == null )
               throw new FailedLoginException("No matching username found in Roles");
            /* We are running with an unauthenticatedIdentity so create an
               empty Roles set and return.
            */
            Group[] roleSets = { new SimpleGroup("Roles") };
            return roleSets;
         }

         do
         {
            String name = rs.getString(1);
            String groupName = rs.getString(2);
            if( groupName == null || groupName.length() == 0 )
               groupName = "Roles";
            Group group = (Group) setsMap.get(groupName);
            if( group == null )
            {
               group = new SimpleGroup(groupName);
               setsMap.put(groupName, group);
            }
            group.addMember(new SimplePrincipal(name));
         } while( rs.next() );
         rs.close();
      }
      catch(NamingException ex)
      {
         throw new LoginException(ex.toString(true));
      }
      catch(SQLException ex)
      {
         super.log.error("SQL failure", ex);
         throw new LoginException(ex.toString());
      }
      finally
      {
         if( ps != null )
         {
            try
            {
               ps.close();
            }
            catch(SQLException e)
            {}
         }
         if( conn != null )
         {
            try
            {
               conn.close();
            }
            catch (Exception ex)
            {}
         }
      }
      
      Group[] roleSets = new Group[setsMap.size()];
      setsMap.values().toArray(roleSets);
      return roleSets;
   }
   
   /** A hook to allow subclasses to convert a password from the database
    into a plain text string or whatever form is used for matching against
    the user input. It is called from within the getUsersPassword() method.
    @param rawPassword, the password as obtained from the database
    @return the argument rawPassword
    */
   protected String convertRawPassword(String rawPassword)
   {
      return rawPassword;
   }
}
