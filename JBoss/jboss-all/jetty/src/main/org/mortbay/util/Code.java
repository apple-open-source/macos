// ========================================================================
// Copyright (c) 1997 MortBay Consulting, Sydney
// $Id: Code.java,v 1.15.2.7 2003/07/11 00:55:02 jules_gosnell Exp $
// ========================================================================

package org.mortbay.util;

import java.io.PrintWriter;
import java.lang.reflect.Method;
import java.util.StringTokenizer;
import java.util.Vector;

/* ----------------------------------------------------------------------- */
/** Coding Standards support.
* Support for defensive programming
*
* Coding defensively as follows:
* <pre>
* {
*    ...
*    Code.assertTrue(booleanExpression(),"Assert that expression evaluates true");
*    ...
*    Code.assertEquals(objectA,objectB,"Assert equality");
*    Code.assertEquals(1,2,"Assert equality");
*    ...
*    Code.debug("Debug message");
*    ...
*    Code.debug("Debug message",exception,object);
*    ...
*    Code.warning("warning message");
*    ...
*    Code.warning("warning message",exception);
*    ...
*    Code.notImplemented();
*    Code.fail("fatal error");
*    ...
* }
* </pre>
* The debug output can be controlled with java properties:
* <UL>
* <LI>DEBUG - If set, debug output is enabled
* <LI>DEBUG_PATTERNS - If set to a comma separated list of strings,
* Then debug output is only produced from those classes whose fully
* qualified class name contains one of the strings as a substring.
* <LI>DEBUG_OPTIONS - A string of option character. 'W' - suppress
* warnings unless debug is on, 'S' suppress stack frame dumps
* <LI>DEBUG_VERBOSE - Set to an integer verbosity level for
* use in controlling debug verbosity (see verbose(int))
* <UL><p>
* <h4>Usage</h4>
* <pre>
* java [-DDEBUG [-DDEBUG_PATTERNS="MyClass,my.package"] \\
*               [-DDEBUG_OPTIONS=[S][W]] \\
*               [-DDEBUG_VERBOSE=n] ] \\
*      my.package.main
* </pre>
*
* @version $Id: Code.java,v 1.15.2.7 2003/07/11 00:55:02 jules_gosnell Exp $
* @author Greg Wilkins
*/
public class Code
{

    /* ------------------------------------------------------------ */
    static
    {
        try
        {
            new Frame(0);
        }
        catch(Error e)
        {
            e.printStackTrace();
            System.err.println("\nERROR: Jetty compiled for java 1.4 and cannot be run on this JVM");
            System.err.println("       Please use the org.mortbay.jetty-jdk1.2.jar instead\n");
            System.exit(1);
        }
    }
    
    /*-------------------------------------------------------------------*/
    private static final Class[] __noArgs=new Class[0];
    private static final String[] __nestedEx =
        {"getTargetException","getTargetError","getException","getRootCause"};

    /*-------------------------------------------------------------------*/
    /** Shared static instances, reduces object creation at expense
     * of lock contention in multi threaded debugging */
    private static StringBufferWriter __stringBufferWriter = new StringBufferWriter();
    private static PrintWriter __printWriter = new PrintWriter(__stringBufferWriter);
    
    /*-------------------------------------------------------------------*/
    private static class Singleton {static Code __instance=new Code();}
    public static Code instance()
    {
        return Singleton.__instance;
    }
    
    /*-------------------------------------------------------------------*/
    boolean _debugOn=false;
    private boolean _suppressStack=true;
    private boolean _suppressWarnings=false;
    private int _verbose=0;
    Vector _debugPatterns=null;
    private String _patterns=null;
    
