// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: HashUserRealm.java,v 1.15.2.6 2003/06/04 04:47:41 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.http;

import java.io.Externalizable;
import java.io.IOException;
import java.io.PrintStream;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.StringTokenizer;
import org.mortbay.util.Code;
import org.mortbay.util.Credential;
import org.mortbay.util.Password;
import org.mortbay.util.Resource;

/* ------------------------------------------------------------ */
/** HashMapped User Realm.
 *
 * An implementation of UserRealm that stores users and roles in-memory in
 * HashMaps.
 * <P>
 * Typically these maps are populated by calling the load() method or passing
 * a properties resource to the constructor. The format of the properties
 * file is: <PRE>
 *  username: password [,rolename ...]
 * </PRE>
 * Passwords may be clear text, obfuscated or checksummed.  The class 
 * com.mortbay.Util.Password should be used to generate obfuscated
 * passwords or password checksums.
 * 
 * If DIGEST Authentication is used, the password must be in a recoverable
 * format, either plain text or OBF:.
 *
 * The HashUserRealm also implements SSORealm but provides no implementation
 * of SSORealm. Instead setSSORealm may be used to provide a delegate
 * SSORealm implementation. 
 *
 * @see Password
 * @version $Id: HashUserRealm.java,v 1.15.2.6 2003/06/04 04:47:41 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class HashUserRealm
    extends HashMap
    implements UserRealm, SSORealm, Externalizable
{
    /** HttpContext Attribute to set to activate SSO.
     */
    public static final String __SSO = "org.mortbay.http.SSO";
    
    /* ------------------------------------------------------------ */
    private String _realmName;
    private String _config;
    protected HashMap _roles=new HashMap(7);
    private SSORealm _ssoRealm;
    

    /* ------------------------------------------------------------ */
    /** Constructor. 
     */
    public HashUserRealm()
    {}
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param name Realm Name
     */
    public HashUserRealm(String name)
    {
        _realmName=name;
    }
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param name Realm name
     * @param config Filename or url of user properties file.
     */
    public HashUserRealm(String name, String config)
        throws IOException
    {
        _realmName=name;
        load(config);
    }
    
    /* ------------------------------------------------------------ */
    public void writeExternal(java.io.ObjectOutput out)
        throws java.io.IOException
    {
        out.writeObject(_realmName);
        out.writeObject(_config);
    }
    
    /* ------------------------------------------------------------ */
    public void readExternal(java.io.ObjectInput in)
        throws java.io.IOException, ClassNotFoundException
    {
        _realmName= (String)in.readObject();
        _config=(String)in.readObject();
        if (_config!=null)
            load(_config);
    }
    

    /* ------------------------------------------------------------ */
    /** Load realm users from properties file.
     * The property file maps usernames to password specs followed by
     * an optional comma separated list of role names.
     *
     * @param config Filename or url of user properties file.
     * @exception IOException 
     */
    public void load(String config)
        throws IOException
    {
        _config=config;
        Code.debug("Load ",this," from ",config);
        Properties properties = new Properties();
        Resource resource=Resource.newResource(config);
        properties.load(resource.getInputStream());

        Iterator iter = properties.entrySet().iterator();
        while(iter.hasNext())
        {
            Map.Entry entry = (Map.Entry)iter.next();

            String username=entry.getKey().toString().trim();
            String credentials=entry.getValue().toString().trim();
            String roles=null;
            int c=credentials.indexOf(',');
            if (c>0)
            {
                roles=credentials.substring(c+1).trim();
                credentials=credentials.substring(0,c).trim();
            }

            if (username!=null && username.length()>0 &&
                credentials!=null && credentials.length()>0)
            {
                put(username,credentials);
                if(roles!=null && roles.length()>0)
                {
                    StringTokenizer tok = new StringTokenizer(roles,", ");
                    while (tok.hasMoreTokens())
                        addUserToRole(username,tok.nextToken());
                }
            }
        }
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @param name The realm name 
     */
    public void setName(String name)
    {
        _realmName=name;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @return The realm name. 
     */
    public String getName()
    {
        return _realmName;
    }

    /* ------------------------------------------------------------ */
    public UserPrincipal authenticate(String username,
                                      Object credentials,
                                      HttpRequest request)
    {
        KnownUser user;
        synchronized (this)
        {
            user = (KnownUser)super.get(username);
        }
        if (user==null)
            return null;

        if (user.authenticate(credentials))
            return user;
        
        return null;
    }
    
    /* ------------------------------------------------------------ */
    public void disassociate(UserPrincipal user)
    {
    }
    
    /* ------------------------------------------------------------ */
    public UserPrincipal pushRole(UserPrincipal user, String role)
    {
        if (user==null)
            user=new User();
        
        return new WrappedUser(user,role);
    }

    /* ------------------------------------------------------------ */
    public UserPrincipal popRole(UserPrincipal user)
    {
        WrappedUser wu = (WrappedUser)user;
        return wu.getUserPrincipal();
    }
    
    /* ------------------------------------------------------------ */
    /** Put user into realm.
     * @param name User name
     * @param credentials String password, Password or UserPrinciple
     *                    instance. 
     * @return Old UserPrinciple value or null
     */
    public synchronized Object put(Object name, Object credentials)
    {
        if (credentials instanceof UserPrincipal)
            return super.put(name.toString(),
                             credentials);
        
        if (credentials instanceof Password)
            return super.put(name,
                             new KnownUser(name.toString(),
                                          (Password)credentials));
        if (credentials != null)
            return super
                .put(name,
                     new KnownUser(name.toString(),
                                   Credential.getCredential(credentials.toString())));
        return null;
    }

    /* ------------------------------------------------------------ */
    /** Add a user to a role.
     * @param userName 
     * @param roleName 
     */
    public synchronized void addUserToRole(String userName, String roleName)
    {
        HashSet userSet = (HashSet)_roles.get(roleName);
        if (userSet==null)
        {
            userSet=new HashSet(11);
            _roles.put(roleName,userSet);
        }
        userSet.add(userName);
    }
    
    /* ------------------------------------------------------------ */
    /** Check if a user is in a role.
     * @param user The user, which must be from this realm 
     * @param roleName 
     * @return True if the user can act in the role.
     */
    public synchronized boolean isUserInRole(UserPrincipal user, String roleName)
    {
        if (user==null || ((User)user).getUserRealm()!=this)
            return false;
        
        HashSet userSet = (HashSet)_roles.get(roleName);
        return userSet!=null && userSet.contains(user.getName());
    }

    /* ------------------------------------------------------------ */
    public String toString()
    {
        return "Realm["+_realmName+"]";
    }
    
    /* ------------------------------------------------------------ */
    public void dump(PrintStream out)
    {
        out.println(this+":");
        out.println(super.toString());
        out.println(_roles);
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @return The SSORealm to delegate single sign on requests to.
     */
    public SSORealm getSSORealm()
    {
        return _ssoRealm;
    }
    
    /* ------------------------------------------------------------ */
    /** Set the SSORealm.
     * A SSORealm implementation may be set to enable support for SSO.
     * @param ssoRealm The SSORealm to delegate single sign on requests to.
     */
    public void setSSORealm(SSORealm ssoRealm)
    {
        _ssoRealm = ssoRealm;
    }
    
    /* ------------------------------------------------------------ */
    public Credential getSingleSignOn(HttpRequest request,
                                      HttpResponse response)
    {
        if (_ssoRealm!=null)
            return _ssoRealm.getSingleSignOn(request,response);
        return null;
    }
    
    
    /* ------------------------------------------------------------ */
    public void setSingleSignOn(HttpRequest request,
                                HttpResponse response,
                                UserPrincipal principal,
                                Credential credential)
    {
        if (_ssoRealm!=null)
            _ssoRealm.setSingleSignOn(request,response,principal,credential);
    }
    
    /* ------------------------------------------------------------ */
    public void clearSingleSignOn(String username)
    {
        if (_ssoRealm!=null)
            _ssoRealm.clearSingleSignOn(username);
    }
    
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    private class User implements UserPrincipal
    {
        List roles=null;

        /* ------------------------------------------------------------ */
        private UserRealm getUserRealm()
        {
            return HashUserRealm.this;
        }
        
        public String getName()
        {
            return "Anonymous";
        }
                
        public boolean isAuthenticated()
        {
            return false;
        }
        
        public boolean isUserInRole(String role)
        {
            return false;
        }
        
        public String toString()
        {
            return getName();
        }        
    }
    
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    private class KnownUser extends User
    {
        private String _userName;
        private Credential _cred;
        
        /* -------------------------------------------------------- */
        KnownUser(String name,Credential credential)
        {
            _userName=name;
            _cred=credential;
        }
        
        /* -------------------------------------------------------- */
        private boolean authenticate(Object credentials)
        {
            return _cred!=null && _cred.check(credentials);
        }
        
        /* ------------------------------------------------------------ */
        public String getName()
        {
            return _userName;
        }
        
        /* -------------------------------------------------------- */
        public boolean isAuthenticated()
        {
            return true;
        }
    
        /* -------------------------------------------------------- */
        public boolean isUserInRole(String role)
        {
            return HashUserRealm.this.isUserInRole(this,role);
        }
    }

    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    private class WrappedUser extends User
    {   
        private UserPrincipal user;
        private String role;

        private WrappedUser(UserPrincipal user, String role)
        {
            this.user=user;
            this.role=role;
        }

        private UserPrincipal getUserPrincipal()
        {
            return user;    
        }

        public String getName()
        {
            return "role:"+role;
        }
                
        public boolean isAuthenticated()
        {
            return true;
        }
        
        public boolean isUserInRole(String role)
        {
            return this.role.equals(role);
        }
    }
}
