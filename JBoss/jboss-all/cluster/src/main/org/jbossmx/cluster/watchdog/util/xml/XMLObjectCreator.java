/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.util.xml;

// 3rd Party Packages
import org.apache.xerces.parsers.DOMParser;

import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

// Standard Java Packages
import java.lang.reflect.Array;
import java.lang.reflect.Constructor;

import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

/**
 * XMLObjectCreator is a utility class for creating Java objects from XML
 * <p>
 * Examples of XML:<pre>
 *   Example 1:
 *   &lt;param class="java.lang.String"&gt;Hello&lt;/param&gt;
 *
 *   Example 2:
 *   &lt;param class="java.lang.Boolean"&gt;true&lt;/param&gt;
 *
 *   Example 3:
 *   &lt;param class="com.company.Rational"&gt;
 *     &lt;param class="java.lang.Integer"&gt;3&lt;/param&gt;
 *     &lt;param class="java.lang.Integer"&gt;4&lt;/param&gt;
 *   &lt;/param&gt;
 *
 *   Example 4:
 *   &lt;param class="com.company.Thing[]"&gt;
 *     &lt;param class="com.company.Thing"&gt;
 *       &lt;param class="java.lang.Integer"&gt;1&lt;/param&gt;
 *       &lt;param class="java.lang.Integer"&gt;2&lt;/param&gt;
 *       &lt;param class="java.lang.Integer"&gt;3&lt;/param&gt;
 *     &lt;/param&gt;
 *   &lt;/param&gt;
 *
 * Note that the following fragment
 *
 *   &lt;param class="java.lang.Thing"&gt;
 *     &lt;param class="java.lang.String"&gt;Value&lt;/param
 *   &lt;/param&gt;
 *
 * can be simplified to:
 *   &lt;param class="java.lang.Thing"&gt;Value&lt;/param&gt;</pre>
 */
public class XMLObjectCreator
{
    /**
     * XMLObjectCreator doesn't need to be created.
     */
    private XMLObjectCreator() {}

    /**
     * Creates an array of Objects from the list of XML Nodes
     *
     * @param    nodeList the list of XML Nodes
     *
     * @return an array of Objects
     * @throws Exception
     */
    public static Object[] createObjects(NodeList nodeList) throws Exception
    {
        Object[] objects = null;

        if((nodeList != null) && (nodeList.getLength() != 0))
        {
            final int paramCount = getMatchingNodeCount(nodeList, "param");

            if(paramCount != 0)
            {
                objects = new Object[paramCount];

                int index = 0;

                for(int nLoop = 0; nLoop < nodeList.getLength(); ++nLoop)
                {
                    if(!nodeList.item(nLoop).getNodeName().equals("param"))
                    {
                        continue;
                    }

                    objects[index] = createObject(nodeList.item(nLoop));

                    ++index;
                }
            }
            else
            {
                objects = new String[1];
                objects[0] = concatenateNodes(nodeList);
            }
        }

        return objects;
    }

    /**
     * Prints out the class type and string representation of all the Objects in the array
     */
    private static void printObjectArray(Object[] array)
    {
        if((array != null) && (array.length != 0))
        {
            for(int aLoop = 0; aLoop < array.length; ++aLoop)
            {
                System.out.println("object[" + aLoop + "] = " + array[aLoop].getClass() + " = "
                                   + array[aLoop]);
            }
        }
    }