    /*-------------------------------------------------------------------*/
    /** Construct shared instance that decodes the options setup
     * environments properties.
     */
    protected Code()
    {
        Singleton.__instance=this;
        try{
            _debugOn= System.getProperty("DEBUG") != null;
        
            String o = System.getProperty("DEBUG_OPTIONS");
            if (o!=null)
            {
                setSuppressStack(!(o.indexOf('S')>=0));
                setSuppressWarnings(!(o.indexOf('W')>=0));
            }
            else
            {
                setSuppressStack(false);
                setSuppressWarnings(false);
            }

            String dp = System.getProperty("DEBUG_PATTERNS");
            setDebugPatterns(dp);

            String v = System.getProperty("DEBUG_VERBOSE");
            if (v!=null)
                setVerbose(Integer.parseInt(v));
        }
        catch (Exception e){
            System.err.println("Exception from getProperty!\n"+
                               "Probably running in applet\n"+
                               "Use Code.initParamsFromApplet or Code.setOption to control debug output.");
        }
    }

    /* ------------------------------------------------------------ */
    /** Set if debugging is on or off.
     * @param debug 
     */
    public static synchronized void setDebug(boolean debug)
    {
        Code code =instance();
        boolean oldDebug=code._debugOn;
        if (code._debugOn && !debug)
            Code.debug(2,"DEBUG OFF");
        code._debugOn=debug;
        if (!oldDebug && debug)
            Code.debug(2,"DEBUG ON");
    }

    /* ------------------------------------------------------------ */
    /** Get the debug status.
     * @return the debug status
     */
    public static boolean getDebug()
    {
        return instance()._debugOn;
    }
    
    
    /* ------------------------------------------------------------ */
    /** Suppress stack trace.
     * @param stack if true stacks are not produced
     */
    public static void setSuppressStack(boolean stack)
    {
        instance()._suppressStack=stack;
    }

    /* ------------------------------------------------------------ */
    /** Get the stack suppression status.
     * @return the stack suppression status
     */
    public static boolean getSuppressStack()
    {
        return instance()._suppressStack;
    }
    

    /* ------------------------------------------------------------ */
    /** Set warning suppression.
     * @param warnings Warnings suppress if this is true and debug is false
     */
    public static void setSuppressWarnings(boolean warnings)
    {
        instance()._suppressWarnings=warnings;
    }
    
    /* ------------------------------------------------------------ */
    /** Get the warnings suppression status.
     * @return the warnings suppression status
     */
    public static boolean getSuppressWarnings()
    {
        return instance()._suppressWarnings;
    }

    
    /* ------------------------------------------------------------ */
    /** Set verbosity level.
     * @param verbose 
     */
    public static void setVerbose(int verbose)
    {
        instance()._verbose=verbose;
    }

    /* ------------------------------------------------------------ */
    /** Get the verbosity level.
     * @return the verbosity level
     */
    public static int getVerbose()
    {
        return instance()._verbose;
    }
    
    /* ------------------------------------------------------------ */
    /** Set debug patterns.
     * @param patterns comma separated string of patterns 
     */
    public static void setDebugPatterns(String patterns)
    {
        Code code = instance();
        code._patterns=patterns;
        if (patterns!=null && patterns.length()>0)
        {
            code._debugPatterns = new Vector();

            StringTokenizer tok = new StringTokenizer(patterns,", \t");
            while (tok.hasMoreTokens())
            {
                String pattern = tok.nextToken();
                code._debugPatterns.addElement(pattern);
            }
        }
        else
            code._debugPatterns = null;
    }

    /* ------------------------------------------------------------ */
    /** Get the debug patterns.
     * @return Coma separated list of debug patterns
     */
    public static String getDebugPatterns()
    {
        return instance()._patterns;
    }

    
    /*-------------------------------------------------------------------*/
    /** Check assertion that a boolean is true. Logs and throws
     * CodeException if
     * the assertion fails.
     * @param b The boolean to assert is true
     * @param m Message to log if assertion fails
     * @throws CodeException Thrown if assertion fails
     */
    public static void assertTrue(boolean b,String m)
    {
        if (!b)
        {
            Log.message(Log.ASSERT, m, new Frame(1));
            throw new CodeException("ASSERT FAIL: "+m);
        }
    }

