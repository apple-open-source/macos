// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: ContextLoader.java,v 1.16.2.9 2003/06/04 04:47:40 starksm Exp $
// ========================================================================

package org.mortbay.http;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.security.CodeSource;
import java.security.PermissionCollection;
import java.util.Arrays;
import java.util.StringTokenizer;
import org.mortbay.util.Code;
import org.mortbay.util.IO;
import org.mortbay.util.Log;
import org.mortbay.util.Resource;

/* ------------------------------------------------------------ */
/** ClassLoader for HttpContext.
 * Specializes URLClassLoader with some utility and file mapping
 * methods.
 *
 * This loader defaults to the 2.3 servlet spec behaviour where non
 * system classes are loaded from the classpath in preference to the
 * parent loader.  Java2 compliant loading, where the parent loader
 * always has priority, can be selected with the setJava2Complient method.
 *
 * @version $Id: ContextLoader.java,v 1.16.2.9 2003/06/04 04:47:40 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class ContextLoader extends URLClassLoader
{
    private boolean _java2compliant=false;
    private ClassLoader _parent;
    private PermissionCollection _permissions;
    private String _urlClassPath;
    private String _fileClassPath;
    private boolean _fileClassPathWarning=false;
    
    /* ------------------------------------------------------------ */
    /** Constructor.
     * @param classPath Comma separated path of filenames or URLs
     * pointing to directories or jar files. Directories should end
     * with '/'.
     * @exception IOException
     */
    public ContextLoader(HttpContext context,
                         String classPath,
                         ClassLoader parent,
                         PermissionCollection permisions)
        throws MalformedURLException, IOException
    {
        super(new URL[0],parent);
        _permissions=permisions;
        _parent=parent;
        if (_parent==null)
            _parent=getSystemClassLoader();

        if (classPath==null)
        {
            _urlClassPath="";
            _fileClassPath="";
        }
        else
        {
            StringTokenizer tokenizer = new StringTokenizer(classPath,",;");
            
            while (tokenizer.hasMoreTokens())
            {
                Resource resource = Resource.newResource(tokenizer.nextToken());
                Code.debug("Path resource=",resource);
                
                // Resolve file path if possible
                File file=resource.getFile();
                
                if (file!=null)
                {
                    _fileClassPath=(_fileClassPath==null)
                        ?file.getCanonicalPath()
                        :(_fileClassPath+File.pathSeparator+file.getCanonicalPath());    
                    URL url = resource.getURL();
                    addURL(url);
                    _urlClassPath=(_urlClassPath==null)
                        ?url.toString()
                        :(_urlClassPath+","+url.toString());        
                }
                else
                {
                    _fileClassPathWarning=true;
                    
                    // Add resource or expand jar/
                    if (!resource.isDirectory() && file==null)
                    {
                        InputStream in =resource.getInputStream();
                        File lib=new File(context.getTempDirectory(),"lib");
                        if (!lib.exists())
                        {
                            lib.mkdir();
                            lib.deleteOnExit();
                        }
                        File jar=File.createTempFile("Jetty-",".jar",lib);
                        
                        jar.deleteOnExit();
                        Code.debug("Extract ",resource," to ",jar);
                        FileOutputStream out = new FileOutputStream(jar);
                        IO.copy(in,out);
                        out.close();
                        _fileClassPath=(_fileClassPath==null)
                        ?jar.getCanonicalPath()
                            :(_fileClassPath+File.pathSeparator+jar.getCanonicalPath());
                        URL url = jar.toURL();
                        addURL(url);
                        _urlClassPath=(_urlClassPath==null)
                            ?url.toString()
                            :(_urlClassPath+","+url.toString());
                    }
                    else
                    {
                        URL url = resource.getURL();
                        addURL(url);
                        _urlClassPath=(_urlClassPath==null)
                            ?url.toString()
                            :(_urlClassPath+","+url.toString());
                    }
                }
            }
        }
        
        if (Code.debug())
        {
            Code.debug("ClassPath=",_urlClassPath);
            Code.debug("FileClassPath=",_fileClassPath);
            Code.debug("Permissions=",_permissions);
            Code.debug("URL=",Arrays.asList(getURLs()));
        }
    }

    /* ------------------------------------------------------------ */
    /** Set Java2 compliant status.
     * @param compliant 
     */
    public void setJava2Compliant(boolean compliant)
    {
        _java2compliant=compliant;
    }

    /* ------------------------------------------------------------ */
    public boolean isJava2Compliant()
    {
        return _java2compliant;
    }
    
    /* ------------------------------------------------------------ */
    public String getFileClassPath()
    {
        if (_fileClassPathWarning)
        {
            _fileClassPathWarning=false;
            if (_fileClassPath==null)
                Log.event("No File Classpath derived from URL path \""+
                             _urlClassPath+"\"");
            else
                Log.event("Incomplete File Classpath \""+ _fileClassPath+
                             "\" derived from URL path \""+
                             _urlClassPath+"\"");
        }
        
        return _fileClassPath;
    }
    
    /* ------------------------------------------------------------ */
    public String toString()
    {
        return "org.mortbay.http.ContextLoader("+
            _urlClassPath+") / "+_parent.toString();
    }
    
    /* ------------------------------------------------------------ */
    public PermissionCollection getPermissions(CodeSource cs)
    {
        PermissionCollection pc =(_permissions==null)?
            super.getPermissions(cs):_permissions;
        Code.debug("loader.getPermissions("+cs+")="+pc);
        return pc;    
    }
    
    /* ------------------------------------------------------------ */
    public synchronized Class loadClass(String name)
        throws ClassNotFoundException
    {
        return loadClass(name,false);
    }
    
    /* ------------------------------------------------------------ */
    protected synchronized Class loadClass(String name, boolean resolve)
        throws ClassNotFoundException
    {
        Class c = findLoadedClass(name);
        ClassNotFoundException ex=null;
        boolean tried_parent=false;
        if (c==null && (_java2compliant||isSystemPath(name)))
        {
            if (Code.verbose()) Code.debug("try loadClass ",name," from ",_parent);
            tried_parent=true;
            try
            {
                c=_parent.loadClass(name);
                if (Code.verbose()) Code.debug("loaded ",c);
            }
            catch(ClassNotFoundException e){ex=e;}
        }
        
        if (c==null)    
        {
            if (Code.verbose()) Code.debug("try findClass ",name," from ",_urlClassPath);
            try
            {
                c=this.findClass(name);
                if (Code.verbose()) Code.debug("loaded ",c);
            }
            catch(ClassNotFoundException e){ex=e;}
        }
        
        if (c==null && !tried_parent)
        {
            if (Code.verbose()) Code.debug("try loadClass ",name," from ",_parent);
            c=_parent.loadClass(name);
            if (Code.verbose()) Code.debug("loaded ",c);
        }
        
        if (c==null)
            throw ex;
        
        if (resolve)
            resolveClass(c);
        
        return c;
    }
    
    /* ------------------------------------------------------------ */
    public synchronized URL getResource(String name)
    {
        URL url = null;
        boolean tried_parent=false;
        if (_java2compliant||isSystemPath(name) )
        {
            if (Code.verbose()) Code.debug("try getResource ",name," from ",_parent);
            tried_parent=true;
            url=_parent.getResource(name);           
        }
        
        if (url==null)    
        {
            if (Code.verbose()) Code.debug("try findResource ",name," from ",_urlClassPath);
            url=this.findResource(name);

            if (url==null && name.startsWith("/"))
            {
                Code.debug("HACK leading / off ",name);
                url=this.findResource(name.substring(1));
            }
        }
        
        if (url==null && !tried_parent)
        {
            if (Code.verbose()) Code.debug("try getResource ",name," from ",_parent);
            url=_parent.getResource(name); 
        }
        
        if (url!=null && Code.verbose())
            Code.debug("found ",url);
        
        return url;
    }
    
    /* ------------------------------------------------------------ */
    public boolean isSystemPath(String name)
    {
        return (name.startsWith("java.") ||
                name.startsWith("javax.servlet.") ||
                name.startsWith("javax.xml.") ||
                name.startsWith("org.mortbay.") ||
                name.startsWith("org.xml.") ||
                name.startsWith("org.w3c.") ||

                name.startsWith("java/") ||
                name.startsWith("javax/servlet/") ||
                name.startsWith("javax/xml/") ||
                name.startsWith("org/mortbay/") ||
                name.startsWith("org/xml/") ||
                name.startsWith("org/w3c/") ||
                
                name.startsWith("/java/") ||
                name.startsWith("/javax/servlet/") ||
                name.startsWith("/javax/xml/") ||
                name.startsWith("/org/mortbay/") ||
                name.startsWith("/org/xml/") ||
                name.startsWith("/org/w3c/")
                );
    }
}


