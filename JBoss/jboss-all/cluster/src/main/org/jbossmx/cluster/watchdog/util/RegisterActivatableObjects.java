/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.util;

// 3rd Party Packages
import org.xml.sax.Attributes;

import org.apache.xerces.parsers.SAXParser;

import org.xml.sax.InputSource;
import org.xml.sax.XMLReader;

import org.xml.sax.helpers.DefaultHandler;

import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;

// Standard Java Packages
import java.io.FileReader;

import java.io.IOException;

import java.lang.reflect.Constructor;

import java.net.MalformedURLException;

import java.rmi.MarshalledObject;
import java.rmi.Naming;
import java.rmi.Remote;
import java.rmi.RemoteException;
import java.rmi.RMISecurityManager;

import java.rmi.activation.Activatable;
import java.rmi.activation.ActivationException;
import java.rmi.activation.ActivationDesc;
import java.rmi.activation.ActivationGroup;
import java.rmi.activation.ActivationGroupID;
import java.rmi.activation.ActivationGroupDesc;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.NoSuchElementException;
import java.util.Properties;
import java.util.StringTokenizer;

/**
 * Class for registering general RMI Activatable Objects via XML.
 *
 * @author Stacy Curl
 *
 * Example xml:
 * <p>
 * <pre>
 * &lt;activation-groups&gt;
 *   &lt;activation-group name="ActiveAgentGroup"
 *                     classpath="hermes-jmx.jar"
 *                     policy="rmid.policy"
 *                     script="/scripts/ActiveAgent"&gt;
 *     &lt;activatable-object name="ActiveAgent"
 *                         class="ActiveAgent"
 *                         codebase="jar:http:blah:8080/hermes-jmx-client.jar!/"
 *                         rmiBinding="ActiveAgent"&gt;
 *       &lt;marshalled-object class="StringInitProperties"&gt;
 *         &lt;constructor-param class="java.lang.String"&gt;
 *           prop1=Value
 *           prop2=value2
 *         &lt;/constructor-param&gt;
 *       &lt;/marshalled-object&gt;
 *     &lt;/activatable-object&gt;
 *   &lt;/activation-group&gt;
 * &lt;/activation-groups&gt;
 *
 * </pre>
 */