    /*-------------------------------------------------------------------*/
    /** Check assertion that o1==o2.  Logs and throws CodeException if the
     * assertion fails.
     * @param o1 The first object to check
     * @param o2 The first object to check
     * @param m Message to log if assertion fails
     * @throws CodeException Thrown if assertion fails
     */
    public static void assertEquals(Object o1,Object o2,String m)
    {
        if (o1!=o2 && o1!=null && !o1.equals(o2))
        {
            Log.message(Log.ASSERT, o1+ " != "+o2+" : "+m, new Frame(1));
            throw new CodeException("ASSERT FAIL: "+m);
        }
    }
    
    /*-------------------------------------------------------------------*/
    /** Check assertion that o1==o2.  Logs and throws CodeException if the
     * assertion fails.
     * @param o1 The first long to check
     * @param o2 The first long to check
     * @param m Message to log if assertion fails
     * @throws CodeException Thrown if assertion fails
     */
    public static void assertEquals(long o1,long o2,String m)
    {
        if (o1!=o2)
        {
            Log.message(Log.ASSERT, o1+ " != "+o2+" : "+m, new Frame(1));
            throw new CodeException("ASSERT FAIL: "+m);
        }
    }
    
    /*-------------------------------------------------------------------*/
    /** Check assertion that o1==o2.  Logs and throws CodeException if the
     * assertion fails.
     * @param o1 The first double to check
     * @param o2 The first double to check
     * @param m Message to log if assertion fails
     * @throws CodeException Thrown if assertion fails
     */
    public static void assertEquals(double o1,double o2,String m)
    {
        if (o1!=o2)
        {
            Log.message(Log.ASSERT, o1+ " != "+o2+" : "+m, new Frame(1));
            throw new CodeException("ASSERT FAIL: "+m);
        }
    }

    /*-------------------------------------------------------------------*/
    /** Check assertion that o1==o2.  Logs and throws CodeException if the
     * assertion fails.
     * @param o1 The first char to check
     * @param o2 The first char to check
     * @param m Message to log if assertion fails
     * @throws CodeException Thrown if assertion fails
     */
    public static void assertEquals(char o1,char o2,String m)
    {
        if (o1!=o2)
        {
            Log.message(Log.ASSERT, o1+ " != "+o2+" : "+m, new Frame(1));
            throw new CodeException("ASSERT FAIL: "+m);
        }
    }

    /*-------------------------------------------------------------------*/
    /** Check assertion that a string is a substring of another.  Logs
     * and throws CodeException if the assertion fails.
     * @param string The string 
     * @param sub The sub string
     * @param m Message to log if assertion fails
     * @throws CodeException Thrown if assertion fails
     */
    public static void assertContains(String string,String sub,String m)
    {
        if (sub!=null && (string==null || string.indexOf(sub)==-1))
        {
            Log.message(Log.ASSERT, '"'+string+
                        "\" does not contain \"" + sub + 
                        "\" : "+m, new Frame(1));
            throw new CodeException("ASSERT FAIL: "+m);
        }
    }
    
    /*-------------------------------------------------------------------*/
    /** Programmer initiated warning, intended to be viewed by technical
     * not operational staff.
     * @param m The message body of the warning
     */
    public static void warning(String m)
    {
        Code code = instance();
        Frame frame = new Frame(1);
        if (!code._suppressWarnings || code.isDebugOnFor(frame) )
            Log.message(Log.WARN, m, frame);
    }
    
    /*-------------------------------------------------------------------*/
    /** Programmer initiated warning, intended to be viewed by technical
     * not operational staff.
     * @param m The message body of the warning
     * @param ex A Throwable object
     */
    public static void warning(String m, Throwable ex)
    {
        Code code = instance();
        Frame frame = new Frame(1);
        if (!code._suppressWarnings || code.isDebugOnFor(frame) )
            Log.message(Log.WARN,
                        new LogMsg(new Object[]{m,ex}),
                        frame);
    }
    
    /*-------------------------------------------------------------------*/
    /** Programmer initiated warning, intended to be viewed by technical
     * not operational staff.
     * @param ex A Throwable object
     */
    public static void warning(Throwable ex)
    {
        Code code = instance();
        Frame frame = new Frame(1);
        if (!code._suppressWarnings || code.isDebugOnFor(frame) )
            Log.message(Log.WARN,
                        new LogMsg(ex),
                        frame);
    }
    
