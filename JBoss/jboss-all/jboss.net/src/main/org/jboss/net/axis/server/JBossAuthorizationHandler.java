/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: JBossAuthorizationHandler.java,v 1.1 2002/03/15 10:04:24 cgjung Exp $

package org.jboss.net.axis.server;

import org.apache.axis.AxisFault;
import org.apache.axis.handlers.BasicHandler;
import org.apache.axis.MessageContext;

import org.jboss.security.SimplePrincipal;
import org.jboss.security.AnybodyPrincipal;
import org.jboss.security.NobodyPrincipal;
import org.jboss.security.RealmMapping;

import javax.naming.InitialContext;
import javax.naming.NamingException;

import java.security.Principal;
import javax.security.auth.Subject;

import java.util.StringTokenizer;
import java.util.Set;
import java.util.Iterator;
import java.util.Collection;
import java.util.Collections;

/**
 * AuthorizationHandler that checks allowed and denied roles against the active
 * subject using a given realmMapping. Is somehow redundant to what, e.g., the JBoss EJB invocation handler
 * does, but maybe we need this to shield access to other container resources
 * such as MBeans for which we will expose security-agnostic providers.
 * <br>
 * <h3>Change History</h3>
 * <ul>
 * <li> jung, 15.03.2002: Added security domain option. </li>
 * </ul>
 * <br>
 * <h3>To Do</h3>
 * <ul>
 * <li> jung, 14.03.2002: Cache simple principals. Principal factory for
 * interacting with various security domains.
 * </ul>
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @created 14.03.2002
 * @version $Revision: 1.1 $
 */

public class JBossAuthorizationHandler extends BasicHandler {

   //
   // Attributes
   //

   /** the security domain against which we call */
   protected RealmMapping realmMapping;
   /** the roles that we want to let through */
   final protected Set rolesAllowed = new java.util.HashSet();
   /** the roles that we want to deny access */
   final protected Set rolesDenied = new java.util.HashSet();
   /** whether this handler has been initialized */
   protected boolean isInitialised;

   //
   // Constructors
   //

   public JBossAuthorizationHandler() {
   }

   //
   // Protected helpers
   //

   /** initializes the roles checked by this handler */
   protected void initialise() throws AxisFault {
      // bind against the jboss security subsystem
      isInitialised = true;
      realmMapping = null;
      String securityDomain = (String) getOption(Constants.SECURITY_DOMAIN_OPTION);
      if (securityDomain != null) {
         try {
            realmMapping =
               (RealmMapping) new InitialContext().lookup(securityDomain);
         } catch (NamingException e) {
            throw new AxisFault("Could not lookup security domain " + securityDomain, e);
         }
      }

      // parse role options 
      String allowedRoles = (String) getOption(Constants.ALLOWED_ROLES_OPTION);

      // default:let all through 
      if (allowedRoles == null) {
         allowedRoles = "*";
      }

      StringTokenizer tokenizer = new StringTokenizer(allowedRoles, ",");
      while (tokenizer.hasMoreTokens()) {
         rolesAllowed.add(getPrincipal(tokenizer.nextToken()));
      }

      String deniedRoles = (String) getOption(Constants.DENIED_ROLES_OPTION);
      if (deniedRoles != null) {
         tokenizer = new StringTokenizer(deniedRoles, ",");
         while (tokenizer.hasMoreTokens()) {
            rolesDenied.add(getPrincipal(tokenizer.nextToken()));
         }
      }
   }

   /** 
    * creates a new principal belonging to the given username,
    * override to adapt to specific security domains.
    */
   protected Principal getPrincipal(String userName) {
      if (userName.equals("*")) {
         return AnybodyPrincipal.ANYBODY_PRINCIPAL;
      } else {
         return new SimplePrincipal(userName);
      }
   }

   /** returns a collection of principals that the context subject
    *  is associated with
    */
   protected Collection getAssociatedPrincipals(MessageContext msgContext) {
      // get the active subject
      Subject activeSubject =
         (Subject) msgContext.getProperty(MessageContext.AUTHUSER);
      if (activeSubject == null) {
         return Collections.singleton(NobodyPrincipal.NOBODY_PRINCIPAL);
      } else {
         return activeSubject.getPrincipals();
      }
   }

   /** return whether the given Principal has the given roles */
   protected boolean doesUserHaveRole(Principal principal, Set roles) {
      return realmMapping.doesUserHaveRole(principal, roles);
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

      // initialize the handler
      if (!isInitialised) {
         synchronized (this) {
            if (!isInitialised) {
               initialise();
            }
         }
      }

      // check association
      if (realmMapping == null) {
         throw new AxisFault("No security domain associated.");
      }

      Iterator allPrincipals = getAssociatedPrincipals(msgContext).iterator();
      boolean accessAllowed = false;
      while (allPrincipals.hasNext()) {
         Principal nextPrincipal = (Principal) allPrincipals.next();
         // a single denied is enough to exclude the access
         if (doesUserHaveRole(nextPrincipal, rolesDenied)) {
            accessAllowed = false;
            break;
            // allowed
         } else if (!accessAllowed && doesUserHaveRole(nextPrincipal, rolesAllowed)) {
            accessAllowed = true;
         }
      }

      if (!accessAllowed) {
         throw new AxisFault("Access denied.");
      }
   }
}