public class RegisterActivatableObjects
    extends DefaultHandler
{
    /**
     * Registers all the RMI Activatable Objects specified in an XML config file
     *
     * @param    args the XML config file name
     * @throws IOException
     * @throws SAXException
     */
    public static void main(String[] args) throws SAXException, IOException
    {
        if(System.getSecurityManager() == null)
        {
            System.setSecurityManager(new RMISecurityManager());
        }

        // Check number of args
        if(args.length < 1)
        {
            System.out.println("Usage: RegisterActivatableObjects <filename> [<translation-entry>*]");
            System.out.println("       translation-entry := FROM=TO");
            System.exit(0);
        }

        // Obtain the filename from 'args'
        final String activationConfigFileName = args[0];

        Properties translationTable = new Properties();
        for (int tLoop = 1; tLoop < args.length; ++tLoop)
        {
            final String key_value = args[tLoop].replace('"', ' ').trim();

            try
            {
                StringTokenizer stringTokenizer = new StringTokenizer(key_value, "=");

                final String from = stringTokenizer.nextToken();
                final String to = stringTokenizer.nextToken();

//                System.out.println("added translation, from: \"${" + from + "}\" to: \"" + to + "\"");

                translationTable.setProperty("${" + from + "}", to);
            }
            catch (NoSuchElementException nsee)
            {
                System.out.println("Error, " + key_value + " doesn't look like FROM=TO");
            }
        }

        // Open up the file into a SAXParser
        XMLReader parser = new SAXParser();
        parser.setFeature("http://xml.org/sax/features/validation", false);

        DefaultHandler handler = new RegisterActivatableObjects(translationTable);
        parser.setContentHandler(handler);
        parser.setErrorHandler(handler);

        FileReader reader = new FileReader(activationConfigFileName);

        InputSource input = new InputSource(reader);

        // Use SAX parser, flow of control will now proceed via SAX events to the startDocument,
        // endDocument, startElement, endElement methods.

        parser.parse(input);
    }

    /**
     *  Default Constructor for RegisterActivatableObjects
     */
    public RegisterActivatableObjects(Properties translationTable)
    {
        m_translationTable = translationTable;
    }

    // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< DefaultHandler methods

    /**
     * Called at the beginning of parsing.
     */
    public void startDocument()
    {
        m_accumulator = new StringBuffer();
    }

    /**
     * Called at the end of parsing.
     */
    public void endDocument() {}

    /**
     * When the parser encounters plain text (not XML elements), it calls
     * this method, which accumulates them in a string buffer.
     * Note that this method may be called multiple times, even with no
     * intervening elements.
     *
     * @param    buffer
     * @param    start
     * @param    length
     */
    public void characters(char[] buffer, int start, int length)
    {
        m_accumulator.append(buffer, start, length);
    }

    /**
     * At the beginning of each new element, erase any accumulated text.
     *
     * @param    namespaceURL
     * @param    localName
     * @param    qname
     * @param    attributes
     */
    public void startElement(String namespaceURL, String localName, String qname,
                             Attributes attributes)
    {
        try
        {
            if(localName.equals(ACTIVATION_GROUP))
            {
                setActivationGroupAttributes(attributes);
            }
            else if(localName.equals(ACTIVATABLE_OBJECT))
            {
                setActivatableObjectAttributes(attributes);
            }
            else if(localName.equals(MARSHALLED_OBJECT))
            {
                setMarshalledObjectAttributes(attributes);

                initialiseMarshalledObjectConstructorParameters();
                initialiseMarshalledObjectConstructorSignature();
            }
            else if(localName.equals(CONSTRUCTOR_PARAM))
            {
                setConstructorParameterAttributes(attributes);
            }

            m_accumulator.setLength(0);

        }
        catch(Exception e)
        {
            e.printStackTrace(System.out);
        }
    }

    /**
     * Take special action when we reach the end of selected elements.
     * Although we don't use a validating parser, this method does assume
     * that the web.xml file we're parsing is valid.
     *
     * @param    namespaceURL
     * @param    localName
     * @param    qname
     */
    public void endElement(String namespaceURL, String localName, String qname)
    {
        try
        {
            final String elementText = translateString(m_accumulator.toString().trim());

            if(localName.equals(ACTIVATION_GROUP))
            {
                setCurrentActivationGroupID(null);
            }
            else if(localName.equals(ACTIVATABLE_OBJECT))
            {
                if(m_currentActivationGroupID == null)
                {
                    createActivationGroup();
                }

                addActivatableObject(elementText);
            }
            else if(localName.equals(MARSHALLED_OBJECT))
            {
                createMarshalledObject(elementText);
            }
            else if(localName.equals(CONSTRUCTOR_PARAM))
            {
                addConstructorParam(elementText);
            }
        }
        catch(Exception e)
        {
            e.printStackTrace(System.out);
        }
    }

    /**
     * Issue a warning
     *
     * @param    exception
     */
    public void warning(SAXParseException exception)
    {
        System.err.println("WARNING: line " + exception.getLineNumber() + ": "
                           + exception.getMessage());
    }

    /**
     * Report a parsing error
     *
     * @param    exception
     */
    public void error(SAXParseException exception)
    {
        System.err.println("ERROR: line " + exception.getLineNumber() + ": "
                           + exception.getMessage());
    }

    /**
     * Report a non-recoverable error and exit
     *
     * @param    exception
     * @throws SAXException
     */
    public void fatalError(SAXParseException exception) throws SAXException
    {
        System.err.println("FATAL: line " + exception.getLineNumber() + ": "
                           + exception.getMessage());

        throw (exception);
    }

    // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< DefaultHandler methods

    /**
     * Sets the current ActivationGroupID to a new ActivationGroupID based on the current
     * ActivationGroup Properties.
     */
    private void createActivationGroup()
    {
//        System.out.println("getActivationGroupProperties() = " + getActivationGroupProperties());

        final String nameAttribute = getActivationGroupProperties().getProperty("name");
        final String classPathAttribute = getActivationGroupProperties().getProperty("classpath");
        final String policyAttribute = getActivationGroupProperties().getProperty("policy");
        final String scriptAttribute = getActivationGroupProperties().getProperty("script");

        try
        {
            setCurrentActivationGroupID(createGroup(nameAttribute, classPathAttribute,
                                                    policyAttribute, scriptAttribute));
        }
        catch(Exception e)
        {
            e.printStackTrace(System.out);
        }
    }

    /**
     * XML Routine for adding an ActivatableObject
     *
     * @param    text
     */
    private void addActivatableObject(String text)
    {
        final String nameAttribute = getActivatableObjectProperties().getProperty("name");
        final String classAttribute = getActivatableObjectProperties().getProperty("class");
        final String rmiBindingAttribute = getActivatableObjectProperties()
            .getProperty("rmi-binding");
        final String codebaseAttribute = getActivatableObjectProperties().getProperty("codebase");

//        System.out.println("name = " + nameAttribute);
//        System.out.println("class = " + classAttribute);
//        System.out.println("rmi-binding = " + rmiBindingAttribute);
//        System.out.println("codebase = " + codebaseAttribute);

        ActivationDesc desc = new ActivationDesc(getCurrentActivationGroupID(), classAttribute,
                                                 codebaseAttribute, getCurrentMarshalledObject());

        try
        {
            // Register with rmid
            Remote remote = Activatable.register(desc);
            System.out.println("Got the stub for the " + rmiBindingAttribute);

            // Bind the stub to a name in the registry running on 1099
            Naming.rebind(rmiBindingAttribute, remote);
            System.out.println("Exported " + rmiBindingAttribute);
        }
        catch(ActivationException ae)
        {
            ae.printStackTrace(System.out);
        }
        catch(MalformedURLException mue)
        {
            mue.printStackTrace(System.out);
        }
        catch(RemoteException re)
        {
            re.printStackTrace(System.out);
        }
    }

    /**
     * XML Routine for creating a MarshalledObject
     *
     * @param    text
     */
    private void createMarshalledObject(String text)
    {
        try
        {
            final String marshalledObjectClassName = getMarshalledObjectProperties()
                .getProperty("class");

            final Class marshalledObjectClass = Class.forName(marshalledObjectClassName);

            final Object[] marshalledObjectParams = getMarshalledObjectConstructorParameters()
                .toArray();

            final String[] marshalledObjectConstructorSignature = listToStringArray(
                getMarshalledObjectConstructorSignature());

            final Class[] marshalledObjectSignatureClasses
                = getClassArrayFromStringArray(marshalledObjectConstructorSignature);

            final Constructor marshalledObjectConstructor = marshalledObjectClass
                .getConstructor(marshalledObjectSignatureClasses);

            setCurrentMarshalledObject(new MarshalledObject(marshalledObjectConstructor
                .newInstance(marshalledObjectParams)));
        }
        catch(Exception e)
        {
            e.printStackTrace(System.out);
        }
    }

    /**
     * XML Routine for adding constructor parameters
     *
     * @param    text
     */
    private void addConstructorParam(String text)
    {
        try
        {
            final String classAttribute = getConstructorParameterProperties().getProperty("class");

            final Class paramClass = Class.forName(classAttribute);

            final Class[] paramConstructorParam = new Class[]{ String.class };
            final Object[] paramConstructorValues = new Object[]{ text };

            Constructor paramClassConstructor = paramClass.getConstructor(paramConstructorParam);

            addMarshalledObjectConstructorParameter(paramClassConstructor
                .newInstance(paramConstructorValues), classAttribute);
        }
        catch(Exception e)
        {
            e.printStackTrace(System.out);
        }
    }

    /**
     * Creates an ActivationGroupID
     *
     * @param    groupName the name of the ActivationGroup
     * @param    groupClassPath the classpath of the ActivationGroup
     * @param    groupPolicy the security policy file of the ActivationGroup
     * @param    script the script to use to spawn the ActivationGroup
     *
     * @return an ActivationGroupID
     * @throws Exception
     */
    private static ActivationGroupID createGroup(
        String groupName, String groupClassPath, String groupPolicy, String script) throws Exception
    {
        System.out.println("createGroup(" + groupName + ", " + groupClassPath + ", " + groupPolicy
                           + ")");

        // Because of the 1.2 security model, a security policy should
        // be specified for the ActivationGroup VM. The first argument
        // to the Properties put method, inherited from Hashtable, is
        // the key and the second is the value

        Properties props = new Properties();
        props.put("java.security.policy", groupPolicy);

        ActivationGroupDesc groupDesc = null;

        String[] commandEnvironmentParameters = null;
        //       commandEnvironmentParameters = new String[] {"-cp", groupClassPath};

        if((script == null) || (script.length() == 0))
        {
            System.out.println("Starting without script");

            groupDesc = new ActivationGroupDesc(props,
                                                new ActivationGroupDesc.CommandEnvironment(null,
                                                    commandEnvironmentParameters));
        }
        else
        {
            System.out.println("Starting with script: " + script);

            groupDesc = new ActivationGroupDesc(props,
                                                new ActivationGroupDesc.CommandEnvironment(script,
                                                    commandEnvironmentParameters));
        }

        ActivationGroupID agi = ActivationGroup.getSystem().registerGroup(groupDesc);

        return agi;
    }

    /**
     * Converts a list of Strings to an array of Strings.
     *
     * @param    toConvert list to convert
     *
     * @return a String array containing the elements of <code>toConvert</code>
     */
    private String[] listToStringArray(List toConvert)
    {
//        Would this work ?
//        return (String[]) toConvert.toArray();

        String[] convertedList = new String[toConvert.size()];

        Iterator i = toConvert.iterator();
        int cLoop = 0;
        for(; i.hasNext(); ++cLoop)
        {
            convertedList[cLoop] = (String) i.next();
        }

        return convertedList;
    }

    /**
     * Utility method for obtaining an array of Classes from an array of class names.
     *
     * @param    classNameArray an array of class names
     *
     * @return   an array of Classes
     * @throws ClassNotFoundException
     * @throws NoSuchMethodException
     */
    private static Class[] getClassArrayFromStringArray(final String[] classNameArray)
        throws ClassNotFoundException, NoSuchMethodException
    {
        Class[] classArray = new Class[classNameArray.length];

        for(int sLoop = 0; sLoop < classNameArray.length; ++sLoop)
        {
            classArray[sLoop] = Class.forName(classNameArray[sLoop]);
        }

        return classArray;
    }

    /**
     * Sets the ActivationGroup attributes
     *
     * @param    attributes the ActivationGroup attributes
     */
    private void setActivationGroupAttributes(Attributes attributes)
    {
        setActivationGroupProperties(new Properties());

        addIndividualAttributeToProperties(getActivationGroupProperties(), attributes, "name");
        addIndividualAttributeToProperties(getActivationGroupProperties(), attributes, "classpath");
        addIndividualAttributeToProperties(getActivationGroupProperties(), attributes, "policy");
        addIndividualAttributeToProperties(getActivationGroupProperties(), attributes, "script");
    }

    /**
     * Sets the Activatable object attributes
     *
     * @param    attributes the Activatable object attributes
     */
    private void setActivatableObjectAttributes(Attributes attributes)
    {
        setActivatableObjectProperties(new Properties());

        addIndividualAttributeToProperties(getActivatableObjectProperties(), attributes, "name");
        addIndividualAttributeToProperties(getActivatableObjectProperties(), attributes, "class");
        addIndividualAttributeToProperties(getActivatableObjectProperties(), attributes,
                                           "rmi-binding");
        addIndividualAttributeToProperties(getActivatableObjectProperties(), attributes,
                                           "codebase");

        setCurrentMarshalledObject(null);
    }

    /**
     * Sets the marshalled object attributes
     *
     * @param    attributes the MarshalledObject attributes
     */
    private void setMarshalledObjectAttributes(Attributes attributes)
    {
        setMarshalledObjectProperties(new Properties());

        addIndividualAttributeToProperties(getMarshalledObjectProperties(), attributes, "class");
    }

    /**
     * Sets the constructor parameter attributes
     *
     * @param    attributes the constructor parameter attributes
     */
    private void setConstructorParameterAttributes(Attributes attributes)
    {
        setConstructorParameterProperties(new Properties());

        addIndividualAttributeToProperties(getConstructorParameterProperties(), attributes,
                                           "class");
    }

    /**
     * Obtains an {@link Attribute} from <code>attributes</code> with the name
     * <code>attributeName</code> and stores it in <code>properties</code>
     *
     * @param    properties where to store the new property
     * @param    attributes where to get the attribute from
     * @param    attributeName the name of attribute to store
     */
    private void addIndividualAttributeToProperties(Properties properties, Attributes attributes,
        String attributeName)
    {
        String attributeValue = attributes.getValue(attributeName);

        if(attributeValue == null)
        {
            attributeValue = "";
        }

        properties.setProperty(attributeName, translateString(attributeValue));
    }

    /**
     * Initialises the marshalled object constructor parameters
     */
    private void initialiseMarshalledObjectConstructorParameters()
    {
        m_marshalledObjectConstructorParameters = new ArrayList();
    }

    /**
     * Initialises the marshalled object constructor signature
     */
    private void initialiseMarshalledObjectConstructorSignature()
    {
        m_marshalledObjectConstructorSignature = new ArrayList();
    }

    /**
     * Adds a marshalled object constructor parameter
     *
     * @param    parameter the parameter to add
     * @param    className the class of <code>parameter</code>
     */
    private void addMarshalledObjectConstructorParameter(Object parameter, String className)
    {
        getMarshalledObjectConstructorParameters().add(parameter);
        getMarshalledObjectConstructorSignature().add(className);
    }

    /**
     *
     */
    private String translateString(String string)
    {
        String translatedString = string;

        if (m_translationTable != null &&
            m_translationTable.size() != 0)
        {
            for (Iterator iterator = m_translationTable.keySet().iterator(); iterator.hasNext() && translatedString.indexOf("${") != -1;)
            {
                final String from = (String) iterator.next();
                translatedString = replace(translatedString, from, m_translationTable.getProperty(from));
            }
        }

//        if (!translatedString.equals(string))
//        {
//            System.out.println("translated:\n" + string + "\n to:\n" + translatedString);
//        }

        return translatedString;
    }

    /** Replaces all occurences of 'before' with 'after' in 'source'
      *
      * @param source - The string to search in
      * @param before - The string to search for
      * @param after  - The string to replace 'before' with
      */
    private static String replace( String source, String before, String after ) {


        //System.out.println("StringUtil.replace(" + source + ", " + before + ", " + after + ")");
        String result       = source;

        int    iPreviousPos = 0;
        int    iPos         = source.indexOf( before, iPreviousPos );

        if ( iPos != -1 ) {
            StringBuffer stringBuffer = new StringBuffer();

            while ( iPos != -1 ) {
                stringBuffer.append( source.substring(iPreviousPos, iPos ) );
                stringBuffer.append( after );

                //System.out.println("So far: " + stringBuffer.toString());
                iPreviousPos = iPos + before.length();
                iPos         = source.indexOf( before, iPreviousPos );
            }

            stringBuffer.append( source.substring( iPreviousPos ) );

            result = stringBuffer.toString();
        }

        return result;
    }

    /**
     * Sets the ActivationGroup properties
     *
     * @param    activationGroupProperties the ActivationGroup properties
     */
    private RegisterActivatableObjects setActivationGroupProperties(
        Properties activationGroupProperties)
    {
        m_activationGroupProperties = activationGroupProperties;

        return this;
    }

    /**
     * Gets the ActivationGroup properties
     *
     * @return the ActivationGroup properties
     */
    private Properties getActivationGroupProperties()
    {
        return m_activationGroupProperties;
    }

    /**
     * Sets the ActivatableObject properties
     *
     * @param    activatableObjectProperties the ActivatableObject properties
     */
    private RegisterActivatableObjects setActivatableObjectProperties(
        Properties activatableObjectProperties)
    {
        m_activatableObjectProperties = activatableObjectProperties;

        return this;
    }

    /**
     * Gets the ActivatableObject properties
     *
     * @return the ActivatableObject properties
     */
    private Properties getActivatableObjectProperties()
    {
        return m_activatableObjectProperties;
    }

    /**
     * Sets the MarshalledObject properties
     *
     * @param    marshalledObjectProperties the MarshalledObject properties
     */
    private RegisterActivatableObjects setMarshalledObjectProperties(
        Properties marshalledObjectProperties)
    {
        m_marshalledObjectProperties = marshalledObjectProperties;

        return this;
    }

    /**
     * Gets the MarshalledObject properties
     *
     * @return the MarshalledObject properties
     */
    private Properties getMarshalledObjectProperties()
    {
        return m_marshalledObjectProperties;
    }

    /**
     * Sets the Constructor Parameters properties
     *
     * @param    constructorParameterProperties the Constructor Parameters properties
     */
    private RegisterActivatableObjects setConstructorParameterProperties(
        Properties constructorParameterProperties)
    {
        m_constructorParameterProperties = constructorParameterProperties;

        return this;
    }

    /**
     * Gets the Constructor Parameters properties
     *
     * @return the Constructor Parameters properties
     */
    private Properties getConstructorParameterProperties()
    {
        return m_constructorParameterProperties;
    }

    /**
     * Sets the current ActivationGroupID
     *
     * @param    currentActivationGroupID the current ActivationGroupID
     */
    private RegisterActivatableObjects setCurrentActivationGroupID(
        ActivationGroupID currentActivationGroupID)
    {
        m_currentActivationGroupID = currentActivationGroupID;

        return this;
    }

    /**
     * Gets the current ActivationGroupID
     *
     * @return the current ActivationGroupID
     */
    private ActivationGroupID getCurrentActivationGroupID()
    {
        return m_currentActivationGroupID;
    }

    /**
     * Gets the constructor parameters of the marshalled object
     *
     * @return the constructor parameters of the marshalled object
     */
    private List getMarshalledObjectConstructorParameters()
    {
        return m_marshalledObjectConstructorParameters;
    }

    /**
     * Gets the constructor signature of the marshalled object
     *
     * @return the constructor signature of the marshalled object
     */
    private List getMarshalledObjectConstructorSignature()
    {
        return m_marshalledObjectConstructorSignature;
    }

    /**
     * Sets the current MarshalledObject
     *
     * @param    currentMarshalledObject the current MarshalledObject
     */
    private RegisterActivatableObjects setCurrentMarshalledObject(
        MarshalledObject currentMarshalledObject)
    {
        m_currentMarshalledObject = currentMarshalledObject;

        return this;
    }

    /**
     * Gets the current MarshalledObject
     *
     * @return the current MarshalledObject
     */
    private MarshalledObject getCurrentMarshalledObject()
    {
        return m_currentMarshalledObject;
    }

    /** Buffer for accumulating the XML read */
    private StringBuffer m_accumulator;
    /** ActivationGroup Properties */
    private Properties m_activationGroupProperties;
    /** ActivatatableObject Properties */
    private Properties m_activatableObjectProperties;
    /** MarshalledObject Properties */
    private Properties m_marshalledObjectProperties;
    /** Constructor Parameter Properties */
    private Properties m_constructorParameterProperties;

    /** The current {@link ActivationGroupID} */
    private ActivationGroupID m_currentActivationGroupID;

    /** The parameters that will be used to construct the marshalled object */
    private List m_marshalledObjectConstructorParameters;
    /** The signature of the marshalled object */
    private List m_marshalledObjectConstructorSignature;

    /** The current MarshalledObject */
    private MarshalledObject m_currentMarshalledObject;

    /** The translation table */
    private Properties m_translationTable;

    /** String used in XML Config */
    public static String ACTIVATION_GROUP = "activation-group";
    /** String used in XML Config */
    public static String ACTIVATABLE_OBJECT = "activatable-object";
    /** String used in XML Config */
    public static String MARSHALLED_OBJECT = "marshalled-object";
    /** String used in XML Config */
    public static String CONSTRUCTOR_PARAM = "constructor-param";
}
