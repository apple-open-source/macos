// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: XmlConfiguration.java,v 1.15.2.6 2003/07/11 00:55:01 jules_gosnell Exp $
// ========================================================================

package org.mortbay.xml;

import java.io.IOException;
import java.io.StringReader;
import java.lang.reflect.Array;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.net.InetAddress;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.UnknownHostException;
import java.util.Map;
import org.mortbay.util.Code;
import org.mortbay.util.InetAddrPort;
import org.mortbay.util.Loader;
import org.mortbay.util.TypeUtil;
import org.mortbay.util.Resource;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;


/* ------------------------------------------------------------ */
/** Configure Objects from XML.
 * This class reads an XML file conforming to the configure.dtd DTD
 * and uses it to configure and object by calling set, put or other
 * methods on the object.
 *
 * @version $Id: XmlConfiguration.java,v 1.15.2.6 2003/07/11 00:55:01 jules_gosnell Exp $
 * @author Greg Wilkins (gregw)
 */
public class XmlConfiguration
{
    private static Class[] __primitives =
    {
        Boolean.TYPE, Character.TYPE, Byte.TYPE, Short.TYPE, Integer.TYPE,
        Long.TYPE, Float.TYPE, Double.TYPE, Void.TYPE
    };
    
    private static Class[] __primitiveHolders =
    {
        Boolean.class, Character.class, Byte.class, Short.class, Integer.class,
        Long.class, Float.class, Double.class, Void.class
    };
    
    /* ------------------------------------------------------------ */
    private static XmlParser __parser;
    private XmlParser.Node _config;

    /* ------------------------------------------------------------ */
    private synchronized static void initParser()
        throws IOException
    {
        if (__parser!=null)
            return;
        
        __parser = new XmlParser();
        Resource config12Resource=Resource.newSystemResource("org/mortbay/xml/configure_1_2.dtd");
        Code.assertTrue(config12Resource.exists(),
                        "org/mortbay/xml/configure_1_2.dtd");
        __parser.redirectEntity
            ("configure.dtd",config12Resource);
        __parser.redirectEntity
            ("configure_1_2.dtd",
                     config12Resource);
        __parser.redirectEntity
            ("http://jetty.mortbay.org/configure_1_2.dtd",
             config12Resource);
        __parser.redirectEntity
            ("-//Mort Bay Consulting//DTD Configure 1.2//EN",
             config12Resource);
        
        Resource config11Resource=Resource.newSystemResource("org/mortbay/xml/configure_1_1.dtd");
        Code.assertTrue(config11Resource.exists(),
                        "org/mortbay/xml/configure_1_1.dtd");
        __parser.redirectEntity
            ("configure_1_1.dtd",
             config11Resource);
        __parser.redirectEntity
            ("http://jetty.mortbay.org/configure_1_1.dtd",
             config11Resource);
        __parser.redirectEntity
            ("-//Mort Bay Consulting//DTD Configure 1.1//EN",
             config11Resource);
        
        Resource config10Resource=Resource.newSystemResource("org/mortbay/xml/configure_1_0.dtd");  
        Code.assertTrue(config10Resource.exists(),
                        "org/mortbay/xml/configure_1_0.dtd");  
        __parser.redirectEntity
            ("configure_1_0.dtd",
             config10Resource);
        __parser.redirectEntity
            ("http://jetty.mortbay.org/configure_1_0.dtd",
             config10Resource);
        __parser.redirectEntity
            ("-//Mort Bay Consulting//DTD Configure 1.0//EN",
             config10Resource);
    }
    
