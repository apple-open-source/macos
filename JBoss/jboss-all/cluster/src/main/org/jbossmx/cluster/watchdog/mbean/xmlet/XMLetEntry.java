/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean.xmlet;

import java.io.File;
import java.io.InputStream;
import java.io.IOException;
import java.io.ObjectInputStream;

import java.net.URL;

import java.net.MalformedURLException;

import java.util.LinkedList;
import java.util.List;
import java.util.Properties;

import javax.management.MBeanServer;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.ServiceNotFoundException;

/**
 * @author Stacy Curl
 */
public class XMLetEntry
{
//    /**
//     */
//    public XMLetEntry(final URL documentURL, final Properties properties)
//        throws ClassNotFoundException, IllegalAccessException, InstantiationException
//    {
//        this(documentURL, properties, null);
//    }
//
//    /**
//     */
//    public XMLetEntry(final URL documentURL, final Properties properties,
//        final String packerClassName)
//            throws ClassNotFoundException, IllegalAccessException, InstantiationException
//    {
//        m_documentURL = documentURL;
//        m_properties = properties;
//
//        setupBaseURL();
//
//        initialiseFailedMBeanPacker(packerClassName);
//    }

    /**
     */
    public XMLetEntry(final URL documentURL, final Properties properties,
        final FailedMBeanPacker failedMBeanPacker)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException
    {
        m_documentURL = documentURL;
        m_properties = properties;

        setupBaseURL();

        m_failedMBeanPacker = failedMBeanPacker;
    }

//    private void initialiseFailedMBeanPacker(final String packerClassName)
//        throws ClassNotFoundException, IllegalAccessException, InstantiationException
//    {
//        if (packerClassName != null)
//        {
//            m_failedMBeanPacker = (FailedMBeanPacker) Class.forName(packerClassName).newInstance();
//        }
//        else
//        {
//            m_failedMBeanPacker = new FailedMBeanPacker()
//            {
//                public Object packFailedMBean(final XMLetEntry xmletEntry, Throwable throwable)
//                {
//                    return packFailedMBeanImpl(xmletEntry, throwable);
//                }
//            };
//        }
//    }

    public void addArg(final String type, final String value)
    {
        if (m_types == null)
        {
            m_types = new LinkedList();
        }

        if (m_values == null)
        {
            m_values = new LinkedList();
        }

        m_types.add(type);
        m_values.add(value);
    }

    public Object createMBean(XMLet xmlet) throws ServiceNotFoundException
    {
        Object result;

        String code = getProperty(CODE_ATTRIBUTE);

        if(code.endsWith(".class"))
        {
            code = code.substring(0, code.length() - 6);
        }

        final String name = getProperty(NAME_ATTRIBUTE);
        final URL codebase = getBaseURL();
        final String version = getProperty(VERSION_ATTRIBUTE);
        final String serName = getProperty(OBJECT_ATTRIBUTE);
        final URL documentBase = getDocumentURL();

        xmlet.addArchiveURLS(codebase, getProperty(ARCHIVE_ATTRIBUTE));

        if((code != null) && (serName != null))
        {
            result = packFailedMBean(this,
                new Error(
                    "CODE and OBJECT parameters cannot be specified at the same time in tag MLET"));
        }
        else if((code == null) && (serName == null))
        {
            result = packFailedMBean(this,
                new Error("Either CODE or OBJECT parameter must be specified in tag MLET"));
        }
        else
        {
            try
            {
                result = createObjectInstance(xmlet, name, code, codebase, serName);
            }
            catch(Throwable throwable)
            {
                result = packFailedMBean(this, throwable);
            }
        }

        return result;
    }

    /**
     * @param    name
     * @param    code
     * @param    codebase
     * @param    serName
     * @param    xmletEntry
     *
     * @return
     * @throws Throwable
     */
    protected ObjectInstance createObjectInstance(XMLet xmlet, final String name,
        final String code, final URL codebase, final String serName)
            throws Throwable
    {
//        LOG.debug("createObjectInstance(" + name + ", " + code + ", " + codebase + ", "
//            + serName + ", " + xmletEntry + ")");

        ObjectInstance objectInstance;
        ObjectName objectName = null;

        if(name != null)
        {
            objectName = new ObjectName(name);
        }

        if(code != null)
        {
            Object[] parameters = convertListToArray(getValues());
            Object[] signature = convertListToArray(getTypes());

            for(int pLoop = 0; pLoop < parameters.length; ++pLoop)
            {
                parameters[pLoop] = createObject((String) parameters[pLoop],
                                                 (String) signature[pLoop]);
            }

            if((signature == null) || (signature.length == 0))
            {
                objectInstance = xmlet.getMBeanServer().createMBean(code, objectName, xmlet.getObjectName());
            }
            else
            {
                String[] stringSignature = new String[signature.length];

                for(int sLoop = 0; sLoop < signature.length; ++sLoop)
                {
                    stringSignature[sLoop] = (String) signature[sLoop];
                }

                objectInstance = xmlet.getMBeanServer().createMBean(code, objectName, xmlet.getObjectName(),
                                                           parameters, stringSignature);
            }
        }
        else
        {
            final Object o = loadSerializedObject(xmlet, codebase, serName);

            xmlet.getMBeanServer().registerMBean(o, objectName);

            objectInstance = new ObjectInstance(name, o.getClass().getName());
        }
//
//        LOG.debug("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< createObjectInstance.done");

        return objectInstance;
    }

