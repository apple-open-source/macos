// ========================================================================
// Copyright (c) 2000 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: SecurityConstraint.java,v 1.15.2.11 2003/07/11 00:55:12 jules_gosnell Exp $
// ========================================================================

package org.mortbay.http;

import java.io.IOException;
import java.io.Serializable;
import java.util.Collections;
import java.util.List;
import org.mortbay.util.Code;
import org.mortbay.util.LazyList;



/* ------------------------------------------------------------ */
/** Describe an auth and/or data constraint. 
 *
 * @version $Revision: 1.15.2.11 $
 * @author Greg Wilkins (gregw)
 */
public class SecurityConstraint
    implements Cloneable, Serializable
{
    /* ------------------------------------------------------------ */
    public final static String __BASIC_AUTH="BASIC";
    public final static String __FORM_AUTH="FORM";
    public final static String __DIGEST_AUTH="DIGEST";
    public final static String __CERT_AUTH="CLIENT_CERT";
    public final static String __CERT_AUTH2="CLIENT-CERT";

    
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    public interface Authenticator extends Serializable
    {
        /* ------------------------------------------------------------ */
        /** Authenticate.
         * @return UserPrincipal if authenticated. Null if Authentication
         * failed. If the SecurityConstraint.__NOBODY instance is returned,
         * the request is considered as part of the authentication process.
         * @exception IOException 
         */
        public UserPrincipal authenticated(UserRealm realm,
                                           String pathInContext,
                                           HttpRequest request,
                                           HttpResponse response)
        throws IOException;

        public String getAuthMethod();
    }

    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /** Nobody user.
     * The Nobody UserPrincipal is used to indicate a partial state of
     * authentication. A request with a Nobody UserPrincipal will be allowed
     * past all authentication constraints - but will not be considered an
     * authenticated request.  It can be used by Authenticators such as
     * FormAuthenticator to allow access to logon and error pages within an
     * authenticated URI tree.
     */
    public static class Nobody implements UserPrincipal
    {
        public String getName() { return "Nobody";}
        public boolean isAuthenticated() {return false;}
        public boolean isUserInRole(String role) {return false;}
    }
    public final static Nobody __NOBODY=new Nobody();

    
    /* ------------------------------------------------------------ */
    public final static int
        DC_NONE=0,
        DC_INTEGRAL=1,
        DC_CONFIDENTIAL=2;
    
    /* ------------------------------------------------------------ */
    public final static String NONE="NONE";
    public final static String ANY_ROLE="*";
    
    /* ------------------------------------------------------------ */
    private String _name;
    private Object _methods;
    private List _umMethods;
    private Object _roles;
    private List _umRoles;
    private int _dataConstraint=DC_NONE;
    private boolean _anyRole=false;
    private boolean _authenticate=false;

    /* ------------------------------------------------------------ */
    /** Constructor. 
     */
    public SecurityConstraint()
    {}

    /* ------------------------------------------------------------ */
    /** Conveniance Constructor. 
     * @param name 
     * @param role 
     */
    public SecurityConstraint(String name,String role)
    {
        setName(name);
        addRole(role);
    }
    
    /* ------------------------------------------------------------ */
    /**
     * @param name 
     */
    public void setName(String name)
    {
        _name=name;
    }    

    /* ------------------------------------------------------------ */
    /** 
     * @param method 
     */
    public synchronized void addMethod(String method)
    {
        _methods=LazyList.add(_methods,method);
    }
    
    /* ------------------------------------------------------------ */
    public List getMethods()
    {
        if (_umMethods==null && _methods!=null)
            _umMethods=Collections.unmodifiableList(LazyList.getList(_methods));
        return _umMethods;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @param method Method name.
     * @return True if this constraint applies to the method. If no
     * method has been set, then the constraint applies to all methods.
     */
    public boolean forMethod(String method)
    {
        if (_methods==null)
            return true;
        for (int i=0;i<LazyList.size(_methods);i++)
            if (LazyList.get(_methods,i).equals(method))
                return true;
        return false;
    }

    /* ------------------------------------------------------------ */
    /** 
     * @param role The rolename.  If the rolename is '*' all other
     * roles are removed and anyRole is set true and subsequent
     * addRole calls are ignored.
     * Authenticate is forced true by this call.
     */
    public synchronized void addRole(String role)
    {
        _authenticate=true;
        if (ANY_ROLE.equals(role))
        {
            _roles=null;
            _umRoles=null;
            _anyRole=true;
        }
        else if (!_anyRole)
            _roles=LazyList.add(_roles,role);
    }

    /* ------------------------------------------------------------ */
    /** 
     * @return True if any user role is permitted.
     */
    public boolean isAnyRole()
    {
        return _anyRole;
    }

    /* ------------------------------------------------------------ */
    /** 
     * @return List of roles for this constraint.
     */
    public List getRoles()
    {
        if (_umRoles==null && _roles!=null)
            _umRoles=Collections.unmodifiableList(LazyList.getList(_roles));
        return _umRoles;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @param role 
     * @return True if the constraint contains the role.
     */
    public boolean hasRole(String role)
    {
        return LazyList.contains(_roles,role);
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @param authenticate True if users must be authenticated 
     */
    public void setAuthenticate(boolean authenticate)
    {
        _authenticate=authenticate;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @return True if the constraint requires request authentication
     */
    public boolean isAuthenticate()
    {
        return _authenticate;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @return True if authentication required but no roles set
     */
    public boolean isForbidden()
    {
        return _authenticate && !_anyRole && LazyList.size(_roles)==0;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @param c 
     */
    public void setDataConstraint(int c)
    {
        if (c<0 || c>DC_CONFIDENTIAL)
            throw new IllegalArgumentException("Constraint out of range");
        _dataConstraint=c;
    }


    /* ------------------------------------------------------------ */
    /** 
     * @return Data constrain indicator: 0=DC+NONE, 1=DC_INTEGRAL & 2=DC_CONFIDENTIAL
     */
    public int getDataConstraint()
    {
        return _dataConstraint;
    }

    /* ------------------------------------------------------------ */
    /** 
     * @return True if there is a data constraint.
     */
    public boolean hasDataConstraint()
    {
        return _dataConstraint>DC_NONE;
    }
    
    
    /* ------------------------------------------------------------ */
    public Object clone()
    {
        SecurityConstraint sc=new SecurityConstraint();
        sc._name=_name;
        sc._dataConstraint=_dataConstraint;
        sc._anyRole=_anyRole;
        sc._authenticate=_authenticate;
        
        sc._methods=LazyList.clone(_methods);
        sc._roles=LazyList.clone(_roles);
        
        return sc;
    }
    
    /* ------------------------------------------------------------ */
    public String toString()
    {
        return "SC{"+_name+
            ","+_methods+
            ","+(_anyRole?"*":(_roles==null?"-":_roles.toString()))+
            ","+(_dataConstraint==DC_NONE
                 ?"NONE}"
                 :(_dataConstraint==DC_INTEGRAL?"INTEGRAL}":"CONFIDENTIAL}"));
    }


    /* ------------------------------------------------------------ */
    /** Check security contraints
     * @param constraints 
     * @param authenticator 
     * @param realm 
     * @param pathInContext 
     * @param request 
     * @param response 
     * @return -1 for  failed, 0 for authentication in process, 1 for passed.
     * @exception HttpException 
     * @exception IOException 
     */
    public static int check(List constraints,
                            Authenticator authenticator,
                            UserRealm realm,
                            String pathInContext,
                            HttpRequest request,
                            HttpResponse response)
        throws HttpException, IOException
    {   
        // for each constraint
        for (int c=0;c<constraints.size();c++)
        {
            SecurityConstraint sc=(SecurityConstraint)constraints.get(c);

            // Check the method applies
            if (!sc.forMethod(request.getMethod()))
                continue;
                    
            // Does this forbid everything?
            if (sc.isForbidden())
            {
                response.sendError(HttpResponse.__403_Forbidden);
                return -1;
            }
            
            // To avoid revealing a request parameter, in the case of browsing
            // to a CONFIDENTIAL resource with http: which also requires FORM authentication
            // (each of which separately trigger a redirect), we MUST check for
            // confidentiality first, doing that redirect to confidentialPort/Scheme first
            // if necessary.  THEN we check for authentication.  This will preserve the
            // confidentialPort/Scheme when FORM authentication pops back to the resource
            // URL.
                            
            // Does it fail a data constraint
            if (sc.hasDataConstraint())
            {
                HttpConnection connection=request.getHttpConnection();
                HttpListener listener = connection.getListener();
                
                switch(sc.getDataConstraint())
                {
                  case SecurityConstraint.DC_INTEGRAL:
                      if (listener.isIntegral(connection))
                          break;

                      if (listener.getIntegralPort()>0)
                      {
                          String url=listener.getIntegralScheme()+
                              "://"+request.getHost()+
                              ":"+listener.getIntegralPort()+
                              request.getPath();
                          if (request.getQuery()!=null)
                              url+="?"+request.getQuery();
                          response.sendRedirect(url);
                      }
                      else
                          response.sendError(HttpResponse.__403_Forbidden);                   
                      return -1;

                  case SecurityConstraint.DC_CONFIDENTIAL:
                      if (listener.isConfidential(connection))
                          break;

                      if (listener.getConfidentialPort()>0)
                      {
                          String url=listener.getConfidentialScheme()+
                              "://"+request.getHost()+
                              ":"+listener.getConfidentialPort()+
                              request.getPath();
                          if (request.getQuery()!=null)
                              url+="?"+request.getQuery();
                          response.sendRedirect(url);
                      }
                      else
                          response.sendError(HttpResponse.__403_Forbidden);
                      return -1;
                      
                  default:
                      response.sendError(HttpResponse.__403_Forbidden);
                      return -1;
                }
            }
            
			// Does it fail a role check?
			if (sc.isAuthenticate())
			{
				if (realm==null)
				{
					response.sendError(HttpResponse.__500_Internal_Server_Error,
									   "Realm Not Configured");
					return -1;
				}
        
				UserPrincipal user = null;
                
				// Handle pre-authenticated request
				if (request.getAuthType()!=null &&
					request.getAuthUser()!=null)
				{
					user=request.getUserPrincipal();
					if (user==null)
						user=realm.authenticate(request.getAuthUser(),
												null,
												request);
					if (user==null && authenticator!=null)
						user=authenticator.authenticated(realm,
														 pathInContext,
														 request,
														 response);
				}
				else if (authenticator!=null)
				{
					// User authenticator.
					user=authenticator.authenticated(realm,
													 pathInContext,
													 request,
													 response);
				}
				else
				{
					// don't know how authenticate
					Code.warning("Mis-configured Authenticator for "+request.getPath());
					response.sendError(HttpResponse.__500_Internal_Server_Error);
				}
                
				// If we still did not get a user
				if (user==null)
					return -1; // Auth challenge or redirection already sent
				else if (user==__NOBODY)
					return 0; // The Nobody user indicates authentication in transit.
                    
                
				if (!sc.isAnyRole())
				{
					List roles=sc.getRoles();
					boolean inRole=false;
					for (int r=roles.size();r-->0;)
					{
						if (user.isUserInRole(roles.get(r).toString()))
						{
							inRole=true;
							break;
						}
					}
                    
					if (!inRole)
					{
						Code.warning("AUTH FAILURE: role for "+user.getName());
						if ("BASIC".equalsIgnoreCase(authenticator.getAuthMethod()))
							((BasicAuthenticator)authenticator).sendChallenge(realm,response);
						else
							response.sendError(HttpResponse.__403_Forbidden,
											   "User not in required role");
						return -1; // role failed.
					}
				}
			}

            
            // Matches a constraint that does not fail
            // anything, so must be OK
            return 1;
        }

        // Didn't actually match any constraint.
        return 0;
    }
}