    /* ------------------------------------------------------------ */
    /** Constructor.
     * Reads the XML configuration file.
     * @param configuration 
     */
    public XmlConfiguration(URL configuration)
        throws SAXException, IOException
    {
        initParser();
        synchronized(__parser)
        {
            _config = __parser.parse(configuration.toString());	
        }
    }
    

    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param configuration String of XML configuration commands
     * excluding the normal XML preamble. The String should start with
     * a "<Configure ...." element.
     * @exception SAXException 
     * @exception IOException 
     */
    public XmlConfiguration(String configuration)
        throws SAXException, IOException
    {
        initParser();
        configuration="<?xml version=\"1.0\"  encoding=\"ISO-8859-1\"?>\n<!DOCTYPE Configure PUBLIC \"-//Mort Bay Consulting//DTD Configure 1.2//EN\" \"http://jetty.mortbay.org/configure_1_2.dtd\">"+
            configuration;
        InputSource source = new InputSource(new StringReader(configuration));
        synchronized(__parser)
        {
            _config = __parser.parse(source);	
        }
    }

    /* ------------------------------------------------------------ */
    /** Configure an object.
     * If the object is of the approprate class, the XML configuration
     * script is applied to the object.
     * @param obj The object to be configured.
     * @exception ClassNotFoundException 
     * @exception NoSuchMethodException 
     * @exception InvocationTargetException 
     */
    public void configure(Object obj)
        throws ClassNotFoundException,
               NoSuchMethodException,
               InvocationTargetException,
               IllegalAccessException
    {
        //Check the class of the object
        Class oClass = nodeClass(_config);
        if (!oClass.isInstance(obj))
            throw new IllegalArgumentException("Object is not of type "+oClass);
        configure(obj,_config,0);
    }
    
    /* ------------------------------------------------------------ */
    /** Create a new object and configure it.
     * A new object is created and configured.
     * @return The newly created configured object.
     * @exception ClassNotFoundException 
     * @exception NoSuchMethodException 
     * @exception InvocationTargetException 
     * @exception InstantiationException 
     * @exception IllegalAccessException 
     */
    public Object newInstance()
        throws ClassNotFoundException,
               NoSuchMethodException,
               InvocationTargetException,
               InstantiationException,
               IllegalAccessException
    {
        Class oClass = nodeClass(_config);
        Object obj = oClass.newInstance();
        configure(obj,_config,0);
        return obj;
    }
    
    
    /* ------------------------------------------------------------ */
    private Class nodeClass(XmlParser.Node node)
        throws ClassNotFoundException
    {
        String className=node.getAttribute("class");
        if (className==null)
            return null;
        
        return Loader.loadClass(XmlConfiguration.class,className);
    }
    
    /* ------------------------------------------------------------ */
    /* Recursive configuration step.
     * This method applies the remaining Set, Put and Call elements
     * to the current object.
     * @param obj 
     * @param cfg 
     * @param i 
     * @exception ClassNotFoundException 
     * @exception NoSuchMethodException 
     * @exception InvocationTargetException 
     */
    private void configure(Object obj,XmlParser.Node cfg,int i)
        throws ClassNotFoundException,
               NoSuchMethodException,
               InvocationTargetException,
               IllegalAccessException
    {
        for(;i<cfg.size();i++)
        {  
            Object o = cfg.get(i);
            if (o instanceof String)
                continue;
            XmlParser.Node node = (XmlParser.Node)o;
            
            String tag=node.getTag();
            if("Set".equals(tag))
                set(obj,node);
            else if("Put".equals(tag))
                put(obj,node);
            else if("Call".equals(tag))
                call(obj,node);
            else if("Get".equals(tag))
                get(obj,node);
            else
                throw new IllegalStateException("Unknown tag: "+tag);
        }
    }
    
