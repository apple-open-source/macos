// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: FileResource.java,v 1.15.2.16 2003/07/11 00:55:02 jules_gosnell Exp $
// ---------------------------------------------------------------------------
package org.mortbay.util;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;
import java.security.Permission;


/* ------------------------------------------------------------ */
/** File Resource.
 *
 * Handle resources of implied or explicit file type.
 * This class can check for aliasing in the filesystem (eg case
 * insensitivity).  By default this is turned on if the platform does
 * not have the "/" path separator, or it can be controlled with the
 * "org.mortbay.util.FileResource.checkAliases" system parameter.
 *
 * If alias checking is turned on, then aliased resources are
 * treated as if they do not exist, nor can they be created.
 *
 * @version $Revision: 1.15.2.16 $
 * @author Greg Wilkins (gregw)
 */
class FileResource extends URLResource
{
    private static boolean __checkAliases;
    static
    {
        __checkAliases=
            "true".equalsIgnoreCase
            (System.getProperty("org.mortbay.util.FileResource.checkAliases","true"));
 
       if (__checkAliases)
            Log.event("Checking Resource aliases");
    }
    
    /* ------------------------------------------------------------ */
    private File _file;
    private transient URL _alias=null;
    private transient boolean _aliasChecked=false;
    
    /* -------------------------------------------------------- */
    FileResource(URL url)
        throws IOException
    {
        super(url,url.openConnection());

        Permission perm = _connection.getPermission();
        _file =new File(perm.getName());
    }
    
    /* -------------------------------------------------------- */
    FileResource(URL url, URLConnection connection, File file)
    {
        super(url,connection);
        _file=file;
    }

    /* -------------------------------------------------------- */
    public Resource addPath(String path)
        throws IOException,MalformedURLException
    {
        if (!isDirectory())
            return super.addPath(path);

        path = URI.canonicalPath(path);
        
        // treat all paths being added as relative
        if (path.startsWith("/"))
            path = path.substring(1);

        File newFile = new File(_file,path);

        if (path.length()>0 && !path.endsWith("/") && newFile.isDirectory())
            path+="/";

        FileResource r=new FileResource(new URL(_url,path),null,newFile);
        return r;
    }
    
    
    
    
    /* ------------------------------------------------------------ */
    /** Get an Alias BadResource if one was created.
     * @return BadResource for alias or null.
     */
    public URL getAlias()
    {
        if (__checkAliases && !_aliasChecked)
        {
            try{
                String abs=_file.getAbsolutePath();
                String can=_file.getCanonicalPath();

                if (!abs.equals(can))
                    _alias=Resource.newResource(can).getURL();
                
                if (_alias!=null && Code.debug())
                {
                    Code.debug("ALIAS abs=",abs);
                    Code.debug("ALIAS can=",can);
                }
            }
            catch(IOException e)
            {
                Code.ignore(e);
            }
        }
        return _alias;
    }
    
    /* -------------------------------------------------------- */
    /**
     * Returns true if the respresenetd resource exists.
     */
    public boolean exists()
    {
        return _file.exists();
    }
        
    /* -------------------------------------------------------- */
    /**
     * Returns the last modified time
     */
    public long lastModified()
    {
        return _file.lastModified();
    }

    /* -------------------------------------------------------- */
    /**
     * Returns true if the respresenetd resource is a container/directory.
     */
    public boolean isDirectory()
    {
        return _file.isDirectory();
    }

    /* --------------------------------------------------------- */
    /**
     * Return the length of the resource
     */
    public long length()
    {
        return _file.length();
    }
        

    /* --------------------------------------------------------- */
    /**
     * Returns the name of the resource
     */
    public String getName()
    {
        return _file.getAbsolutePath();
    }
        
    /* ------------------------------------------------------------ */
    /**
     * Returns an File representing the given resource or NULL if this
     * is not possible.
     */
    public File getFile()
    {
        return _file;
    }
        
    /* --------------------------------------------------------- */
    /**
     * Returns an input stream to the resource
     */
    public InputStream getInputStream() throws IOException
    {
        return new FileInputStream(_file);
    }
        
    /* --------------------------------------------------------- */
    /**
     * Returns an output stream to the resource
     */
    public OutputStream getOutputStream()
        throws java.io.IOException, SecurityException
    {
        return new FileOutputStream(_file);
    }
        
    /* --------------------------------------------------------- */
    /**
     * Deletes the given resource
     */
    public boolean delete()
        throws SecurityException
    {
        return _file.delete();
    }

    /* --------------------------------------------------------- */
    /**
     * Rename the given resource
     */
    public boolean renameTo( Resource dest)
        throws SecurityException
    {
        if( dest instanceof FileResource)
            return _file.renameTo( ((FileResource)dest)._file);
        else
            return false;
    }

    /* --------------------------------------------------------- */
    /**
     * Returns a list of resources contained in the given resource
     */
    public String[] list()
    {
        String[] list =_file.list();
        if (list==null)
            return null;
        for (int i=list.length;i-->0;)
        {
            if (new File(_file,list[i]).isDirectory() &&
                !list[i].endsWith("/"))
                list[i]+="/";
        }
        return list;
    }
        
    /* ------------------------------------------------------------ */
    /** Encode according to this resource type.
     * File URIs are not encoded.
     * @param uri URI to encode.
     * @return The uri unchanged.
     */
    public String encode(String uri)
    {
        return uri;
    }
    
    /* ------------------------------------------------------------ */
    /**
     * @param o
     * @return
     */
    public boolean equals( Object o)
    {
        if (this == o)
            return true;

        if (null == o || ! (o instanceof FileResource))
            return false;

        FileResource f=(FileResource)o;
        return f._file == _file || (null != _file && _file.equals(f._file));
    }

    /* ------------------------------------------------------------ */
    /**
     * @return the hashcode.
     */
    public int hashCode()
    {
       return null == _file ? super.hashCode() : _file.hashCode();
    }
}
