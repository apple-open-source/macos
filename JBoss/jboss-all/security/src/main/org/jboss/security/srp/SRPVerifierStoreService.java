/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.srp;

import java.io.File;
import java.io.IOException;
import java.net.URL;
import javax.naming.InitialContext;
import javax.naming.Name;

import org.jboss.naming.NonSerializableFactory;
import org.jboss.security.srp.SerialObjectStore;
import org.jboss.system.ServiceMBeanSupport;

/** The JMX mbean interface for the SRP password verifier store. This
implementation uses the SerialObjectStore as a simple and yet secure
source of usernames and their password verifiers and verifier salts. It
also provides a simple interface for adding and deleting users from the
SerialObjectStore. The mbean stores a non-serializable reference to the
SRPVerifierStore interface in JNDI under the property.

@see org.jboss.security.srp.SerialObjectStore

@author Scott.Stark@jboss.org
@version $Revision: 1.6.4.2 $
*/
public class SRPVerifierStoreService extends ServiceMBeanSupport
   implements SRPVerifierStoreServiceMBean
{
    private SerialObjectStore store;
    private String fileName = "SRPVerifierStore.ser";
    private String jndiName = "srp/DefaultVerifierSource";

// --- Begin SRPVerifierStoreServiceMBean interface methods
   /** Get the jndi name for the SRPVerifierSource implementation binding.
    */
    public String getJndiName()
    {
        return jndiName;
    }
   /** set the jndi name for the SRPVerifierSource implementation binding.
    */
    public void setJndiName(String jndiName)
    {
        this.jndiName = jndiName;
    }
    public void setStoreFile(String fileName) throws IOException
    {
        this.fileName = fileName;
        if( store != null )
        {
            File storeFile = new File(fileName);
            store.save(storeFile);
        }
    }

    public void addUser(String username,String password) throws IOException
    {
        try
        {
            store.addUser(username, password);
            save();
            log.debug("Added username: "+username);
        }
        catch(Exception e)
        {
            log.warn("Failed to addUser, username="+username, e);
        }
    }

    public void delUser(String username) throws IOException
    {
        store.delUser(username);
        log.debug("Added username: "+username);
        save();
    }
// --- End SRPVerifierStoreServiceMBean interface methods

    public String getName()
    {
        return "SRPVerifierStoreService";
    }

    public void initService() throws Exception
    {
    }
    
    public void startService() throws Exception
    {
        File storeFile = new File(fileName);
        store = new SerialObjectStore(storeFile);
        log.info("Created SerialObjectStore at: "+storeFile.getAbsolutePath());
        // Bind a reference to store using NonSerializableFactory as the ObjectFactory
        InitialContext ctx = new InitialContext();
        Name name = ctx.getNameParser("").parse(jndiName);
        NonSerializableFactory.rebind(name, store, true);
    }

    private void save() throws IOException
    {
        if( store != null )
        {   // Try to locate the file on the classpath
            File storeFile = new File(fileName);
            ClassLoader loader = Thread.currentThread().getContextClassLoader();
            URL url = loader.getResource(fileName);
            if( url == null )
            {   // Try to locate the file's parent on the classpath
                String parent = storeFile.getParent();
                if( parent != null )
                {
                    url = loader.getResource(parent);
                    if( url != null )
                    {
                        storeFile = new File(url.getFile(), storeFile.getName());
                    }
                    // else, just go with storeFile as a system file path
                }
            }
            else
            {
                storeFile = new File(url.getFile());
            }
            store.save(storeFile);
        }
    }
}