    /* ------------------------------------------------------------ */
    /* Call a set method.
     * This method makes a best effort to find a matching set method.
     * The type of the value is used to find a suitable set method by
     * 1. Trying for a trivial type match.
     * 2. Looking for a native type match.
     * 3. Trying all correctly named methods for an auto conversion.
     * 4. Attempting to construct a suitable value from original value.
     * @param obj 
     * @param node 
     */
    private void set(Object obj,XmlParser.Node node)
        throws ClassNotFoundException,
               NoSuchMethodException,
               InvocationTargetException,
               IllegalAccessException
    {
        String attr=node.getAttribute("name");
        String name = "set"+attr.substring(0,1).toUpperCase()+attr.substring(1);
        Object value= value(obj,node);
        Object[] arg={value};
        
        Class oClass = nodeClass(node);
        if (oClass!=null)
            obj=null;
        else
            oClass = obj.getClass();
        
        Class[] vClass = {Object.class};
        if (value!=null)
            vClass[0]=value.getClass();

        Code.debug(obj,".",name,"(",vClass[0]," ",value,")");
        
        // Try for trivial match
        try{
            Method set = oClass.getMethod(name,vClass);
            set.invoke(obj,arg);
            return;
        }
        catch(IllegalArgumentException e)
        {if(Code.verbose(Integer.MAX_VALUE))Code.ignore(e);}
        catch(IllegalAccessException e)
        {if(Code.verbose(Integer.MAX_VALUE))Code.ignore(e);}
        catch(NoSuchMethodException e)
        {if(Code.verbose(Integer.MAX_VALUE))Code.ignore(e);}
        
        // Try for native match
        try{
            Field type = vClass[0].getField("TYPE");
            vClass[0]=(Class)type.get(null);
            Method set = oClass.getMethod(name,vClass);
            set.invoke(obj,arg);
            return;
        }
        catch(NoSuchFieldException e)
        {if(Code.verbose(Integer.MAX_VALUE))Code.ignore(e);}
        catch(IllegalArgumentException e)
        {if(Code.verbose(Integer.MAX_VALUE))Code.ignore(e);}
        catch(IllegalAccessException e)
        {if(Code.verbose(Integer.MAX_VALUE))Code.ignore(e);}
        catch(NoSuchMethodException e)
        {if(Code.verbose(Integer.MAX_VALUE))Code.ignore(e);}


        // Try a field
        try
        {
            Field field=oClass.getField(attr);
            if (Modifier.isPublic(field.getModifiers()))
            {
                field.set(obj,value);
                return;
            }
        }
        catch (NoSuchFieldException e)
        {if(Code.verbose(Integer.MAX_VALUE))Code.ignore(e);}

        // Search for a match by trying all the set methods
        Method[] sets = oClass.getMethods();
        Method set=null;
        for (int s=0;sets!=null && s<sets.length;s++)
        {
            if (name.equals(sets[s].getName()) &&
                sets[s].getParameterTypes().length==1)
            {
                // lets try it
                try
                {
                    set=sets[s];
                    sets[s].invoke(obj,arg);
                    return;
                }
                catch(IllegalArgumentException e){if(Code.verbose(Integer.MAX_VALUE))Code.ignore(e);}
                catch(IllegalAccessException e){if(Code.verbose(Integer.MAX_VALUE))Code.ignore(e);}
            }
        }

        // Try converting the arg to the last set found.
        if (set!=null)
        {
            try
            {
                Class sClass=set.getParameterTypes()[0];
                if (sClass.isPrimitive())
                {
                    for (int t=0;t<__primitives.length;t++)
                    {
                        if (sClass.equals(__primitives[t]))
                        {
                            sClass=__primitiveHolders[t];
                            break;
                        }
                    }
                }
                Constructor cons = sClass.getConstructor(vClass);
                arg[0]=cons.newInstance(arg);
                set.invoke(obj,arg);
                return;
            }
            catch(NoSuchMethodException e)
            {if(Code.verbose(Integer.MAX_VALUE))Code.ignore(e);}
            catch(IllegalAccessException e)
            {if(Code.verbose(Integer.MAX_VALUE))Code.ignore(e);}
            catch(InstantiationException e)
            {if(Code.verbose(Integer.MAX_VALUE))Code.ignore(e);}
        }

        // No Joy
        throw new NoSuchMethodException(oClass+"."+name+"("+vClass[0]+")");
    }
    
