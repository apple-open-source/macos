package org.jboss.jetty;

import org.jboss.security.SecurityAssociation;
import org.mortbay.j2ee.J2EEWebApplicationHandler;

public class
  JBossWebApplicationHandler
  extends J2EEWebApplicationHandler
{
  protected void
    disassociateSecurity()
  {
    // overwrite any security context associated with current thread
    SecurityAssociation.setPrincipal(null);
    SecurityAssociation.setCredential(null);
  }
}
