/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.iiop;

import java.net.URL;
import java.util.Collections;
import java.util.Map;
import java.util.WeakHashMap;
import javax.management.ObjectName;

import org.jboss.logging.Logger;
import org.jboss.proxy.compiler.IIOPStubCompiler;
import org.jboss.mx.loading.UnifiedClassLoader;
import org.jboss.web.WebClassLoader;

/**
 * A subclass of WebClassLoader that does IIOP bytecode generation on the fly.
 *
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.5.2.3 $
*/
public class WebCL extends WebClassLoader
{
    /** Logger for trace messages */
    static Logger logger = Logger.getLogger(WebCL.class);

    /** Map from stub classes into bytecode arrays (stub bytecode cache) */
    private Map loadedStubMap = Collections.synchronizedMap(new WeakHashMap());

    /** Creates new WebCL */
    public WebCL(ObjectName container, UnifiedClassLoader parent)
    {
        super(container, parent);
        logger.debug("Constructed WebCL " + this.toString());
        logger.debug("           parent " + parent.toString());

        // Turn standard loading back on (we do classloading)
        standard = true;
    }

    /** Gets a string key used as the key into the WebServer's loaderMap. */
    public String getKey()
    {
        String className = getClass().getName();
        int dot = className.lastIndexOf('.');
        if( dot >= 0 )
            className = className.substring(dot+1);
        String jndiName = getContainer().getKeyProperty("jndiName");
        String key =  className + '[' + jndiName + ']';
        return key;
    }

    /** Gets the bytecodes for a given stub class. */
    public byte[] getBytes(Class clz) {
        byte[] code = (byte[])loadedStubMap.get(clz);
        return (code == null) ? null : (byte[])code.clone();
    }
   
    protected  Class findClass(String name) 
        throws ClassNotFoundException 
    {
        if (logger.isTraceEnabled()) {
            logger.trace("findClass(" + name + ") called");
        }
        if (name.endsWith("_Stub")) {
            int start = name.lastIndexOf('.') + 1;
            if (name.charAt(start) == '_') {
                String pkg = name.substring(0, start);
                String interfaceName = pkg + name.substring(start + 1, 
                                                            name.length() - 5);

                // This is a workaround for a problem in the RMI/IIOP 
                // stub loading code in SUN JDK 1.4.x, which prepends 
                // "org.omg.stub." to classes in certain name spaces, 
                // such as "com.sun". This non-compliant behavior
                // results in failures when deploying SUN example code, 
                // including ECPerf and PetStore, so we remove the prefix.
                if (interfaceName.startsWith("org.omg.stub.com.sun."))
                    interfaceName = interfaceName.substring(13);

                Class intf = super.loadClass(interfaceName);
                if (logger.isTraceEnabled()) {
                    logger.trace("loaded class " + interfaceName);
                }
                
                try {
                    byte[] code = 
                        IIOPStubCompiler.compile(intf, name);
               
                    if (logger.isTraceEnabled()) {
                        logger.trace("compiled stub class for " 
                                     + interfaceName);
                    }
                    Class clz = defineClass(name, code, 0, code.length);
                    if (logger.isTraceEnabled()) {
                        logger.trace("defined stub class for " 
                                     + interfaceName);
                    }
                    resolveClass(clz);
                    try {
                        clz.newInstance();
                    } 
                    catch (Throwable t) {
                        //t.printStackTrace();
                        throw new org.jboss.util.NestedRuntimeException(t);
                    }
                    if (logger.isTraceEnabled()) {
                        logger.trace("resolved stub class for " 
                                     + interfaceName);
                    }
                    loadedStubMap.put(clz, code);
                    return clz;
                }
                catch (RuntimeException e) {
                    logger.error("failed finding class " + name, e);
                    //throw e;
                    return super.findClass(name);
                }
            }
            else {
                return super.findClass(name);
            }
        }
        else {
            return super.findClass(name);
        }
    }

}
