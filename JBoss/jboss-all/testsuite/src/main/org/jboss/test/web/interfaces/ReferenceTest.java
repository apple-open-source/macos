package org.jboss.test.web.interfaces;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.Serializable;

/** A simple Serializable that is used to test the call optimization
of EJBs. When a call is optimized an instance of this class should be
passed by reference rather than being serialized.

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
*/
public class ReferenceTest implements Serializable
{
    private transient boolean wasSerialized;

    /** Creates new ReferenceTest */
    public ReferenceTest()
    {
    }

    public boolean getWasSerialized()
    {
        return wasSerialized;
    }

    private void readObject(ObjectInputStream in)
         throws IOException, ClassNotFoundException
    {
        wasSerialized = true;
    }
}