    /*-------------------------------------------------------------------*/
    /** Programmer initiated fatal CodeException.
     * @param m The message body to log with the fatal error
     * @throws CodeException thrown to cause fatal error
     */
    public static void fail(String m) 
    {
        Log.message(Log.FAIL, m, new Frame(1));
        throw new CodeException("FAIL: "+m);
    }

    /*-------------------------------------------------------------------*/
    /** Programmer initiated fatal error.
     * @param m The message body to log with the fatal error
     * @param ex The Throwable to print the full stack trace of
     * @throws CodeException thrown to cause fatal error
     */
    public static void fail(String m, Throwable ex) 
    {
        Log.message(Log.FAIL,
                    new LogMsg(new Object[]{m,ex}),
                    new Frame(1));
        throw new CodeException("FAIL: "+m+ex.toString());
    }

    /*-------------------------------------------------------------------*/
    /** Programmer initiated fatal error.
     * @param ex The Throwable to print the full stack trace of
     * @throws CodeException thrown to cause fatal error
     */
    public static void fail(Throwable ex) 
    {
        Log.message(Log.FAIL,
                    new LogMsg(ex),
                    new Frame(1));
        throw new CodeException("FAIL: "+ex.toString());
    }

    /*-------------------------------------------------------------------*/
    /** Standard fatal error for a method not yet implemented.
     */
    public static void notImplemented() 
    {
        Log.message(Log.FAIL, "Not Implemented", new Frame(1));
        throw new CodeException("Not Implemented");
    }

    /* ------------------------------------------------------------ */
    /** Get verbosity level.
     * @return true if any level of verbosity is set.
     */
    public static boolean verbose()
    {
        return instance()._verbose>0;
    }
    
    /* ------------------------------------------------------------ */
    /** Get verbosity level.
     * @param v verbosity level to check against
     * @return true if current level of verbosity greater than or equal to v
     */
    public static boolean verbose(int v)
    {
        return instance()._verbose>=v;
    }
    
    /*-------------------------------------------------------------------*/
    /** Get debug status for the current stack frame.
     * @return true if debug is on for this stack frame
     */
    public static boolean debug()
    {
        Code code = instance();
        if (code._debugOn)
        {
            Frame frame = new Frame(1,true);
            return code.isDebugOnFor(frame);
        }
        return false;
    }
    
    /*-------------------------------------------------------------------*/
    /** Programming debugging output sent to the Log.
     * For debug output to be generated, the property DEBUG must
     * be passed to the java VM.  If the property DEBUG_PATTERN is
     * set, it is used as a regular expression on the method, file and
     * thread names to filter debug generation.
     * <strong>Do not rely on side-effects</strong>
     * @param m The debug message to log.
     */
    public static void debug(String m)
    {
        Code code = instance();
        if (code._debugOn)
        {
            Frame frame = new Frame(1,true);
            if (code.isDebugOnFor(frame))
            {
                frame.complete();
                Log.message(Log.DEBUG, m, frame);
            }
        }
    }
    
    /*-------------------------------------------------------------------*/
    /** As debug(String)
     * <strong>Do not rely on side-effects</strong>
     * @param m The debug message to log.
     * @param ex The Throwable to print the full stack trace of
     */
    public static void debug(String m, Throwable ex)
    {
        Code code = instance();
        if (code._debugOn)
        {
            Frame frame = new Frame(1,true);
            if (code.isDebugOnFor(frame))
            {
                frame.complete();
                Log.message(Log.DEBUG,
                            new LogMsg(new Object[]{m,ex}),
                            frame);
            }
        }
    }
    
    /*-------------------------------------------------------------------*/
    /** As debug(String)
     * <strong>Dont rely on side-effects</strong>
     * @param ex The Throwable to print the full stack trace of
     */
    public static void debug(Throwable ex)
    {
        Code code = instance();
        if (code._debugOn)
        {
            Frame frame = new Frame(1,true);
            if (code.isDebugOnFor(frame))
            {
                frame.complete();
                Log.message(Log.DEBUG,
                            new LogMsg(ex),
                            frame);
            }
        }
    }
    