    /**
     * Creates a Java object represented by <code>node</code>
     *
     * @param node XML containing structural information on how to construct a Java object
     * @return a Java object represented by <code>node</code>
     */
    public static Object createObject(Node node) throws Exception
    {
        if(!node.getNodeName().equals("param"))
        {
            throw new Exception("Only accept 'param' nodes, got \"" + node.getNodeName()
                                + "\"");
        }

        Class classInstance = getClassFromNode(node);

        Object createdObject = null;
        Object[] childObjects = createObjects(node.getChildNodes());

        if(!classInstance.isArray())
        {
            if (childObjects == null || childObjects.length == 0)
            {
                createdObject = classInstance.newInstance();
            }
            else
            {
                try
                {
                    //Class[] constructorClasses = getConstructorClasses(node.getChildNodes());
                    Class[] constructorClasses = getConstructorClasses(childObjects);

                    Constructor constructor = getMatchingConstructor(classInstance,
                        constructorClasses);

                    createdObject = constructor.newInstance(childObjects);
                }
                catch (Exception e)
                {
                    e.printStackTrace(System.out);
                    throw e;
                }
            }
        }
        else
        {
            createdObject = Array.newInstance(classInstance.getComponentType(),
                                              childObjects.length);

            for(int aLoop = 0; aLoop < childObjects.length; ++aLoop)
            {
                Array.set(createdObject, aLoop, childObjects[aLoop]);
            }
        }

        return createdObject;
    }

    private static Constructor getMatchingConstructor(Class classInstance,
        Class[] constructorClasses)
    {
        Constructor matchingConstructor = null;

        Constructor[] constructors = classInstance.getConstructors();
        if (constructors != null)
        {
            for (int cLoop = 0; cLoop < constructors.length && matchingConstructor == null; ++cLoop)
            {
                if (isAssignableFrom(constructors[cLoop].getParameterTypes(), constructorClasses))
                {
                    matchingConstructor = constructors[cLoop];
                }
            }
        }

        return matchingConstructor;
    }

    private static boolean isAssignableFrom(final Class[] left, final Class[] right)
    {
        boolean isAssignableFrom = true;

        boolean bothSameSize = (left == null && right == null) ||
            (left != null && right != null && left.length == right.length);

        if (!bothSameSize)
        {
            isAssignableFrom = false;
        }
        else
        {
            if (left != null)
            {
                for (int cLoop = 0; cLoop < left.length && isAssignableFrom; ++cLoop)
                {
                    if (!left[cLoop].isAssignableFrom(right[cLoop]))
                    {
                        isAssignableFrom = false;
                    }
                }
            }
        }

        return isAssignableFrom;
    }

    private static Class[] getConstructorClasses(Object[] objects)
    {
        Class[] classes = null;

        if (objects != null)
        {
            classes = new Class[objects.length];

            for (int oLoop = 0; oLoop < objects.length; ++oLoop)
            {
                classes[oLoop] = objects[oLoop].getClass();
            }
        }

        return classes;
    }

    /**
     * Gets an array of Classes from the node List, if <code>nodeList</code> is empty of only
     * contains #text node types that a String.class is returned
     *
     * @param    nodeList the node List
     *
     * @return an array of Classes
     * @throws ClassNotFoundException
     */
    private static Class[] getConstructorClasses(NodeList nodeList) throws ClassNotFoundException
    {
        Class[] constructorClasses = null;

        if((nodeList == null) || (nodeList.getLength() == 0)
            || (getMatchingNodeCount(nodeList, "#text") == nodeList.getLength()))
        {
            constructorClasses = new Class[1];
            constructorClasses[0] = java.lang.String.class;
        }
        else
        {
            List constructorClassesList = new LinkedList();

//            constructorClasses = new Class[nodeList.getLength()];

            for(int nLoop = 0; nLoop < nodeList.getLength(); ++nLoop)
            {
                final Node node = nodeList.item(nLoop);
                if (!"#text".equals(node.getNodeName()))
                {
                    constructorClassesList.add(getClassFromNode(nodeList.item(nLoop)));
                }
//                System.out.println("getConstructorClasses.item(" + nLoop + ") = "
//                                   + nodeList.item(nLoop).getNodeName() + ", "
//                                   + nodeList.item(nLoop).getNodeValue());
            }

            constructorClasses = new Class[constructorClassesList.size()];

            int index = 0;
            for (Iterator iterator = constructorClassesList.iterator(); iterator.hasNext();++index)
            {
                constructorClasses[index] = (Class) iterator.next();
            }

        }

        return constructorClasses;
    }

