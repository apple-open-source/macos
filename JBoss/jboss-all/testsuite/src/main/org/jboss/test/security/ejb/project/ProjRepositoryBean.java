/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.security.ejb.project;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.Name;
import javax.naming.NamingException;
import javax.naming.directory.Attributes;
import javax.naming.directory.BasicAttributes;
import javax.naming.directory.DirContext;

import org.jboss.test.security.ejb.project.support.HeirMemoryMap;
import org.jboss.test.security.interfaces.IProjRepository;
import org.jboss.test.security.interfaces.ProjRepositoryHome;
import org.jboss.test.security.interfaces.ProjRepository;

/** The ProjRepository session bean implementation. This is a trivial
implementation that always creates the same set of project data.

@see javax.naming.Name
@see javax.naming.directory.Attributes

@author Scott_Stark@displayscape.com
@version $Revision: 1.3 $
*/
public class ProjRepositoryBean implements SessionBean, IProjRepository
{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
    private SessionContext context;
    private HeirMemoryMap projRepository;

    /** @link dependency 
     * @clientRole implementation
     * @supplierRole home*/
    /*#ProjRepositoryHome lnkProjRepositoryHome;*/

    /** @link dependency 
     * @supplierRole remote
     * @clientRole implementation*/
    /*#ProjRepository lnkProjRepository;*/
// --- Begin IProjRepository interface methods
    public void createFolder(Name folderPath) throws NamingException, RemoteException
    {
        throw new RemoteException("Not implemented");
    }

    public void deleteFolder(Name folderPath,boolean recursive) throws NamingException, RemoteException
    {
        throw new RemoteException("Not implemented");
    }

    public void createItem(Name itemPath,Attributes attributes) throws NamingException, RemoteException
    {
        throw new RemoteException("Not implemented");
    }

    public void updateItem(Name itemPath,Attributes attributes) throws NamingException, RemoteException
    {
        throw new RemoteException("Not implemented");
    }

    public void deleteItem(Name itemPath) throws NamingException, RemoteException
    {
        try
        {
            projRepository.unbind(itemPath);
        }
        catch(Exception e)
        {
            log.debug("failed", e);
        }
    }

    public Attributes getItem(Name itemPath) throws NamingException, RemoteException
    {
        log.debug("ProjRepositoryBean.getItem() itemPath="+itemPath);
        Attributes attributes = projRepository.getAttributes(itemPath);
        return attributes;
    }
// --- End IProjRepository interface methods

// --- Begin ProjRepositoryHome methods
    public void ejbCreate(Name projectName) throws CreateException
    {
        log.debug("ProjRepositoryBean.ejbCreate() projectName="+projectName);
        // Add the same data structure to every project
        projRepository = new HeirMemoryMap();
        try
        {
            BasicAttributes attributes = new BasicAttributes();
            attributes.put("name", projectName);
            attributes.put("owner", "scott");
            DirContext projectCtx = projRepository.createSubcontext(projectName, attributes);
            attributes = new BasicAttributes();
            attributes.put("name", "Drawings");
            attributes.put("isFolder", "false");
            attributes.put("contentType", "text/html");
            attributes.put("size", "1024");
            projectCtx.bind("readme.html", null, attributes);
            attributes.put("owner", "scott");
            // Documents subctx
            attributes = new BasicAttributes();
            attributes.put("name", "Documents");
            attributes.put("isFolder", "true");
            attributes.put("owner", "scott");
            DirContext dctx = projectCtx.createSubcontext("Documents", attributes);
            attributes = new BasicAttributes();
            attributes.put("name", "index.html");
            attributes.put("isFolder", "false");
            attributes.put("contentType", "text/html");
            attributes.put("size", "1234");
            dctx.bind("index.html", null, attributes);
            attributes.put("owner", "scott");
            // Documents/Private subctx
            attributes = new BasicAttributes();
            attributes.put("name", "Private");
            attributes.put("isFolder", "true");
            attributes.put("owner", "scott");
            dctx = projectCtx.createSubcontext("Documents/Private", attributes);
            attributes = new BasicAttributes();
            attributes.put("name", "passwords");
            attributes.put("isFolder", "false");
            attributes.put("contentType", "text/plain");
            attributes.put("size", "8173");
            attributes.put("owner", "scott");
            dctx.bind("passwords", null, attributes);
            // Documents/Public subctx
            attributes = new BasicAttributes();
            attributes.put("name", "Public");
            attributes.put("isFolder", "true");
            attributes.put("owner", "scott");
            dctx = projectCtx.createSubcontext("Documents/Public", attributes);
            attributes = new BasicAttributes();
            attributes.put("name", "readme.txt");
            attributes.put("isFolder", "false");
            attributes.put("contentType", "text/plain");
            attributes.put("size", "13584");
            attributes.put("owner", "scott");
            dctx.bind("readme.txt", null, attributes);
            // Documents/Public/starksm subctx
            attributes = new BasicAttributes();
            attributes.put("name", "starksm");
            attributes.put("isFolder", "true");
            attributes.put("owner", "starksm");
            dctx = projectCtx.createSubcontext("Documents/Public/starksm", attributes);
            attributes = new BasicAttributes();
            attributes.put("name", ".bashrc");
            attributes.put("isFolder", "false");
            attributes.put("contentType", "text/plain");
            attributes.put("size", "1167");
            attributes.put("owner", "starksm");
            dctx.bind(".bashrc", null, attributes);
            // Drawing subctx
            attributes = new BasicAttributes();
            attributes.put("name", "Drawings");
            attributes.put("isFolder", "true");
            attributes.put("owner", "scott");
            dctx = projectCtx.createSubcontext("Drawings", attributes);
            attributes = new BasicAttributes();
            attributes.put("name", "view1.jpg");
            attributes.put("isFolder", "false");
            attributes.put("contentType", "image/jpeg");
            attributes.put("owner", "scott");
            dctx.bind("view1.jpg", null, attributes);
        }
        catch(NamingException e)
        {
            throw new CreateException(e.toString(true));
        }
    }

// --- End ProjRepositoryHome methods

// --- Begin SessionBean interface methods
    public void setSessionContext(SessionContext context)
    {
        this.context = context;
    }
    
    public void ejbRemove()
    {
    }

    public void ejbActivate()
    {
    }
    
    public void ejbPassivate()
    {
    }
// --- End SessionBean interface methods
}
