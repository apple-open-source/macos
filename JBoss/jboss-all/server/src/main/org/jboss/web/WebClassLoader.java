package org.jboss.web;

import java.net.URL;
import java.net.URLClassLoader;
import javax.management.ObjectName;

import org.jboss.mx.loading.UnifiedClassLoader;
import org.jboss.mx.loading.LoaderRepositoryClassLoader;

/** A simple subclass of URLClassLoader that is used in conjunction with the
the WebService mbean to allow dynamic loading of resources and classes from
deployed ears, ejb jars and wars. A WebClassLoader is associated with a
Container and must have an UnifiedClassLoader as its parent. It overrides the
getURLs() method to return a different set of URLs for remote loading than
what is used for local loading.
<p>
WebClassLoader has two methods meant to be overriden by subclasses: getKey()
and getBytes(). The latter is a no-op in this implementation and should be
overriden by subclasses with bytecode generation ability, such as the
classloader used by the iiop module.
<p>
WebClassLoader subclasses must have a constructor with the same signature
as the WebClassLoader constructor.

@see #getUrls()
@see #setWebURLs(URL[])

@author <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>.
@author Sacha Labourey <sacha.labourey@cogito-info.ch>
@author Vladimir Blagojevic <vladimir@xisnext.2y.net>
@author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
@version $Revision: 1.6.4.2 $
*/
public class WebClassLoader extends LoaderRepositoryClassLoader
{
    /** This WebClassLoader is associated with this container. */
    private ObjectName containerName;

    /** The URLs returned by the getURLs() method override */
   private URL[] webURLs;

   private String codebaseString;

   /** Creates new WebClassLoader.
       Subclasses must have a constructor with the same signature. */
   public WebClassLoader(ObjectName containerName, UnifiedClassLoader parent)
   {
      super(parent, parent.getLoaderRepository());
      this.containerName = containerName;
   }

    /** Gets a string key used as the key into the WebServer's loaderMap. */
    public String getKey()
    {
        String className = getClass().getName();
        int dot = className.lastIndexOf('.');
        if( dot >= 0 )
            className = className.substring(dot+1);
        String key =  className + '[' + hashCode() + ']';
        return key;
    }

    /** Gets the JMX ObjectName of the WebClassLoader's container. */
    public ObjectName getContainer()
    {
        return containerName;
    }

    /** Returns the single URL for my parent, an UnifiedClassLoader. */
    public URL getURL()
    {
        return ((UnifiedClassLoader)getParent()).getURL();
    }

    /** Get the list of URLs that should be used as the RMI annotated codebase.
     This is the URLs previously set via setWebURLs. If setWebURLs has not
     been invoked or was passed in a null value, the super class value of
     getURLs() is used.
    @return the local web URLs if not null, else the value of super.getURLs()
     */
    public URL[] getURLs()
    {
        URL[] urls = webURLs;
        if( urls == null )
            urls = super.getURLs();
        return urls;
    }
    /** Access the URLClassLoader.getURLs() value.
     @return the URLs used for local class and resource loading
     */
    public URL[] getLocalURLs()
    {
        return super.getURLs();
    }

    /** Set the URLs that should be returned from this classes getURLs() override.
     @param webURLs, the set of URL codebases to be used for remote class loading.
     */
    public void setWebURLs(URL[] webURLs)
    {
        this.webURLs = webURLs;
      StringBuffer sb = new StringBuffer();
      for (int i = 0; i < webURLs.length; i++)
      {
         sb.append(webURLs[i].toString());
         if (i < webURLs.length - 1)
         {
            sb.append(" ");
         }
      }
      codebaseString = sb.toString();
    }

   public String getCodebaseString()
   {
      return codebaseString;
   }

    /** Gets the bytecodes for a given class.
     *  This implementation always returns null, indicating that it is unable
     *  to get bytecodes for any class. Should be overridden by subclasses
     *  with bytecode generation capability (such as the classloader used by
     *  the iiop module, which generates IIOP stubs on the fly).
     *
     @param cls a <code>Class</code>
     @return a byte array with the bytecodes for class <code>cls</code>, or
     *       null if this classloader is unable to return such byte array.
     */
    public byte[] getBytes(Class clz)
    {
        return null; // this classloader is unable to return bytecodes
    }

}
