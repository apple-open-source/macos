package org.jboss.test.security.ejb.project.support;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Hashtable;
import java.util.Iterator;
import javax.naming.Binding;
import javax.naming.CompositeName;
import javax.naming.Context;
import javax.naming.InvalidNameException;
import javax.naming.Name;
import javax.naming.NameAlreadyBoundException;
import javax.naming.NamingEnumeration;
import javax.naming.NamingException;
import javax.naming.NameNotFoundException;
import javax.naming.NameParser;
import javax.naming.NotContextException;
import javax.naming.OperationNotSupportedException;
import javax.naming.directory.Attribute;
import javax.naming.directory.Attributes;
import javax.naming.directory.BasicAttributes;
import javax.naming.directory.DirContext;
import javax.naming.directory.ModificationItem;
import javax.naming.directory.SearchControls;

/** A simple in memory implementation of DirContext that uses a HashMap as the
 store and unix style path names.

@author Scott_Stark@displayscape.com
@version $Id: HeirMemoryMap.java,v 1.1 2001/03/05 10:11:02 stark Exp $
*/
public class HeirMemoryMap extends DirContextStringImpl implements DirContext, Serializable
{
	private static NameParser nameParser = DefaultName.getNameParser();
	private HashMap bindings = new HashMap();
	private HashMap bindingAttrs = new HashMap();
	private HeirMemoryMap parent;
	private String contextName;
	private Hashtable env;

	/** Creates new HeirMemoryMap */
    public HeirMemoryMap()
	{
		this.contextName = "";
    }
    public HeirMemoryMap(String contextName, HeirMemoryMap parent, Attributes attributes) throws NamingException
	{
		this(contextName, parent, attributes, null);
	}
    public HeirMemoryMap(String contextName, HeirMemoryMap parent, Attributes attributes, Hashtable env) throws NamingException
	{
		this.contextName = contextName == null ? "" : contextName;
		this.parent = parent;
		bindingAttrs.put("", attributes.clone());
		if( parent != null )
			parent.bind(contextName, this);
		this.env = env;
    }

	public String toString()
	{
		Name name = null;
		try
		{
			name = getFullName();
		}
		catch(NamingException e)
		{
		}
		return name.toString();
	}

	String getName()
	{
		return contextName;
	}
	void setName(String contextName)
	{
		this.contextName = contextName;
	}
	Name getFullName() throws NamingException
	{
		CompositeName name = new CompositeName(getName());
		HeirMemoryMap context = parent;
		if( context == null )
			return name;

		try
		{
			while( context.parent != null )
			{
				name.add(0, context.getName());
				context = context.parent;
			}
		}
		catch(NamingException e)
		{
		}
		return name;
	}

// --- 
	public Object addToEnvironment(String p1,Object p2) throws NamingException
	{
		return null;
	}
	public Object removeFromEnvironment(String p1) throws NamingException
	{
		return null;
	}
	
	public void bind(Name name, Object value) throws NamingException
	{
		bind(name, value, null);
	}
	
	public void bind(Name name, Object value, Attributes attributes) throws NamingException
	{
        if( name.isEmpty() )
		{
            throw new InvalidNameException("Cannot bind empty name");
        }

		internalBind(name, value, attributes, true);
	}

	public void close() throws NamingException
	{
	}
	
	public Name composeName(Name p1,Name p2) throws NamingException {
		return null;
	}
	
	public Context createSubcontext(Name name) throws NamingException
	{
		return createSubcontext(name, null);
	}
	
	public DirContext createSubcontext(Name name, Attributes attributes) throws NamingException
	{
        if( name.isEmpty() )
		{
            throw new InvalidNameException("Cannot createSubcontext with empty name");
        }

        DirContext subctx = null;
        String atom = name.get(0);
        if( name.size() == 1 )
        {
            subctx = new HeirMemoryMap(atom, this, attributes, env);
        }
		else
		{
			DirContext context = (DirContext) bindings.get(atom);
			subctx = context.createSubcontext(name.getSuffix(1), attributes);
		}

		return subctx;
	}
	
