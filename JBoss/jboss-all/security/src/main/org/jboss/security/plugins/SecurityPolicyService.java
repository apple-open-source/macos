/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.security.plugins;

import java.io.FileNotFoundException;
import java.net.URL;
import javax.naming.InitialContext;
import javax.naming.Reference;
import javax.naming.StringRefAddr;
import javax.security.auth.Policy;
import javax.security.auth.login.Configuration;

import org.jboss.naming.NonSerializableFactory;
import org.jboss.security.SecurityPolicy;
import org.jboss.security.SecurityPolicyParser;
import org.jboss.system.ServiceMBeanSupport;

/** The implementation class for the JMX SecurityPolicyServiceMBean. This
service creates a SecurityPolicy instance using a xml based policy store.

@author Scott.Stark@jboss.org
@version $Revision: 1.4 $
*/
public class SecurityPolicyService extends ServiceMBeanSupport implements SecurityPolicyServiceMBean
{
    private String jndiName = "DefaultSecurityPolicy";
    private SecurityPolicy securityPolicy;
    private SecurityPolicyParser policySource;
    private String policyFile;

   /** Get the jndi name under which the SRPServerInterface proxy should be bound
    */
    public String getJndiName()
    {
        return jndiName;
    }
   /** Set the jndi name under which the SRPServerInterface proxy should be bound
    */
    public void setJndiName(String jndiName)
    {
        this.jndiName = jndiName;
    }

    public String getPolicyFile()
    {
        return policyFile;
    }
    public void setPolicyFile(String policyFile)
    {
        this.policyFile = policyFile;
    }

    public String getName()
    {
        return "SecurityPolicyService";
    }

    public void startService() throws Exception
    {
        ClassLoader loader = Thread.currentThread().getContextClassLoader();
        URL policyURL = loader.getResource(policyFile);
        if( policyURL == null )
            throw new FileNotFoundException("Failed to find URL for policy resource: "+policyFile);
        System.out.println("Loading policy file from: "+policyURL);
        policySource = new SecurityPolicyParser(policyURL);
        securityPolicy = new SecurityPolicy(policySource);
        policySource.refresh();

        InitialContext ctx = new InitialContext();
        NonSerializableFactory.rebind(jndiName, securityPolicy);

        // Bind a reference to securityPolicy using NonSerializableFactory as the ObjectFactory
        String className = securityPolicy.getClass().getName();
        String factory = NonSerializableFactory.class.getName();
        StringRefAddr addr = new StringRefAddr("nns", jndiName);
        Reference memoryRef = new Reference(className, addr, factory, null);
        ctx.rebind(jndiName, memoryRef);

        // Install securityPolicy as the JAAS Policy
        Policy.setPolicy(securityPolicy);
        // Install securityPolicy as the JAAS Configuration
        Configuration.setConfiguration(securityPolicy.getLoginConfiguration());
    }

}
