package org.jbossmx.cluster.watchdog.mbean.xmlet;

import java.io.IOException;
import java.io.InputStream;
import java.io.ObjectInputStream;
import java.io.ObjectStreamClass;

import java.lang.ClassNotFoundException;

import java.lang.reflect.Array;

/**
 * @author Stacy Curl
 */
public class XMLetObjectInputStream extends ObjectInputStream
{
    public XMLetObjectInputStream(InputStream inputStream, XMLet xmlet)
        throws IOException
    {
        super(inputStream);
        m_xmlet = xmlet;
    }

    /**
     * Use the given ClassLoader rather than using the system class
     */
    protected Class resolveClass(ObjectStreamClass objectstreamclass)
        throws IOException, ClassNotFoundException
    {
        Class resolvedClass;

        final String className = objectstreamclass.getName();

        if (!className.startsWith("["))
        {
            resolvedClass = m_xmlet.loadClass(className);
        }
        else
        {
            final int numDimensions = countArrayDimensions(className);

            Class class1;

            if (className.charAt(numDimensions) == 'L')
            {
                class1 = m_xmlet.loadClass(className.substring(numDimensions + 1,
                    className.length() - 1));
            }
            else
            {
                if (className.length() != numDimensions + 1)
                {
                    throw new ClassNotFoundException(className);
                }

                class1 = primitiveType(className.charAt(numDimensions));
            }

            int emptyArray[] = new int[numDimensions];

            for (int dLoop = 0; dLoop < numDimensions; dLoop++)
            {
                emptyArray[dLoop] = 0;
            }

            resolvedClass = Array.newInstance(class1, emptyArray).getClass();
        }

        return resolvedClass;
    }

    private static int countArrayDimensions(final String arrayClassName)
    {
        int arrayDimensions;
        for (arrayDimensions = 1; arrayClassName.charAt(arrayDimensions) == '['; arrayDimensions++)
        {
            ;
        }

        return arrayDimensions;
    }

    private static Class primitiveType(char c)
    {
        Class type = null;

        switch(c)
        {
            case 'B':
                type = Byte.TYPE;
                break;

            case 'C':
                type = Character.TYPE;
                break;

            case 'D':
                type = Double.TYPE;
                break;

            case 'F':
                type = Float.TYPE;
                break;

            case 'I':
                type = Integer.TYPE;
                break;

            case 'J':
                type = Long.TYPE;
                break;

            case 'S':
                type = Short.TYPE;
                break;

            case 'Z':
                type = Boolean.TYPE;
                break;
        }

        return type;
    }

    private XMLet m_xmlet;
}