    /* ------------------------------------------------------------ */
    /* Call a put method.
     * 
     * @param obj 
     * @param node 
     */
    private void put(Object obj,XmlParser.Node node)
        throws NoSuchMethodException,
               ClassNotFoundException,
               InvocationTargetException,
               IllegalAccessException
    {
        if (!(obj instanceof Map))
            throw new IllegalArgumentException("Object for put is not a Map: "+
                                               obj);
        Map map = (Map) obj;
        
        String name = node.getAttribute("name");
        Object value= value(obj,node);
        map.put(name,value);
        Code.debug(obj+".put("+name+","+value+")");
    }
    
    
    
    /* ------------------------------------------------------------ */
    /* Call a get method.
     * Any object returned from the call is passed to the configure
     * method to consume the remaining elements.
     * @param obj 
     * @param node 
     * @return 
     * @exception NoSuchMethodException 
     * @exception ClassNotFoundException 
     * @exception InvocationTargetException 
     */
    private Object get(Object obj,XmlParser.Node node)
        throws NoSuchMethodException,
               ClassNotFoundException,
               InvocationTargetException,
               IllegalAccessException
    {
        Class oClass = nodeClass(node);
        if (oClass!=null)
            obj=null;
        else
            oClass = obj.getClass();        
        
        String name=node.getAttribute("name");
        Code.debug("get ",name);
        
        try
        {
            // try calling a getXxx method.
            Method method = oClass.getMethod("get"+
                                             name.substring(0,1).toUpperCase()+
                                             name.substring(1),
                                             null);
            obj=method.invoke(obj,null);
            configure(obj,node,0);
        }
        catch (NoSuchMethodException nsme)
        {
            try
            {
                Field field = oClass.getField(name);
                obj=field.get(obj);
                configure(obj,node,0);
            }
            catch(NoSuchFieldException nsfe)
            {
                throw nsme;
            }
        }
        return obj;
    }
    
    /* ------------------------------------------------------------ */
    /* Call a method.
     * A method is selected by trying all methods with matching
     * names and number of arguments.
     * Any object returned from the call is passed to the configure
     * method to consume the remaining elements.
     * Note that if this is a static call we consider only methods
     * declared directly in the given class. i.e. we ignore any static
     * methods in superclasses.
     * @param obj 
     * @param node 
     * @return 
     * @exception NoSuchMethodException 
     * @exception ClassNotFoundException 
     * @exception InvocationTargetException 
     */
    private Object call(Object obj,XmlParser.Node node)
        throws NoSuchMethodException,
               ClassNotFoundException,
               InvocationTargetException,
               IllegalAccessException
    {
        Class oClass = nodeClass(node);
        if (oClass!=null)
            obj=null;
        else
            oClass = obj.getClass();
        
        int size=0;
        int argi=node.size();
        for (int i=0;i<node.size();i++)
        {
            Object o = node.get(i);
            if (o instanceof String)
                continue;
            if(!((XmlParser.Node)o).getTag().equals("Arg"))
            {
                argi=i;
                break;
            }
            size++;
        }

        Object[] arg = new Object[size];
        for (int i=0,j=0;j<size;i++)
        {
            Object o = node.get(i);
            if (o instanceof String)
                continue;
            arg[j++]=value(obj,(XmlParser.Node)o);
        }
        
        String method=node.getAttribute("name");
        Code.debug("call ",method);
        
        // Lets just try all methods for now
        Method[] methods = oClass.getMethods();
        for (int c=0;methods!=null && c<methods.length;c++)
        {            
            if (!methods[c].getName().equals(method))
                continue;
            if (methods[c].getParameterTypes().length!=size)
                continue;
            if (Modifier.isStatic(methods[c].getModifiers())!=(obj==null))
                continue;
	    if ((obj == null) && methods[c].getDeclaringClass()!=oClass)
		continue;
            
            Object n=null;
            boolean called=false;
            try{n=methods[c].invoke(obj,arg);called=true;}
            catch(IllegalAccessException e)
            {if(Code.verbose(Integer.MAX_VALUE))Code.ignore(e);}
            catch(IllegalArgumentException e)
            {if(Code.verbose(Integer.MAX_VALUE))Code.ignore(e);}
            if (called)
            {
                configure(n,node,argi);
                return n;
            }
        }

        throw new IllegalStateException("No Method: "+node+" on "+oClass);
    }
    