    /**
     * Returns the number of nodes in <code>nodeList</code> that have a node name equal to <code>nodeName</code>
     *
     * @param nodeList nodes to search through
     * @param nodeName node name to look for
     * @return number of nodes in <code>nodeList</code> that have a node name equal to <code>nodeName</code>
     */
    private static int getMatchingNodeCount(NodeList nodeList, String nodeName)
    {
        int numMatching = 0;

        for(int nLoop = 0; nLoop < nodeList.getLength(); ++nLoop)
        {
            if(nodeList.item(nLoop).getNodeName().equals(nodeName))
            {
                numMatching++;
            }
        }

        return numMatching;
    }

    /**
     * Concatenates the values of all the nodes in <code>nodeList</code>
     *
     * @param nodeList nodes to concatenate
     * @return Concatenated values of the nodes in <code>nodeList</code>
     */
    private static String concatenateNodes(NodeList nodeList)
    {
        StringBuffer concatenatedTextNodes = new StringBuffer();

        for(int nLoop = 0; nLoop < nodeList.getLength(); ++nLoop)
        {
            concatenatedTextNodes.append(nodeList.item(nLoop).getNodeValue());
        }

        return concatenatedTextNodes.toString();
    }

    /**
     * Creates a Class object from the 'class' attribute of <code>node</code>
     *
     * @param node node containing 'class' attribute
     * @return Class class represented by 'class' attribute of <code>node</code>
     * @exception ClassNotFoundException if 'class' attribute of <code>node</code> represents a class not in the classpath
     */
    private static Class getClassFromNode(Node node) throws ClassNotFoundException
    {
        return Class
            .forName(translateArrayClassString(node.getAttributes().getNamedItem("class")
                .getNodeValue()));
    }

    /**
     * Converts <code>from</code> to a {@link java.lang.Class#forName(java.lang.String)} compliant String
     * <p>
     * <code>Class.forName</code> requires strings that represent arrays to specified in a form like
     * <code>[Ljava.lang.String;</code> or <code>[[Lcom.company.MyClass;</code>
     * This function translates from a nicer form <code>java.lang.String[]</code>, <code>com.company.MyClass[][]</code>, etc.
     * to form required by <code>Class.forName</code>
     *
     * @param from Nicely formated class name
     * @return <code>from</code> converted to a <code>Class.forName</code> compliant String
     */
    private static String translateArrayClassString(String from)
    {
        int arrayCount = howManyOccurences(from, "[]");

        String translated = from;

        if(arrayCount != 0)
        {
            translated = repeatString("[", arrayCount) + "L"
                         + from.substring(0, from.indexOf("[]")) + ";";
        }

        return translated;
    }

    /**
     * Returns the number of times <code>searchIn</code> occurs in <code>searchIn</code>
     *
     * @param searchIn String to search in
     * @param searchFor String to search for
     * @return number of times <code>searchIn</code> occurs in <code>searchIn</code>
     */
    private static int howManyOccurences(final String searchIn, final String searchFor)
    {
        int numberOfOccurences = 0;
        int seekPos = 0;

        while((seekPos = searchIn.indexOf(searchFor, seekPos)) != -1)
        {
            numberOfOccurences++;
            seekPos++;
        }

        return numberOfOccurences;
    }

    /**
     * Returns <code>repeatWhat</code> repeated <code>howMany</code> times
     *
     * @param repeatWhat String to repeat
     * @param howMany number of times to repeat
     * @return <code>repeatWhat</code> repeated <code>howMany</code> times
     */
    private static String repeatString(final String repeatWhat, final int howMany)
    {
        StringBuffer repeatedString = new StringBuffer();

        for(int rLoop = 0; rLoop < howMany; ++rLoop)
        {
            repeatedString.append(repeatWhat);
        }

        return repeatedString.toString();
    }
}