    /*-------------------------------------------------------------------*/
    /** Debug with frame depth.
     * @param depth Depth of debug frame, 1=caller, 2=callers caller...
     * @param o Object
     */
    public static void debug(int depth,Object o)
    {
        Code code = instance();
        if (code._debugOn)
        {
            Frame frame = new Frame(depth,true);
            if (code.isDebugOnFor(frame))
            {
                frame.complete();
                Log.message(Log.DEBUG,
                            new LogMsg(o),
                            frame);
            }
        }
    }
    
    /*-------------------------------------------------------------------*/
    /** As debug(String)
     * <strong>Do not rely on side-effects</strong>
     * @param o The Object to pass to the debug Log.
     */
    public static void debug(Object o)
    {
        Code code = instance();
        if (code._debugOn)
        {
            Frame frame = new Frame(1,true);
            if (code.isDebugOnFor(frame))
            {
                frame.complete();
                Log.message(Log.DEBUG,
                            new LogMsg(o),
                            frame);
            }
        }
    }
    
    /*-------------------------------------------------------------------*/
    /** As debug(String)
     * <strong>Do not rely on side-effects</strong>
     * @param o1 An Object to pass to the debug Log.
     * @param o2 Another Object to pass to the debug Log.
     */
    public static void debug(Object o1,Object o2)
    {
        Code code = instance();
        if (code._debugOn)
        {
            Frame frame = new Frame(1,true);
            if (code.isDebugOnFor(frame))
            {
                frame.complete();
                Log.message(Log.DEBUG,
                            new LogMsg(new Object[]{o1,o2}),
                            frame);
            }
        }
    }
    
    /*-------------------------------------------------------------------*/
    /** As debug(String)
     * <strong>Do not rely on side-effects</strong>
     * @param o1 An Object to pass to the debug Log.
     * @param l a long to wrap as a Long and pass to Log.
     */
    public static void debug(Object o1,long l)
    {
        Code code = instance();
        if (code._debugOn)
        {
            Frame frame = new Frame(1,true);
            if (code.isDebugOnFor(frame))
            {
                frame.complete();
                Log.message(Log.DEBUG,
                            new LogMsg(new Object[]{o1,new Long(l)}),
                            frame);
            }
        }
    }
    
    /*-------------------------------------------------------------------*/
    /** As debug(String)
     * <strong>Do not rely on side-effects</strong>
     * @param o1 An Object to pass to the debug Log.
     * @param o2 Another Object to pass to the debug Log.
     * @param o3 Another Object to pass to the debug Log.
     */
    public static void debug(Object o1,Object o2,Object o3)
    {
        Code code = instance();
        if (code._debugOn)
        {
            Frame frame = new Frame(1,true);
            if (code.isDebugOnFor(frame))
            {
                frame.complete();
                Log.message(Log.DEBUG,
                            new LogMsg(new Object[]{o1,o2,o3}),
                            frame);
            }
        }
    }
    
    /*-------------------------------------------------------------------*/
    /** As debug(String)
     * <strong>Do not rely on side-effects</strong>
     * @param o1 An Object to pass to the debug Log.
     * @param o2 Another Object to pass to the debug Log.
     * @param o3 Another Object to pass to the debug Log.
     * @param o4 Another Object to pass to the debug Log.
     */
    public static void debug(Object o1,Object o2,Object o3, Object o4)
    {
        Code code = instance();
        if (code._debugOn)
        {
            Frame frame = new Frame(1,true);
            if (code.isDebugOnFor(frame))
            {
                frame.complete();
                Log.message(Log.DEBUG,
                            new LogMsg(new Object[]{o1,o2,o3,o4}),
                            frame);
            }
        }
    }
    
