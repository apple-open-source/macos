/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.auth.spi;

import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.Map;
import java.util.Properties;
import java.util.StringTokenizer;

import java.security.acl.Group;
import javax.security.auth.Subject;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.login.LoginException;

import org.jboss.security.SimpleGroup;
import org.jboss.security.SimplePrincipal;
import org.jboss.security.auth.spi.UsernamePasswordLoginModule;

/** A simple properties file based login module that consults two Java Properties
formatted text files for username to password("users.properties") and
username to roles("roles.properties") mapping. The names of the properties
files may be overriden by the usersProperties and rolesProperties options.
The properties files are loaded during initialization using the thread context
class loader. This means that these files can be placed into the J2EE
deployment jar or the JBoss config directory.

The users.properties file uses a format:
    username1=password1
    username2=password2
    ...

to define all valid usernames and their corresponding passwords.

The roles.properties file uses a format:
    username1=role1,role2,...
    username1.RoleGroup1=role3,role4,...
    username2=role1,role3,...

to define the sets of roles for valid usernames. The "username.XXX" form of
property name is used to assign the username roles to a particular named
group of roles where the XXX portion of the property name is the group name.
The "username=..." form is an abbreviation for "username.Roles=...".
The following are therefore equivalent:
    jduke=TheDuke,AnimatedCharacter
    jduke.Roles=TheDuke,AnimatedCharacter

@author <a href="edward.kenworthy@crispgroup.co.uk">Edward Kenworthy</a>, 12th Dec 2000
@author Scott.Stark@jboss.org
*/
public class UsersRolesLoginModule extends UsernamePasswordLoginModule
{
    /** The name of the properties resource containing user/passwords */
    private String usersRsrcName = "users.properties";
    /** The name of the properties resource containing user/roles */
    private String rolesRsrcName = "roles.properties";
    /** The users.properties values */
    private Properties users;
    /** The roles.properties values */
    private Properties roles;

    /** Initialize this LoginModule.
     *@param options, the login module option map. Supported options include:
     *usersProperties: The name of the properties resource containing
      user/passwords. The default is "users.properties"
     *rolesProperties: The name of the properties resource containing user/roles
      The default is "roles.properties".
     */
    public void initialize(Subject subject, CallbackHandler callbackHandler, Map sharedState, Map options)
    {
        super.initialize(subject, callbackHandler, sharedState, options);
        try
        {
            // Check for usersProperties & rolesProperties
            String option = (String) options.get("usersProperties");
            if( option != null )
               usersRsrcName = option;
            option = (String) options.get("rolesProperties");
            if( option != null )
               rolesRsrcName = option;
            // Load the properties file that contains the list of users and passwords
            loadUsers();
            loadRoles();
        }
        catch(Exception e)
        {
            // Note that although this exception isn't passed on, users or roles will be null
            // so that any call to login will throw a LoginException.
            super.log.error("Failed to load users/passwords/role files", e);
        }
    }

    /** Method to authenticate a Subject (phase 1). This validates that the
     *users and roles properties files were loaded and then calls
     *super.login to perform the validation of the password.
     *@exception LoginException, thrown if the users or roles properties files
     *were not found or the super.login method fails.
     */
    public boolean login() throws LoginException
    {
        if( users == null )
            throw new LoginException("Missing users.properties file.");
        if( roles == null )
            throw new LoginException("Missing roles.properties file.");

        return super.login();
    }

    /** Create the set of roles the user belongs to by parsing the roles.properties
        data for username=role1,role2,... and username.XXX=role1,role2,...
        patterns.
    @return Group[] containing the sets of roles 
    */
    protected Group[] getRoleSets() throws LoginException
    {
        String targetUser = getUsername();
        Enumeration users = roles.propertyNames();
        SimpleGroup rolesGroup = new SimpleGroup("Roles");
        ArrayList groups = new ArrayList();
        groups.add(rolesGroup);
        while( users.hasMoreElements() && targetUser != null )
        {
            String user = (String) users.nextElement();
            String value = roles.getProperty(user);
            // See if this entry is of the form targetUser[.GroupName]=roles
            int index = user.indexOf('.');
            boolean isRoleGroup = false;
            boolean userMatch = false;
            if( index > 0 && targetUser.regionMatches(0, user, 0, index) == true )
                isRoleGroup = true;
            else
               userMatch = targetUser.equals(user);

            // Check for username.RoleGroup pattern
            if( isRoleGroup == true )
            {
                String groupName = user.substring(index+1);
                if( groupName.equals("Roles") )
                    parseGroupMembers(rolesGroup, value);
                else
                {
                    SimpleGroup group = new SimpleGroup(groupName);
                    parseGroupMembers(group, value);
                    groups.add(group);
                }
            }
            else if( userMatch == true )
            {
                // Place these roles into the Default "Roles" group
                parseGroupMembers(rolesGroup, value);
            }
        }
        Group[] roleSets = new Group[groups.size()];
        groups.toArray(roleSets);
        return roleSets;
    }
    protected String getUsersPassword()
    {
        String username = getUsername();
        String password = null;
        if( username != null )
            password = users.getProperty(username , null);
        return password;
    }

// utility methods
    private void parseGroupMembers(Group group, String value)
    {
        StringTokenizer tokenizer = new StringTokenizer(value, ",");
        while( tokenizer.hasMoreTokens() )
        {
            String token = tokenizer.nextToken();
            SimplePrincipal p = new SimplePrincipal(token);
            group.addMember(p);
        }
    }

    private void loadUsers() throws IOException
    {
        users = loadProperties(usersRsrcName);
    }

    private void loadRoles() throws IOException
    {
        roles = loadProperties(rolesRsrcName);
    }

    /**
    * Loads the given properties file and returns a Properties object containing the
    * key,value pairs in that file.
    * The properties files should be in the class path.
    */
    private Properties loadProperties(String propertiesName) throws IOException
    {
        Properties bundle = null;
        ClassLoader loader = Thread.currentThread().getContextClassLoader();
        URL url = loader.getResource(propertiesName);
        if( url == null )
            throw new IOException("Properties file " + propertiesName + " not found");
        super.log.trace("Properties file="+url);
        InputStream is = url.openStream();
        if( is != null )
        {
            bundle = new Properties();
            bundle.load(is);
        }
        else
        {
            throw new IOException("Properties file " + propertiesName + " not avilable");
        }
        return bundle;
    }
}