    /* ------------------------------------------------------------ */
    /* Create a new value object.
     *
     * @param obj 
     * @param node 
     * @return 
     * @exception NoSuchMethodException 
     * @exception ClassNotFoundException 
     * @exception InvocationTargetException 
     */
    private Object newObj(Object obj,XmlParser.Node node)
        throws NoSuchMethodException,
               ClassNotFoundException,
               InvocationTargetException,
               IllegalAccessException
    {
        Class oClass = nodeClass(node);
        int size=0;
        int argi=node.size();
        for (int i=0;i<node.size();i++)
        {
            Object o = node.get(i);
            if (o instanceof String)
                continue;
            if(!((XmlParser.Node)o).getTag().equals("Arg"))
            {
                argi=i;
                break;
            }
            size++;
        }

        Object[] arg = new Object[size];
        for (int i=0,j=0;j<size;i++)
        {
            Object o = node.get(i);
            if (o instanceof String)
                continue;
            arg[j++]=value(obj,(XmlParser.Node)o);
        }

        Code.debug("new ",oClass);
        
        // Lets just try all constructors for now
        Constructor[] constructors = oClass.getConstructors();
        for (int c=0;constructors!=null && c<constructors.length;c++)
        {
            if (constructors[c].getParameterTypes().length!=size)
                continue;

            Object n=null;
            boolean called=false;
            try{
                n=constructors[c].newInstance(arg);
                called=true;
            }
            catch(IllegalAccessException e)
            {if(Code.verbose(Integer.MAX_VALUE))Code.ignore(e);}
            catch(InstantiationException e)
            {if(Code.verbose(Integer.MAX_VALUE))Code.ignore(e);}
            catch(IllegalArgumentException e)
            {if(Code.verbose(Integer.MAX_VALUE))Code.ignore(e);}
            if(called)
            {
                configure(n,node,argi);
                return n;
            }
        }

        throw new IllegalStateException("No Constructor: "+node+" on "+obj);
    }
    
    
    /* ------------------------------------------------------------ */
    /* Create a new value object.
     *
     * @param obj 
     * @param node 
     * @return 
     * @exception NoSuchMethodException 
     * @exception ClassNotFoundException 
     * @exception InvocationTargetException 
     */
    private Object newArray(Object obj,XmlParser.Node node)
        throws NoSuchMethodException,
               ClassNotFoundException,
               InvocationTargetException,
               IllegalAccessException
    {
        // Get the type
        Class aClass = java.lang.Object.class;
        String type = node.getAttribute("type");
        if (type!=null)
        {
            aClass=TypeUtil.fromName(type);
            if (aClass==null)
            {
                if ("String".equals(type))
                    aClass=java.lang.String.class;
                else if ("URL".equals(type))
                    aClass=java.net.URL.class;
                else if ("InetAddress".equals(type))
                    aClass=java.net.InetAddress.class;
                else if ("InetAddrPort".equals(type))
                    aClass=org.mortbay.util.InetAddrPort.class;
                else
                    aClass=Loader.loadClass(XmlConfiguration.class,type);
            }
        }

        Object array = Array.newInstance(aClass,node.size());

        for (int i=0;i<node.size();i++)
        {
            Object o = node.get(i);
            if (o instanceof String)
                continue;
            XmlParser.Node item = (XmlParser.Node)o;
            if (!item.getTag().equals("Item"))
                throw new IllegalStateException("Not an Item");
            Object v=value(obj,item);
            if (v!=null)
                Array.set(array,i,v);
        }
        
        return array;
    }
    
    
    /* ------------------------------------------------------------ */
    /* Get the value of an element.
     * If no value type is specified, then white space is trimmed out of the
     * value. If it contains multiple value elements they are added as
     * strings before being converted to any specified type.
     * @param node 
     */
    private Object value(Object obj,XmlParser.Node node)
        throws NoSuchMethodException,
               ClassNotFoundException,
               InvocationTargetException,
               IllegalAccessException
    {
        // Get the type
        String type = node.getAttribute("type");
        
        // handle trivial case
        if (node.size()==0)
        {
            if ("String".equals(type))
                return "";
            return null;
        }

        // Trim values
        int first=0;
        int last=node.size()-1;
            
        // Handle default trim type
        if (type==null || !"String".equals(type))
        {
            // Skip leading white
            Object item=null;
            while(first<=last )
            {
                item=node.get(first);
                if (!(item instanceof String))
                    break;
                item=((String)item).trim();
                if (((String)item).length()>0)
                    break;
                first++;
            }

            // Skip trailing white
            while(first<last)
            {
                item=node.get(last);
                if (!(item instanceof String))
                    break;
                item=((String)item).trim();
                if (((String)item).length()>0)
                    break;
                last--;
            }

            // All white, so return null
            if (first>last)
                return null;
        }

        Object value=null;
        
        if (first==last)
            //  Single Item value
            value=itemValue(obj,node.get(first));
        else
        {
            // Get the multiple items as a single string
            StringBuffer buf = new StringBuffer();
            synchronized(buf)
            {
                for (int i=first;i<=last;i++)
                {
                    Object item = node.get(i);
                    buf.append(itemValue(obj,item));
                }
                value=buf.toString();
            }
        }

        // Untyped or unknown
        if (value==null )
        {
            if ("String".equals(type))
                return "";
            return null;
        }

        
        // Try to type the object
        if (type==null)
        {
            if (value !=null && value instanceof String)
                return ((String)value).trim();
            return value;
        }

        if ("String".equals(type) || "java.lang.String".equals(type))
            return value.toString();

        Class pClass = TypeUtil.fromName(type);
        if (pClass!=null)
            return TypeUtil.valueOf(pClass,value.toString());
        
        if ("URL".equals(type) || "java.net.URL".equals(type))
        {
            if (value instanceof URL)
                return value;
            try{return new URL(value.toString());}
            catch(MalformedURLException e)
            {throw new InvocationTargetException(e);}
        }
        
        if ("InetAddress".equals(type)|| "java.net.InetAddress".equals(type))
        {
            if (value instanceof InetAddress)
                return value;
            try {return InetAddress.getByName(value.toString());}
            catch(UnknownHostException e)
            {throw new InvocationTargetException(e);}
        }
        
        if ("InetAddrPort".equals(type) || "org.mortbay.util.InetAddrPort".equals(type))
        {
            if (value instanceof InetAddrPort)
                return value;
            try{return new InetAddrPort(value.toString());}
            catch(UnknownHostException e)
            {throw new InvocationTargetException(e);}
        }

        throw new IllegalStateException("Unknown type "+type);
    }
    