	public void destroySubcontext(Name name) throws NamingException
	{
		unbind(name);
	}

	public Attributes getAttributes(Name name) throws NamingException
	{
		return getAttributes(name, null);
	}

	public Attributes getAttributes(Name name, String[] attrIDs) throws NamingException
	{
		Attributes nameAttributes = null;
        String atom = name.get(0);
		if( name.isEmpty() == true )
		{
            nameAttributes = (Attributes) bindingAttrs.get("");
		}
        else if( name.size() == 1 )
        {
            Object binding = bindings.get(atom);
            if( binding != null )
            {
                if( binding instanceof DirContext )
                {
                    DirContext dirCtx = (DirContext) binding;
                    return dirCtx.getAttributes(name.getSuffix(1), attrIDs);
                }
            }
            nameAttributes = (Attributes) bindingAttrs.get(atom);
        }
		else
		{
			DirContext context = (DirContext) bindings.get(atom);
			nameAttributes = context.getAttributes(name.getSuffix(1), attrIDs);
		}

		if( nameAttributes != null && attrIDs != null )
		{
			BasicAttributes matches = new BasicAttributes(nameAttributes.isCaseIgnored());
			for(int a = 0; a < attrIDs.length; a ++)
			{
				Attribute attr = nameAttributes.get(attrIDs[a]);
				if( attr != null )
					matches.put(attr);
			}
			nameAttributes = matches;
		}
		return nameAttributes;
	}

	public java.util.Hashtable getEnvironment() throws NamingException
	{
		return env;
	}

	public String getNameInNamespace() throws NamingException
	{
		return toString();
	}

	public NameParser getNameParser(Name p1) throws NamingException
	{
		return nameParser;
	}

	public DirContext getSchema(Name p1) throws NamingException
	{
		throw new OperationNotSupportedException("Not implemented yet");
	}
	
	public DirContext getSchemaClassDefinition(Name p1) throws NamingException
	{
		throw new OperationNotSupportedException("Not implemented yet");
	}
	
	public NamingEnumeration list(Name p1) throws NamingException
	{
		return null;
	}

	public NamingEnumeration listBindings(Name name) throws NamingException
	{
		NamingEnumeration iter = null;

		if( name.isEmpty() == true )
		{
			Iterator keys = bindings.keySet().iterator();
			ArrayList tmp = new ArrayList();
			while( keys.hasNext() )
			{
				String key = (String) keys.next();
				Object value = bindings.get(key);
				Attributes attributes = (Attributes) bindingAttrs.get(key);
                DirBinding tuple = new DirBinding(key, value, attributes);
				tmp.add(tuple);
			}
			iter = new NameBindingIterator(tmp.iterator(), this);
		}
		else
		{
			String atom = name.get(0);
			Context context = (Context) bindings.get(atom);
			iter = context.listBindings(name.getSuffix(1));
		}

		return iter;
	}

	public Object lookup(Name name) throws NamingException
	{
		if( name.isEmpty() == true )
			return this;

		String atom = name.get(0);
		Object binding = bindings.get(atom);
		if( name.size() == 1 )
		{   /* Need to check that binding is null and atom is not a key
                since a null value could have been bound.
            */
			if( binding == null && bindings.containsKey(atom) == false )
			{
				NameNotFoundException e = new NameNotFoundException("Failed to find: "+atom);
				e.setRemainingName(name);
				e.setResolvedObj(this);
				throw e;
			}
		}
		else if( (binding instanceof Context) )
		{
			Context context = (Context) binding;
			binding = context.lookup(name.getSuffix(1));
		}
		else
		{
			NotContextException e = new NotContextException(atom + " does not name a directory context that supports attributes");
			e.setRemainingName(name);
			e.setResolvedObj(binding);
			throw e;
		}
		return binding;
	}

	public Object lookupLink(Name p1) throws NamingException
	{
		throw new OperationNotSupportedException("Not implemented yet");
	}
	
