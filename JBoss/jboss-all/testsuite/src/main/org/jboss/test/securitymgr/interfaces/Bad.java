package org.jboss.test.securitymgr.interfaces;

import java.io.IOException;
import java.rmi.RemoteException;
import java.security.Principal;
import javax.ejb.EJBObject;

/**
 */
public interface Bad extends EJBObject
{
    public Principal getSecurityAssociationPrincipal();
    public Object getSecurityAssociationCredential();
    public void setSecurityAssociationPrincipal(Principal user);
    public void setSecurityAssociationCredential(char[] password);
}
