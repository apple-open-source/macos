package org.jboss.test.jrmp.ejb;

import java.io.Serializable;
import org.jboss.test.jrmp.interfaces.IString;

public class AString implements IString, Serializable
{
    private String theString;

    public AString(String theString)
    {
        this.theString = theString;
    }
    public String toString()
    {
        return theString;
    }
}
