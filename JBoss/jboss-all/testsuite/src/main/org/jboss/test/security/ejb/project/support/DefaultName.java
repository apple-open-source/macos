package org.jboss.test.security.ejb.project.support;

import java.util.Enumeration;
import java.util.Properties;
import javax.naming.CompoundName;
import javax.naming.InvalidNameException;
import javax.naming.Name;
import javax.naming.NameParser;
import javax.naming.NamingException;

/** A simple subclass of CompoundName that fixes the name syntax to:
	jndi.syntax.direction = left_to_right
	jndi.syntax.separator = "/"

@author Scott_Stark@displayscape.com
@version $Revision: 1.1 $
*/
public class DefaultName extends CompoundName
{
	/** The Properties used for the project directory heirarchical names */
	static Name emptyName;
	static Properties nameSyntax = new Properties();
	static
	{
		nameSyntax.put("jndi.syntax.direction", "left_to_right");
		nameSyntax.put("jndi.syntax.separator", "/");
		try
		{
			emptyName = new DefaultName("");
		}
		catch(InvalidNameException e)
		{
		}	
	}

    private static class DefaultNameParser implements NameParser
    {
        public Name parse(String path) throws NamingException
        {
            DefaultName name = new DefaultName(path);
            return name;
        }
    }

    public static NameParser getNameParser()
    {
        return new DefaultNameParser();
    }

	/** Creates new DefaultName */
    public DefaultName(Enumeration comps)
	{
		super(comps, nameSyntax);
    }
    public DefaultName(String name) throws InvalidNameException
	{
		super(name, nameSyntax);
    }
    public DefaultName(Name name)
	{
		super(name.getAll(), nameSyntax);
    }
    public DefaultName()
	{
		this(emptyName);
    }

}