    /* ------------------------------------------------------------ */
    /* Get the value of a single element.
     * @param obj 
     * @param item 
     * @return 
     * @exception ClassNotFoundException 
     */
    private Object itemValue(Object obj, Object item)
        throws NoSuchMethodException,
               ClassNotFoundException,
               InvocationTargetException,
               IllegalAccessException
    {
        // String value
        if (item instanceof String)
            return item;
        
        XmlParser.Node node=(XmlParser.Node)item;
        String tag=node.getTag();
        if ("Call".equals(tag))
            return call(obj,node);
        if ("Get".equals(tag))
            return get(obj,node);
        if ("New".equals(tag))
            return newObj(obj,node);
        if ("Array".equals(tag))
            return newArray(obj,node);
        
        if ("SystemProperty".equals(tag))
        {
            String name=node.getAttribute("name");
            String defaultValue=node.getAttribute("default");
            return System.getProperty(name, defaultValue);
        }
        
        Code.warning("Unknown value tag: "+node,new Throwable());
        return null;
    }    
    
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    public static void main(String[] arg)
    {
        try
        {
            for (int i=0;i<arg.length;i++)
                new XmlConfiguration
                    (Resource.newResource(arg[i]).getURL()).newInstance();
        }
        catch (Exception e)
        {
            Code.warning(e);
        }
    }
}

