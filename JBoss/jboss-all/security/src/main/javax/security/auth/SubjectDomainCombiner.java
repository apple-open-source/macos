/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.security.auth;

import java.security.DomainCombiner;
import java.security.ProtectionDomain;

/** An alternate implementation of the JAAS 1.0 Configuration class that deals
 * with ClassLoader shortcomings that were fixed in the JAAS included with
 * JDK1.4 and latter. This version allows LoginModules to be loaded from the
 * Thread context ClassLoader and uses an XML based configuration by default.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class SubjectDomainCombiner implements DomainCombiner
{
   private Subject subject;

   public SubjectDomainCombiner(Subject subject)
   {
      this.subject = subject;
   }

   public ProtectionDomain[] combine(ProtectionDomain[] currentDomains,
         ProtectionDomain[] assignedDomains)
   {
      ProtectionDomain[] pd = {};
      return pd;
   }

   public Subject getSubject()
   {
      return subject;
   }
}