    /*-------------------------------------------------------------------*/
    /** As debug(String)
     * <strong>Do not rely on side-effects</strong>
     * @param o1 An Object to pass to the debug Log.
     * @param l1 a long to wrap as a Long and pass to Log.
     * @param o2 Another Object to pass to the debug Log.
     * @param l2 Another long to wrap as a Long and pass to Log.
     */
    public static void debug(Object o1,long l1,Object o2, long l2)
    {
        Code code = instance();
        if (code._debugOn)
        {
            Frame frame = new Frame(1,true);
            if (code.isDebugOnFor(frame))
            {
                frame.complete();
                Log.message(Log.DEBUG,
                            new LogMsg(new Object[]{o1,new Long(l1),o2,new Long(l2)}),
                            frame);
            }
        }
    }
    
    /*-------------------------------------------------------------------*/
    /** As debug(String)
     * <strong>Do not rely on side-effects</strong>
     * @param o1 An Object to pass to the debug Log.
     * @param o2 Another Object to pass to the debug Log.
     * @param o3 Another Object to pass to the debug Log.
     * @param o4 Another Object to pass to the debug Log.
     * @param o5 Another Object to pass to the debug Log.
     */
    public static void debug(Object o1,Object o2,Object o3, Object o4,
                             Object o5)
    {
        Code code = instance();
        if (code._debugOn)
        {
            Frame frame = new Frame(1,true);
            if (code.isDebugOnFor(frame))
            {
                frame.complete();
                Log.message(Log.DEBUG,
                            new LogMsg(new Object[]{o1,o2,o3,o4,o5}),
                            frame);
            }
        }
    }
    /*-------------------------------------------------------------------*/
    /** As debug(String)
     * <strong>Do not rely on side-effects</strong>
     * @param o1 An Object to pass to the debug Log.
     * @param o2 Another Object to pass to the debug Log.
     * @param o3 Another Object to pass to the debug Log.
     * @param o4 Another Object to pass to the debug Log.
     * @param o5 Another Object to pass to the debug Log.
     * @param o6 Another Object to pass to the debug Log.
     */
    public static void debug(Object o1,Object o2,Object o3, Object o4,
                             Object o5,Object o6)
    {
        Code code = instance();
        if (code._debugOn)
        {
            Frame frame = new Frame(1,true);
            if (code.isDebugOnFor(frame))
            {
                frame.complete();
                Log.message(Log.DEBUG,
                            new LogMsg(new Object[]{o1,o2,o3,o4,o5,o6}),
                            frame);
            }
        }
    }
    /*-------------------------------------------------------------------*/
    /** As debug(String)
     * <strong>Do not rely on side-effects</strong>
     * @param o1 An Object to pass to the debug Log.
     * @param o2 Another Object to pass to the debug Log.
     * @param o3 Another Object to pass to the debug Log.
     * @param o4 Another Object to pass to the debug Log.
     * @param o5 Another Object to pass to the debug Log.
     * @param o6 Another Object to pass to the debug Log.
     * @param o7 Another Object to pass to the debug Log.
     */
    public static void debug(Object o1,Object o2,Object o3, Object o4,
                             Object o5,Object o6,Object o7)
    {
        Code code = instance();
        if (code._debugOn)
        {
            Frame frame = new Frame(1,true);
            if (code.isDebugOnFor(frame))
            {
                frame.complete();
                Log.message(Log.DEBUG,
                            new LogMsg(new Object[]{o1,o2,o3,o4,o5,o6,o7}),
                            frame);
            }
        }
    }
    /*-------------------------------------------------------------------*/
    /** As debug(String)
     * <strong>Do not rely on side-effects</strong>
     * @param o1 An Object to pass to the debug Log.
     * @param o2 Another Object to pass to the debug Log.
     * @param o3 Another Object to pass to the debug Log.
     * @param o4 Another Object to pass to the debug Log.
     * @param o5 Another Object to pass to the debug Log.
     * @param o6 Another Object to pass to the debug Log.
     * @param o7 Another Object to pass to the debug Log.
     * @param o8 Another Object to pass to the debug Log.
     */
    public static void debug(Object o1,Object o2,Object o3,Object o4,
                             Object o5,Object o6,Object o7,Object o8)
    {
        Code code = instance();
        if (code._debugOn)
        {
            Frame frame = new Frame(1,true);
            if (code.isDebugOnFor(frame))
            {
                frame.complete();
                Log.message(Log.DEBUG,
                            new LogMsg(new Object[]{o1,o2,o3,o4,o5,o6,o7,o8}),
                            frame);
            }
        }
    }
    
    
    /*-------------------------------------------------------------------*/
    /** Ignore an exception.
     * @param ex The Throwable to print the full stack trace of
     */
    public static void ignore(Throwable ex)
    {
        Code code = instance();
        if (code._debugOn && code._verbose>0)
        {
            Frame frame = new Frame(1,true);
            if (code.isDebugOnFor(frame))
            {
                frame.complete();
                if (code._verbose>1)
                    Log.message(Log.DEBUG,
                                new LogMsg(new Object[]{"IGNORED ",ex}),
                                frame);
                else
                    Log.message(Log.DEBUG,
                                new LogMsg(new Object[]{"IGNORED ",ex.toString()}),
                                frame);
            }
        }
    }
    
