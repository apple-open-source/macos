// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: XmlParser.java,v 1.15.2.5 2003/06/04 04:48:04 starksm Exp $
// ========================================================================

package org.mortbay.xml;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.AbstractList;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.Stack;
import javax.xml.parsers.SAXParser;
import javax.xml.parsers.SAXParserFactory;
import org.mortbay.util.Code;
import org.mortbay.util.Resource;
import org.xml.sax.Attributes;
import org.xml.sax.ContentHandler;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;
import org.xml.sax.XMLReader;
import org.xml.sax.helpers.DefaultHandler;


/*--------------------------------------------------------------*/
/** XML Parser wrapper.
 * This class wraps any standard JAXP1.1 parser with convieniant error and
 * entity handlers and a mini dom-like document tree.
 * <P>
 * By default, the parser is created as a validating parser. This can be 
 * changed by setting the "org.mortbay.xml.XmlParser.NotValidating"
 * system property to true.
 *
 * @version $Id: XmlParser.java,v 1.15.2.5 2003/06/04 04:48:04 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class XmlParser 
{
    private Map _redirectMap = new HashMap();
    private SAXParser _parser;
    private Map _observerMap;
    private Stack _observers = new Stack();
    
    /* ------------------------------------------------------------ */
    /** Construct
     */
    public XmlParser()
    {
        try
        {
            SAXParserFactory factory = SAXParserFactory.newInstance();
            factory.setValidating(!Boolean.getBoolean("org.mortbay.xml.XmlParser.NotValidating"));
            _parser = factory.newSAXParser();
        }
        catch(Exception e)
        {
            Code.warning(e);
            throw new Error(e.toString());
        }
    }
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     */
    public XmlParser(boolean validating)
    {
        try
        {
            SAXParserFactory factory = SAXParserFactory.newInstance();
            factory.setValidating(validating);
            _parser = factory.newSAXParser();
        }
        catch(Exception e)
        {
            Code.warning(e);
            throw new Error(e.toString());
        }
    }

    /* ------------------------------------------------------------ */
    /** 
     * @param name 
     * @param entity
     */
    public synchronized void redirectEntity(String name,Resource entity)
    {
        if (entity!=null)
            _redirectMap.put(name,entity);
    }

    /* ------------------------------------------------------------ */
    /** Add a ContentHandler.
     * Add an additional content handler that is triggered on a tag
     * name. SAX events are passed to the ContentHandler provided from
     * a matching start element to the corresponding end element.
     * Only a single content handler can be registered against each tag.
     * @param trigger Tag local or q name.
     * @param observer SAX ContentHandler
     */
    public synchronized void addContentHandler(String trigger,
                                               ContentHandler observer)
    {
        if (_observerMap==null)
            _observerMap=new HashMap();
        _observerMap.put(trigger,observer);
    }

    /* ------------------------------------------------------------ */
    public synchronized Node parse(InputSource source)
        throws IOException,SAXException
    {
        Handler handler= new Handler();
        XMLReader reader = _parser.getXMLReader();
        reader.setContentHandler(handler);
  	reader.setErrorHandler(handler);
  	reader.setEntityResolver(handler);
        _parser.parse(source, handler);
        if (handler._error!=null)
            throw handler._error;
        Node doc=(Node)handler._top.get(0);
        handler.clear();
        return doc;
    }
    
    
    /* ------------------------------------------------------------ */
    /** Parse URL.
     */
    public synchronized Node parse(String url)
        throws IOException,SAXException
    {
        return parse(new InputSource(url));
    }
    
    /* ------------------------------------------------------------ */
    /** Parse File. 
     */
    public synchronized Node parse(File file)
        throws IOException,SAXException
    {
        return parse(new InputSource(file.toURL().toString()));
    }

    /* ------------------------------------------------------------ */
    /** Parse InputStream.
     */
    public synchronized Node parse(InputStream in)
        throws IOException,SAXException
    {
        Handler handler= new Handler();
        XMLReader reader = _parser.getXMLReader();
        reader.setContentHandler(handler);
  	reader.setErrorHandler(handler);
  	reader.setEntityResolver(handler);
        _parser.parse(new InputSource(in), handler);
        if (handler._error!=null)
            throw handler._error;
        Node doc=(Node)handler._top.get(0);
        handler.clear();
        return doc;
    }
    
    
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    private class Handler extends DefaultHandler
    {
        Node _top = new Node(null,null,null);
        SAXParseException _error;
        private Node _context = _top;

        /* ------------------------------------------------------------ */
        void clear()
        {
            _top=null;
            _error=null;
            _context=null;
        }
        
        /* ------------------------------------------------------------ */
        public void startElement (String uri, String localName, String qName, Attributes attrs)
            throws SAXException
        {
            String name=(uri==null || uri.equals(""))?qName:localName;
            Node node= new Node(_context,name,attrs);
            _context.add(node);
            _context=node;
            
            ContentHandler observer=null;
            if (_observerMap!=null)
                 observer=(ContentHandler) _observerMap.get(name);
            _observers.push(observer);

            for(int i=0;i<_observers.size();i++)
                if (_observers.get(i)!=null)
                    ((ContentHandler)_observers.get(i))
                        .startElement(uri,localName,qName,attrs);
        }

        /* ------------------------------------------------------------ */
        public void endElement (String uri, String localName, String qName)
            throws SAXException
        {
            _context=_context._parent;
            for(int i=0;i<_observers.size();i++)
                if (_observers.get(i)!=null)
                    ((ContentHandler)_observers.get(i))
                        .endElement(uri,localName,qName);
            _observers.pop();
        }

        /* ------------------------------------------------------------ */
        public void ignorableWhitespace (char buf [], int offset, int len)
            throws SAXException
        {
            // XXX - for testing
            // characters(buf,offset,len);
            
            for(int i=0;i<_observers.size();i++)
                if (_observers.get(i)!=null)
                    ((ContentHandler)_observers.get(i))
                        .ignorableWhitespace (buf,offset,len);
        }

        /* ------------------------------------------------------------ */
        public void characters (char buf [], int offset, int len)
            throws SAXException
        {
            _context.add(new String(buf,offset,len));
            for(int i=0;i<_observers.size();i++)
                if (_observers.get(i)!=null)
                    ((ContentHandler)_observers.get(i))
                        . characters(buf,offset,len);
        }
        
        /* ------------------------------------------------------------ */
        public void warning(SAXParseException ex)
        {
            Code.debug(ex);
            Code.warning("WARNING@"+getLocationString(ex)+
                         " : "+ex.toString());
        }
                
        /* ------------------------------------------------------------ */
        public void error(SAXParseException ex)
            throws SAXException
        {
            // Save error and continue to report other errors
            if(_error==null)
                _error=ex;
            Code.debug(ex);
            Code.warning("ERROR@"+getLocationString(ex)+
                         " : "+ex.toString());
        }
        
        /* ------------------------------------------------------------ */
        public void fatalError(SAXParseException ex)
            throws SAXException
        {
            _error=ex;
            Code.debug(ex);
            Code.warning("FATAL@"+getLocationString(ex)+
                         " : "+ex.toString());
            throw ex;
        }	    

        /* ------------------------------------------------------------ */
        private String getLocationString(SAXParseException ex)
        {
            return ex.getSystemId()+
                " line:"+ex.getLineNumber()+
                " col:"+ex.getColumnNumber();
        }
        
        /* ------------------------------------------------------------ */
        public InputSource resolveEntity
            (String pid, String sid)
        {
            Resource resource=null;
            if(pid!=null)
                resource = (Resource)_redirectMap.get(pid);
            if(resource==null)
                resource = (Resource)_redirectMap.get(sid);
            if (resource==null)
            {
                String dtd = sid;
                if (dtd.lastIndexOf('/')>=0)
                    dtd=dtd.substring(dtd.lastIndexOf('/')+1);
                resource = (Resource)_redirectMap.get(dtd);
            }

            if (resource!=null && resource.exists())
            {
                try
                {
                    InputStream in= resource.getInputStream();
                    Code.debug("Redirected entity ",
                               sid," --> ",resource);
                    return new InputSource(in);
                }
                catch(IOException e){Code.ignore(e);}
            }
            return null;
        }
    }
    
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /** XML Attribute.
     */
    public static class Attribute
    {
        private String _name;
        private String _value;
        Attribute(String n,String v)
        {
            _name=n;
            _value=v;
        }
        public String getName() {return _name;}
        public String getValue() {return _value;}
    }
    
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /** XML Node.
     * Represents an XML element with optional attributes and
     * ordered content.
     */
    public static class Node extends AbstractList
    {
        Node _parent;
        private ArrayList _list;
        private String _tag;
        private Attribute[] _attrs;
        private boolean _lastString=false;
        
        /* ------------------------------------------------------------ */
        Node(Node parent,String tag, Attributes attrs)
        {
            _parent=parent;
            _tag=tag;

            if (attrs!=null)
            {
                _attrs=new Attribute[attrs.getLength()];
                for (int i = 0; i <attrs.getLength(); i++)
		{
		    String name = attrs.getLocalName(i);
		    if ( name==null || name.equals("") )
			name = attrs.getQName(i);
                    _attrs[i]=new Attribute(name,
                                            attrs.getValue(i));
                }                    
            }
        }
        
        /* ------------------------------------------------------------ */
        public Node getParent()
        {
            return _parent;
        }  

        /* ------------------------------------------------------------ */
        public String getTag()
        {
            return _tag;
        } 

        /* ------------------------------------------------------------ */
        /** Get an array of element attributes.
         */
 	public Attribute[] getAttributes()
        {
            return _attrs;
        }
        
        /* ------------------------------------------------------------ */
        /** Get an element attribute.
         * @return attribute or null.
         */
 	public String getAttribute(String name)
        {
            return getAttribute(name,null);
        }
        
        /* ------------------------------------------------------------ */
        /** Get an element attribute.
         * @return attribute or null.
         */
 	public String getAttribute(String name, String dft)
        {
            if (_attrs==null || name==null)
                return dft;
            for (int i=0;i<_attrs.length;i++)
                if (name.equals(_attrs[i].getName()))
                    return _attrs[i].getValue();
            return dft;
        }
        
        
        /* ------------------------------------------------------------ */
        /** Get the number of children nodes.
         */
 	public int size()
        {
            if (_list!=null)
                return _list.size();
            return 0;
        }
        
        /* ------------------------------------------------------------ */
        /** Get the ith child node or content.
         * @return Node or String.
         */
 	public Object get(int i)
        {
            if (_list!=null)
                return _list.get(i);
            return null;
        }
        
        /* ------------------------------------------------------------ */
        /** Get the first child node with the tag.
         * @param tag 
         * @return Node or null.
         */
 	public Node get(String tag)
        {
            if (_list!=null)
            {
                for (int i=0;i<_list.size();i++)
                {
                    Object o=_list.get(i);
                    if (o instanceof Node)
                    {
                        Node n=(Node)o;
                        if (tag.equals(n._tag))
                            return n;
                    }
                }
            }
            return null;
        }

        
        /* ------------------------------------------------------------ */
        public void add(int i, Object o)
        {
            if (_list==null)
                _list=new ArrayList();
            if (o instanceof String)
            {
                if (_lastString)
                {
                    int last=_list.size()-1;
                    _list.set(last,(String)_list.get(last)+o);
                }
                else
                    _list.add(i,o);
                _lastString=true;
            }
            else
            {
                _lastString=false;
                _list.add(i,o);
            }
        }	

        /* ------------------------------------------------------------ */
        public void clear()
        {
            if (_list!=null)
                _list.clear();
            _list=null;
        }
        
        /* ------------------------------------------------------------ */
        /** Get a tag as a string.
         * @param tag The tag to get 
         * @param tags IF true, tags are included in the value.
         * @param trim If true, trim the value.
         * @return  results of get(tag).toString(tags).
         */
        public String getString(String tag, boolean tags, boolean trim)
        {
            Node node=get(tag);
            if (node==null)
                return null;
            String s =node.toString(tags);
            if (s!=null && trim)
                s=s.trim();
            return s;
        }
        
        /* ------------------------------------------------------------ */
        public synchronized String toString()
        {
            return toString(true);
        }
        
        /* ------------------------------------------------------------ */
        /** Convert to a string.
         * @param tag If false, only content is shown.
         */
 	public synchronized String toString(boolean tag)
        {
            StringBuffer buf = new StringBuffer();
            synchronized(buf)
            {
                toString(buf,tag);
                return buf.toString();
            }
        }
        
        /* ------------------------------------------------------------ */
        /** Convert to a string.
         * @param tag If false, only content is shown.
         */
 	public synchronized String toString(boolean tag,boolean trim)
        {
            String s=toString(tag);
            if (s!=null && trim)
                s=s.trim();
            return s;
        }
        
        /* ------------------------------------------------------------ */
        private synchronized void toString(StringBuffer buf,boolean tag)
        {
            if(tag)
            {
                buf.append("<");
                buf.append(_tag);
            
		if (_attrs!=null)
		{
		    for (int i=0;i<_attrs.length;i++)
		    {
			buf.append(' ');
			buf.append(_attrs[i].getName());
			buf.append("=\"");
			buf.append(_attrs[i].getValue());
			buf.append("\"");
		    }
		}
            }

            if (_list!=null)
            {
                if(tag)
                    buf.append(">");
                
                for (int i=0;i<_list.size();i++)
                {
                    Object o=_list.get(i);
                    if (o==null)
                        continue;
                    if (o instanceof Node)
                        ((Node)o).toString(buf,tag);
                    else
                        buf.append(o.toString());
                }

                if(tag)
                {
                    buf.append("</");
                    buf.append(_tag);
                    buf.append(">");
                }
            }
            else if (tag)
                buf.append("/>");
        }
        
        /* ------------------------------------------------------------ */
        /** Iterator over named child nodes.
         * @param tag The tag of the nodes.
         * @return Iterator over all child nodes with the specified tag.
         */
 	public Iterator iterator(final String tag)
        {
            return new Iterator()
                {
                    int c=0;
                    Node _node;

                    /* -------------------------------------------------- */
                    public boolean hasNext()
                    {
                        if (_node!=null)
                            return true;
                        while (_list!=null && c<_list.size())
                        {
                            Object o=_list.get(c);
                            if (o instanceof Node)
                            {
                                Node n=(Node)o;
                                
                                if (tag.equals(n._tag))
                                {
                                    _node=n;
                                    return true;
                                }
                            }
                            c++;
                        }
                        return false;
                    }
        
                    /* -------------------------------------------------- */
                    public Object next()
                    {
                        try
                        {
                            if (hasNext())
                                return _node;
                            throw new NoSuchElementException();
                        }
                        finally
                        {
                            _node=null;
                            c++;
                        }
                    }

                    /* -------------------------------------------------- */
                    public void remove()
                    {
                        throw new UnsupportedOperationException("Not supported");
                    }
                    
                };
        }
    }
}