	public void modifyAttributes(Name p1,ModificationItem[] p2) throws NamingException
	{
		throw new OperationNotSupportedException("Not implemented yet");
	}
	
	public void modifyAttributes(Name p1,int p2,Attributes p3) throws NamingException
	{
		throw new OperationNotSupportedException("Not implemented yet");
	}
	
	public void rebind(Name name, Object value) throws NamingException
	{
		rebind(name, value, null);
	}
	
	public void rebind(Name name, Object value, Attributes attributes) throws NamingException
	{
        if( name.isEmpty() )
		{
            throw new InvalidNameException("Cannot bind empty name");
        }

		internalBind(name, value, attributes, false);
	}

	public void rename(Name p1,Name p2) throws NamingException
	{
		throw new OperationNotSupportedException("Not implemented yet");
	}

	public NamingEnumeration search(Name p1,Attributes p2) throws NamingException
	{
		throw new OperationNotSupportedException("Not implemented yet");
	}

	public NamingEnumeration search(Name p1,String p2,SearchControls p3) throws NamingException
	{
		throw new OperationNotSupportedException("Not implemented yet");
	}

	public NamingEnumeration search(Name p1,Attributes p2,String[] p3) throws NamingException
	{
		throw new OperationNotSupportedException("Not implemented yet");
	}

	public NamingEnumeration search(Name p1,String p2,Object[] p3,SearchControls p4) throws NamingException
	{
		throw new OperationNotSupportedException("Not implemented yet");
	}

	public void unbind(Name name) throws NamingException
	{
        if( name.isEmpty() )
		{
            throw new InvalidNameException("Cannot unbind empty name");
        }

		String atom = name.get(0);
		Object binding = bindings.get(atom);
		if( name.size() == 1 )
		{   /* Need to check that binding is null and atom is not a key
                since a null value could have been bound.
            */
			if( binding == null && bindings.containsKey(atom) == false )
			{
				NameNotFoundException e = new NameNotFoundException("Failed to find: "+atom);
				e.setRemainingName(name);
				e.setResolvedObj(this);
				throw e;
			}
    		bindings.remove(atom);
            bindingAttrs.remove(atom);
		}
		else if( (binding instanceof Context) )
		{
			Context context = (Context) binding;
			context.unbind(name.getSuffix(1));
		}
		else
		{
			NotContextException e = new NotContextException(atom + " does not name a directory context that supports attributes");
			e.setRemainingName(name);
			e.setResolvedObj(binding);
			throw e;
		}
	}
// ---

	private void internalBind(Name name, Object value, Attributes attributes, boolean isBind) throws NamingException
	{
		String atom = name.get(0);
		Object binding = bindings.get(atom);

		if( name.size() == 1 )
		{
	    	if( binding != null && isBind == false )
			{
				throw new NameAlreadyBoundException("Use rebind to override");
			}

			// Add object to internal data structure
			bindings.put(atom, value);

			// Add attributes
			if( attributes != null )
			{
				bindingAttrs.put(atom, attributes);
		    }
		}
		else
		{
		    // Intermediate name: Consume name in this context and continue
			if( (binding instanceof Context) == false )
			{
				NotContextException e = new NotContextException(atom + " does not name a context");
				e.setRemainingName(name);
				e.setResolvedObj(binding);
				throw e;
			}

			if( attributes == null )
			{
				Context context = (Context) binding;
				if( isBind == true )
					context.bind(name.getSuffix(1), value);
				else
					context.rebind(name.getSuffix(1), value);
			}
			else if( (binding instanceof DirContext) == false )
			{
				NotContextException e = new NotContextException(atom + " does not name a directory context that supports attributes");
				e.setRemainingName(name);
				e.setResolvedObj(binding);
				throw e;
			}
			else
			{
				DirContext context = (DirContext) binding;
				if( isBind == true )
					context.bind(name.getSuffix(1), value, attributes);
				else
					context.rebind(name.getSuffix(1), value, attributes);
			}
		}
	}
}
