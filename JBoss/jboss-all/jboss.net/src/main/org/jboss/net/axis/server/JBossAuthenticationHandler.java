/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: JBossAuthenticationHandler.java,v 1.1.4.2 2003/04/26 09:05:24 cgjung Exp $

package org.jboss.net.axis.server;

import org.apache.axis.AxisFault;
import org.apache.axis.handlers.BasicHandler;
import org.apache.axis.MessageContext;

import org.jboss.security.SimplePrincipal;
import org.jboss.security.NobodyPrincipal;
import org.jboss.security.SecurityAssociation;
import org.jboss.security.SubjectSecurityManager;

import javax.naming.InitialContext;
import javax.naming.NamingException;

import java.security.Principal;
import javax.security.auth.Subject;

/**
 * <p>
 * AuthenticationHandler that interacts with a given JBoss autentication
 * manager via default simple principals and passchars from the 
 * HTTP Basic Authentication. 
 * </p>
 * <p> 
 * It is derived from org.apache.axis.handlers.SimpleAuthenticationHandler.
 * Note that this is somehow redundant to the WebContainer security, but we want
 * to be able to install different authentication schemes for different 
 * web services behind a single entry-point.
 * </p>
 * <p>
 * Per default, unauthenticated calls will be routed by NOT asking the
 * jboss auth manager for validation (most domains still would refuse NobodyPrincipal, 
 * no matter how configured) and building a "null" security association.
 * </p>
 * <p>
 * By specifiying the validateUnauthenticatedCalls=true option, the handler
 * will however try to interface the securityManager with the NobodyPrincipal and
 * an empty password and ask for a proper security association. This is 
 * important when dealing with MS Clients (thanks to John Landers for pointing
 * this out) that won´t send any authentication data if not confronted with
 * an HTTP error on the first try.
 * </p> 
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @since 14.03.2002
 * @version $Revision: 1.1.4.2 $
 * @TODO jung, 14.03.2002: Need principal factory and cache for interacting with various security domains.
 */

public class JBossAuthenticationHandler extends BasicHandler {

   //
   // Attributes
   //

   /** whether this handler has been initialized already */
   protected boolean isInitialised;

   /** whether this handler should let through unauthenticated calls */
   protected boolean shouldValidateUnauthenticatedCalls;

   /** 
    * this is the authentication manager that is responsible for our security domain 
    * if that is null, this authenticationhandler will block any call, rather deactivate
    * the handler, then, or run against a NullSecurityManager
    */
   protected SubjectSecurityManager authMgr;

   //
   // Constructors
   //

   /** default, all options are set afterwards */
   public JBossAuthenticationHandler() {
   }

   //
   // Protected helpers
   //

   /** 
    * initialize this authenticationhandler lazy, after the options have been
    * set.
    */
   protected void initialise() throws AxisFault {
      isInitialised = true;
      authMgr=null;
      shouldValidateUnauthenticatedCalls=false;
      String securityDomain = (String) getOption(Constants.SECURITY_DOMAIN_OPTION);
      if (securityDomain != null) {
         try {
            // bind against the jboss security subsystem
            authMgr =
               (SubjectSecurityManager) new InitialContext().lookup(securityDomain);
         } catch (NamingException e) {
            throw new AxisFault(
               "Could not lookup associated security domain " + securityDomain,
               e);
         }
      }
      String unauthenticatedCalls = (String) getOption(Constants.VALIDATE_UNAUTHENTICATED_CALLS_OPTION);
      if (unauthenticatedCalls != null) {
         try {
            // bind against the jboss security subsystem
            shouldValidateUnauthenticatedCalls=new Boolean(unauthenticatedCalls).booleanValue();
         } catch (Exception e) {
            throw new AxisFault(
               "Could not set validateUnauthenticatedCalls option.",e);
         }
      }
   }

   /** 
    * creates a new principal belonging to the given username,
    * override to adapt to specific security domains.
    */
   protected Principal getPrincipal(String userName) {
      if (userName == null) {
         return NobodyPrincipal.NOBODY_PRINCIPAL;
      } else {
         return new SimplePrincipal(userName);
      }
   }

   /** validates the given principal with the given password */
   protected void validate(Principal userPrincipal, String passwd) throws AxisFault {
      // build passchars
      char[] passChars = passwd != null ? passwd.toCharArray() : null;
      // do the validation only if authenticated or validation enforced
      if(shouldValidateUnauthenticatedCalls || userPrincipal != NobodyPrincipal.NOBODY_PRINCIPAL) {
         // have to use pointer comparison here, but it´s a singleton, right?
         if(!authMgr.isValid(userPrincipal, passChars)) {
            throw new AxisFault( "Server.Unauthenticated", 
            org.apache.axis.utils.Messages.getMessage 
            ("cantAuth01", 
            userPrincipal.getName()), 
            null, null ); 
         }
      }
   }

   /** associates the call context with the given info */
   protected Subject associate(Principal userPrincipal, String passwd) {
      // pointer comparison, again	      
      if (shouldValidateUnauthenticatedCalls || userPrincipal != NobodyPrincipal.NOBODY_PRINCIPAL) {
         SecurityAssociation.setPrincipal(userPrincipal);
         SecurityAssociation.setCredential(passwd!=null ? passwd.toCharArray() : null);
      } else {
         // Jboss security normally does not like nobody:null
         SecurityAssociation.setPrincipal(null);
         SecurityAssociation.setCredential(null);
      }
      return authMgr.getActiveSubject();
   }

   //
   // API
   //

   /**
    * Authenticate the user and password from the msgContext. Note that
    * we do not disassociate the subject here, since that would have
    * to be done by a separate handler in the response chain and we
    * currently expect Jetty or the WebContainer to do that for us
    */

   public void invoke(MessageContext msgContext) throws AxisFault {

      // double check does not work on multiple processors, unfortunately
      if (!isInitialised) {
         synchronized (this) {
            if (!isInitialised) {
               initialise();
            }
         }
      }

      if (authMgr == null) {
         throw new AxisFault("No security domain associated.");
      }

      // we take the id out of the        
      String userID = msgContext.getUsername();
      // convert into a principal
      Principal userPrincipal = getPrincipal(userID);
      // the password that has been provided
      String passwd = msgContext.getPassword();
      // validate the user
      validate(userPrincipal, passwd);
      // associate the context 
      Subject subject = associate(userPrincipal, passwd);
      // with the security subject
      msgContext.setProperty(MessageContext.AUTHUSER, subject);
   }

}