    /*-------------------------------------------------------------------*/
    private boolean isDebugOnFor(Frame frame)
    {
        if (_debugOn)
        {
            if (_debugPatterns==null)
                return true;
            else
            {
                int i = _debugPatterns.size();
                for (;--i>=0;)
                {
                    if(frame.getWhere().indexOf((String)_debugPatterns
                                                .elementAt(i))>=0)
                        return true;
                }
            }
        }
        return false;
    }
    
    /*-------------------------------------------------------------------*/
    private static void formatObject(StringBuffer buf,Object o)
    {
        if (o==null)
            buf.append("null");
        else if (o instanceof Throwable)
        {
            Throwable ex = (Throwable) o;
            buf.append('\n');
                
            if (Code.instance()._suppressStack)
            {
                buf.append(ex.toString());
                buf.append("\nNo stack available\n--");
            }
            else
            {
                synchronized(__printWriter)
                {
                    __stringBufferWriter.setStringBuffer(buf);
                    expandThrowable(ex);
                    __printWriter.flush();
                }
            }
        }
        else
            buf.append(o.toString());   
    }

    /* ------------------------------------------------------------ */
    private synchronized static void expandThrowable(Throwable ex)
    {
        ex.printStackTrace(__printWriter);

        if (ex instanceof MultiException)
        {
            MultiException mx = (MultiException)ex;
            
            for (int i=0;i<mx.size();i++)
            {
                __printWriter.print("["+i+"]=");
                Throwable ex2=mx.getException(i);
                expandThrowable(ex2);
            }
        }
        else
        {
            for (int i=0;i<__nestedEx.length;i++)
            {
                try
                {
                    Method getTargetException =
                        ex.getClass().getMethod(__nestedEx[i],__noArgs);
                    Throwable ex2=(Throwable)getTargetException.invoke(ex,null);
                    if (ex2!=null)
                    {
                        __printWriter.println(__nestedEx[i]+"():");
                        expandThrowable(ex2);
                    }
                }
                catch(Exception ignore){}
            }
        }
    }
    
    /*-------------------------------------------------------------------*/
    /** Log Message Object passed to the Log by Code.
     */
    public final static class LogMsg
    {
        private Object _item;
        private Object[] _items;

        private LogMsg(Object item)
        {
            _item=item;
        }
        
        private LogMsg(Object[] items)
        {
            _items=items;
        }

        public int size()
        {
            if (_items==null)
                return 1;
            return _items.length;
        }
        
        public Object item(int i)
        {
            if (_items==null && i==0)
                return _item;
            return _items[i];
        }

        public Throwable getThrowable()
        {
            for (int i=0;i<size();i++)
                if (item(i) instanceof Throwable)
                    return (Throwable)item(i);
            return null;
        }
        
        public String toString()
        {
            StringBuffer buf = new StringBuffer(256);
            synchronized(buf)
            {
                for (int i=0;i<size();i++)
                    formatObject(buf,item(i));
                return buf.toString();
            }
        }
    }
    
}