    /**
     * @param    parameter
     * @param    signature
     *
     * @return
     */
    protected Object createObject(final String parameter, final String signature)
    {
        Object object = null;

        if("java.lang.Boolean".equals(signature) || "boolean".equals(signature))
        {
            object = new Boolean(parameter);
        }
        else if("java.lang.Byte".equals(signature) || "byte".equals(signature))
        {
            object = new Byte(parameter);
        }
        else if("java.lang.Short".equals(signature) || "short".equals(signature))
        {
            object = new Short(parameter);
        }
        else if("java.lang.Long".equals(signature) || "long".equals(signature))
        {
            object = new Long(parameter);
        }
        else if("java.lang.Integer".equals(signature) || "int".equals(signature))
        {
            object = new Integer(parameter);
        }
        else if("java.lang.Float".equals(signature) || "float".equals(signature))
        {
            object = new Float(parameter);
        }
        else if("java.lang.Double".equals(signature) || "double".equals(signature))
        {
            object = new Double(parameter);
        }
        else if("java.lang.String".equals(signature))
        {
            object = parameter;
        }

        return object;
    }

    /**
    * Loads the serialized object specified by the <CODE>OBJECT</CODE>
    * attribute of the <CODE>MLET</CODE> tag.
    *
     * @param    codebase
     * @param    filename
    * @return The serialized object.
    * @exception ClassNotFoundException The specified serialized object could  not be found.
    * @exception IOException An I/O error occurred while loading serialized object.
    */
    protected Object loadSerializedObject(XMLet xmlet, URL codebase, String filename)
        throws IOException, ClassNotFoundException
    {
        if(filename != null)
        {
            filename = filename.replace(File.separatorChar, '/');
        }

        InputStream is = xmlet.getResourceAsStream(filename);

        if(is != null)
        {
            try
            {
                ObjectInputStream ois = new XMLetObjectInputStream(is, xmlet);
                Object serObject = ois.readObject();
                ois.close();

                return serObject;
            }
            catch(IOException e)
            {
                throw e;
            }
            catch(ClassNotFoundException e)
            {
                throw e;
            }
        }
        else
        {
            throw new Error("File " + filename + " containing serialized object not found");
        }
    }

    public Object packFailedMBean(final XMLetEntry xmletEntry, Throwable throwable)
    {
        return m_failedMBeanPacker.packFailedMBean(xmletEntry, throwable);
    }

    private Object packFailedMBeanImpl(final XMLetEntry xmletEntry, Throwable throwable)
    {
        return throwable;
    }

    private static Object[] convertListToArray(List list)
    {
        Object[] array = null;
        if (list != null)
        {
            array = list.toArray();
        }
        else
        {
            array = new Object[0];
        }
        return array;
    }

    public List getTypes()
    {
        return m_types;
    }

    public List getValues()
    {
        return m_values;
    }

    public String getProperty(final String propertyName)
    {
        return m_properties.getProperty(propertyName);
    }

    public URL getDocumentURL()
    {
        return m_documentURL;
    }

    public URL getBaseURL()
    {
        return m_baseURL;
    }

    private void setupBaseURL()
    {
        String codebase = m_properties.getProperty("codebase");

        // Initialize baseURL
        if (codebase != null)
        {
            if (!codebase.endsWith("/")) {
                codebase += "/";
            }
            try
            {
                m_baseURL = new URL(m_documentURL, codebase);
            }
            catch (MalformedURLException e) {}
        }

        if (m_baseURL == null)
        {
            String file = m_documentURL.getFile();
            int i = file.lastIndexOf('/');
            if (i > 0 && i < file.length() - 1)
            {
                try
                {
                    m_baseURL = new URL(m_documentURL, file.substring(0, i + 1));
                }
                catch (MalformedURLException e) {}
            }
        }
        if (m_baseURL == null)
        {
            m_baseURL = m_documentURL;
        }
    }

    /** */
    private List m_types;
    /** */
    private List m_values;
    /** */
    private Properties m_properties;

    /** */
    private URL m_documentURL;
    /** */
    private URL m_baseURL;

    /** */
    public static final String TYPE_ATTRIBUTE = "type";
    public static final String VALUE_ATTRIBUTE = "value";
    public static final String ARCHIVE_ATTRIBUTE = "archive";
    public static final String CODEBASE_ATTRIBUTE = "codebase";
    public static final String OBJECT_ATTRIBUTE = "object";
    public static final String CODE_ATTRIBUTE = "code";
    public static final String NAME_ATTRIBUTE = "name";
    public static final String VERSION_ATTRIBUTE = "version";


    /** */
    FailedMBeanPacker m_failedMBeanPacker;
}
