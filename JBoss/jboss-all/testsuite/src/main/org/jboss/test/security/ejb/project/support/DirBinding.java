package org.jboss.test.security.ejb.project.support;

import javax.naming.Binding;
import javax.naming.directory.Attributes;

/** A subclass of Binding that adds support for Attributes. This class is used
to pass a contexts raw bindings to NameBindingIterator.

@author Scott_Stark@displayscape.com
@version $Revision: 1.1 $
*/
public class DirBinding extends Binding
{
	private transient Attributes attributes;

	/** Constructs an instance of a Binding given its relative name, object,
	 attributes and whether the name is relative. 
	@param obj - The possibly null object bound to name.
	@param attributes - the attributes associated with obj
	*/
    public DirBinding(String name, Object obj, Attributes attributes)
	{
		this(name, null, obj, true, attributes);
	}
	/** Constructs an instance of a Binding given its relative name, class name,
	 object, attributes and whether the name is relative.
	@param name - The non-null string name of the object.
	@param className - The possibly null class name of the object bound to name.
	 If null, the class name of obj is returned by getClassName(). If obj is
	 also null, getClassName() will return null.
	@param obj - The possibly null object bound to name.
	@param attributes - the attributes associated with obj
	*/
    public DirBinding(String name, String className, Object obj, Attributes attributes)
	{
		this(name, className, obj, true, attributes);
	}
	/** Constructs an instance of a Binding given its name, object, attributes
	 and whether the name is relative. 
	@param name - The non-null string name of the object.
	@param obj - The possibly null object bound to name.
	@param isRelative - true if name is a name relative to the target context
	 (which is named by the first parameter of the listBindings() method);
	 false if name is a URL string.
	@param attributes - the attributes associated with obj
	*/
    public DirBinding(String name, String className, Object obj, boolean isRelative,
		Attributes attributes)
	{
		super(name, className, obj, isRelative);
		this.attributes = attributes;
    }

	public Attributes getAttributes()
	{
		return attributes;
	}
	public void setAttributes(Attributes attributes)
	{
		this.attributes = attributes;
	}
}
