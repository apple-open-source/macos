/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.proxy.compiler;

import java.lang.reflect.*;
import java.util.*;
import java.io.*;

/**
 * A simple bytecode assembler.
 *
 * @deprecated Use {@link ProxyCompiler} or Jakarta BCEL instead.
 *
 * @author Unknown
 * @version $Revision: 1.3 $
 */
public class ProxyAssembler
{
   // constant pool:
   Vector cv = new Vector();
   Hashtable ct = new Hashtable();
   Hashtable ut = new Hashtable();
   short cn = 1;

   // members:
   Vector members;
   AMember current;
   ByteArrayOutputStream code;	// current.code
   Stack stack;		// current.stack

   // other info:
   String className;
   int modifiers;
   Class superClass;
   Class interfaces[];

   public short getIndex(Object x) {
      Object n = ct.get(x);
      if (n == null) {
         n = new Short(cn++);
         ct.put(x, n);
         cv.addElement(x);
      }
      return ((Short)n).shortValue();
   }

   public short getUtfIndex(String x) {
      Object n = ut.get(x);
      if (n == null) {
         n = new Short(cn++);
         ut.put(x, n);
         int xlen = 2 + x.length(); // x.utfLength(), really
         ByteArrayOutputStream bytes = new ByteArrayOutputStream(xlen);
         DataOutputStream ds = new DataOutputStream(bytes);
         try {
            ds.writeByte(CONSTANT_UTF8);
            ds.writeUTF(x);
         } catch (IOException ee) {
            throw new RuntimeException(ee.toString());
         }
         cv.addElement(bytes.toByteArray());
      }
      return ((Short)n).shortValue();
   }

   public short getNTIndex(String name, String sig) {
      NameAndType nt = new NameAndType();
      nt.name = getUtfIndex(name);
      nt.sig = getUtfIndex(sig);
      return getIndex(nt);
   }

   public short getClassIndex( Class c )
   {
      short ci = getUtfIndex(c.getName().replace('.', '/'));
      short data[] = { CONSTANT_CLASS, ci };
      return getIndex( data );
   }

   public short getMemberIndex(Object cls, String name, Class ptypes[]) {
      if (cls instanceof Class) {
         Class c = (Class) cls;
         Member m;
         try {
            if (ptypes == null) {
               m = c.getField(name);
            } else if (name.equals("<init>")) {
               m = c.getConstructor(ptypes);
            } else {
               m = c.getMethod(name, ptypes);
            }
         } catch (NoSuchMethodException ee) {
            throw new IllegalArgumentException(ee+" in "+c);
         } catch (NoSuchFieldException ee) {
            throw new IllegalArgumentException(ee+" in "+c);
         }
         return getIndex(m);
      } else if (cls instanceof ProxyAssembler) {
         ProxyAssembler asm = (ProxyAssembler) cls;
         String sig = getSig(null, ptypes);
         AMember m = asm.findMember(sig, name);
         if (m == null) {
            throw new IllegalArgumentException(sig + " " + name);
         }
         return getIndex(m);
      } else {
         throw new IllegalArgumentException("not a type: "+cls);
      }
   }
   public short getMemberIndex(Object cls, String name) {
      return getMemberIndex(cls, name, null);
   }

   public static String getSig(Class t) {
      if (t == null) {
         return "";
      } else if (t.isPrimitive()) {
         if (false) {
            return "";
         } else if (t == Boolean.TYPE) {
            return "Z";
         } else if (t == Character.TYPE) {
            return "C";
         } else if (t == Byte.TYPE) {
            return "B";
         } else if (t == Short.TYPE) {
            return "S";
         } else if (t == Integer.TYPE) {
            return "I";
         } else if (t == Long.TYPE) {
            return "J";
         } else if (t == Float.TYPE) {
            return "F";
         } else if (t == Double.TYPE) {
            return "D";
         } else if (t == Void.TYPE) {
            return "V";
         } else {
            Class a = java.lang.reflect.Array.newInstance(t, 0).getClass();
            return getSig(a).substring(1);
         }
      } else if (t.isArray()) {
         return t.getName().replace('.', '/');
      } else {
         return "L" + t.getName().replace('.', '/') + ";";
      }
   }

   public static String getSig(Class rt, Class pt[]) {
      if (pt == null) {
         return getSig(rt);
      }
      StringBuffer sb = new StringBuffer();
      sb.append("(");
      for (int i = 0; i < pt.length; i++) {
         sb.append(getSig(pt[i]));
      }
      sb.append(")");
      sb.append(getSig(rt));
      return sb.toString();
   }

   boolean isInterface() {
      return Modifier.isInterface(modifiers);
   }

   public ProxyAssembler(String className, int modifiers,
                         Class superClass, Class interfaces[]) {
      if (interfaces == null)  interfaces = new Class[0];
      this.className = className;
      this.modifiers = modifiers;
      this.superClass = superClass;
      this.interfaces = interfaces;
      cv.addElement(null);	// the first cpool entry is unused
      members = new Vector();
      addMember(0, "", null, "" );
   }

   private static class AMember {
      int mods;
      int sp;
      int spmax;
      int locmax;
      int index;
      Class type;		// field or method return type
      String sig;
      String name;
      Vector attr;
      Stack stack;
      ByteArrayOutputStream code;
      ProxyAssembler asm;
   }
   private static class Attr {
      String name;
      Object data;
   }
   private static class AValue {	// found in the stack
      int num;
      Object type;
   }
   private static class NameAndType {
      short name;
      short sig;
      // must act as a hashtable key:
      public boolean equals(Object x) {
         if (x instanceof NameAndType) {
            NameAndType that = (NameAndType)x;
            return that.name == name && that.sig == sig;
         }
         return false;
      }
      public int hashCode() {
         return name + sig * 1000;
      }
   }

   public Object getCurrentMember() {
      return current;
   }
   public void setCurrentMember(Object m) {
      if (m == null) {
         m = members.elementAt(0);
      }
      current = (AMember) m;
      code = current.code;
      stack = current.stack;
   }

   AMember findMember(String sig, String name) {
      for (int i = 0; i < members.size(); i++) {
         AMember m = (AMember) members.elementAt(i);
         if (m.name.equals(name)) {
            if (!sig.startsWith("(") ? !m.sig.startsWith("(")
                : m.sig.startsWith(sig)) {
               return m;
            }
         }
      }
      return null;
   }


   void addExceptionAttribute( Class[] classes )
   {
      try
      {

         if ((classes == null) || (classes.length == 0))
            return;

         ByteArrayOutputStream baos = new ByteArrayOutputStream();
         DataOutputStream dos = new DataOutputStream( baos );
         short count = (short) classes.length;
         dos.writeShort( count );
         for (int iter=0; iter<classes.length; iter++)
         {
            dos.writeShort( getClassIndex(classes[iter]) );
         }
         dos.flush();
         baos.flush();
         addAttribute( "Exception", baos.toByteArray() );
      }
      catch (IOException cantHappen)
      {
         cantHappen.printStackTrace();
      }
   }

   AMember addMember(int mods, String sig,  Class[] exceptionClasses, String name) {
      String qsig = sig.substring(0, 1 + sig.indexOf(')'));
      AMember m = findMember(qsig, name);
      if (m != null) {
         setCurrentMember(m);
         current.mods |= mods;
         modifiers |= (mods & Modifier.ABSTRACT);
         return m;
      }
      m = new AMember();
      m.asm = this;
      if (isMethodSig(sig)) {
         m.code = new ByteArrayOutputStream();
         m.stack = new Stack();
      }
      m.sig = sig;
      m.name = name;
      m.attr = new Vector();
      m.index = members.size();
      m.mods = mods;
      members.addElement(m);
      setCurrentMember(m);
      this.addExceptionAttribute( exceptionClasses );
      return m;
   }

   public Object addMember(int mods, Class rtype, String name, Class ptypes[], Class[] exceptionClasses) {
      AMember m = addMember(mods, getSig(rtype, ptypes), exceptionClasses, name);
      if (ptypes != null && stack.size() == 0) {
         // push the arguments onto the stack
         if (!Modifier.isStatic(mods)) {
            declare(this);
         }
         for (int i = 0; i < ptypes.length; i++) {
            declare(ptypes[i]);
         }
      }
      m.type = rtype;
      this.addExceptionAttribute( exceptionClasses );
      return m;
   }

   public Object addMember(int mods, Class type, Class[] exceptionClasses, String name ) {
      return addMember(mods, type, name, null, exceptionClasses);
   }

   public void addAttribute(AMember m, String name, Object data) {
      if (m == null) {
         m = (AMember) members.elementAt(0);
      }
      Attr a = new Attr();
      a.name = name; a.data = data;
      m.attr.addElement(a);
   }

   public void addAttribute(String name, Object data) {
      addAttribute(current, name, data);
   }

   // instruction emitters
   private final static int opc_iconst_0 = 3,
      opc_bipush = 16,
      opc_sipush = 17,
      opc_ldc = 18,
      opc_ldc_w = 19,
      opc_ldc2_w = 20,
      opc_aaload = 50,
      opc_aastore = 83,
      opc_dup = 89,
      opc_getfield = 180,
      field_put = 1,
      field_static = -2,
      opc_invokevirtual = 182,
      opc_invokespecial = 183,
      opc_invokestatic = 184,
      opc_invokeinterface = 185,
      opc_new = 187,
      opc_newarray = 188,
      opc_anewarray = 189,
      opc_aload = 25,
      opc_aload_0 = 42,
      opc_wide = 196,
      opc_areturn = 176,
      opc_return = 177,
      opc_checkcast = 192,
      kind_a = 0,
      kind_i = -4,
      kind_l = -3,
      kind_f = -2,
      kind_d = -1,
      kind_b = 1,
      kind_c = 2,
      kind_s = 3;

   public int declare(Object t) {
      int n = current.sp;
      current.sp += 1;
      if (t == Double.TYPE || t == Long.TYPE) {
         current.sp += 1;
      }
      if (current.spmax < current.sp) {
         current.spmax = current.sp;
      }
      AValue se = new AValue();
      se.num = n; se.type = t;
      stack.push(se);
      return stack.size() - 1;
   }

   public void undeclare(Object t) {
      AValue se = (AValue) stack.pop();
      current.sp = se.num;
   }

   public void pushConstant(Object x) {
      int op = opc_ldc_w;
      if (x instanceof Integer) {
         declare(Integer.TYPE);
         int v = ((Integer)x).intValue();
         if (v >= -1 && v <= 5) {
            code.write(opc_iconst_0 + v);
            return;
         } else if ((v > -(1 << 7)) && (v < (1 << 7))) {
            code.write(opc_bipush);
            code.write(v);
            return;
         } else if ((v > -(1 << 15)) && (v < (1 << 15))) {
            code.write(opc_sipush);
            codeShort(v);
            return;
         }
      } else if (x instanceof Float) {
         declare(Float.TYPE);
      } else if (x instanceof String) {
         declare(String.class);
      } else if (x instanceof Long) {
         declare(Long.TYPE);
         op = opc_ldc2_w;
      } else if (x instanceof Double) {
         declare(Double.TYPE);
         op = opc_ldc2_w;
      } else {
         throw new RuntimeException("unexpected: "+x);
      }
      int xi = getIndex(x);
      if (op == opc_ldc_w && xi < (1 << 8)) {
         code.write(opc_ldc);
         code.write(xi);
      } else {
         code.write(op);
         codeShort(xi);
      }
   }
   public void pushConstant(int x) {
      pushConstant(new Integer(x));
   }

   public int pushLocal(int loc) {
      if (current.locmax < loc) {
         current.locmax = loc;
      }
      AValue se = (AValue) stack.elementAt(loc);
      int kind = typeKind(se.type, false);
      if (se.num <= 3) {
         code.write(opc_aload_0 + (kind * 4) + se.num);
      } else {
         codeWide(opc_aload + kind, se.num);
      }
      return declare(se.type);
   }

   public int dup() {
      code.write(opc_dup);
      return declare(stack.peek());
   }

   public void checkCast(Object t) {
      code.write(opc_checkcast);
      codeShort(getIndex(t));
      AValue se = (AValue) stack.pop();
      if (se.type instanceof Class && ((Class)se.type).isPrimitive()) {
         undeclare(Object.class); // get an error
         declare(t);
      }
      se.type = t;
   }

   public int pushNewArray(Object etype) {
      int kind = typeKind(etype, true);
      int tcode;
      Class t;
      switch (kind) {
       case kind_a:
          code.write(opc_anewarray);
          codeShort(getIndex(etype));
          return declare(Object[].class);
       case kind_f:
          tcode = 0x00000006;
          t = float[].class;
          break;
       case kind_d:
          tcode = 0x00000007;
          t = double[].class;
          break;
       case kind_i:
          tcode = 0x0000000a;
          t = int[].class;
          break;
       case kind_l:
          tcode = 0x0000000b;
          t = long[].class;
          break;
       case kind_b:
          if (etype == Boolean.TYPE) {
             tcode = 0x00000004;
             t = boolean[].class;
          } else {
             tcode = 0x00000008;
             t = byte[].class;
          }
          break;
       case kind_c:
          tcode = 0x00000005;
          t = char[].class;
          break;
       case kind_s:
          tcode = 0x00000009;
          t = short[].class;
          break;
       default:
          return 0;
      }
      code.write(opc_newarray);
      code.write(tcode);
      return declare(t);	// etype[]
   }

   public void setElement(Object etype) {
      int kind = typeKind(etype, true);
      code.write(opc_aastore + kind);
      undeclare(etype);
      undeclare(Integer.TYPE);
      undeclare(null);	// etype[]
   }

   public void pushElement(Object etype) {
      int kind = typeKind(etype, true);
      code.write(opc_aaload + kind);
      undeclare(Integer.TYPE);
      undeclare(null);	// etype[]
      declare(etype);
   }

   public void ret() {
      if (current.sig.endsWith("V")) {
         code.write(opc_return);
         return;
      }
      Object t = current.type;
      undeclare(t);
      code.write(opc_areturn + typeKind(t, false));
      stack = null;
   }

   private int dofield(Object cls, String name, boolean isPut) {
      int fi = getMemberIndex(cls, name);
      Object x = cv.elementAt(fi);
      int op = opc_getfield;
      int mod;
      Object t;
      if (x instanceof Field) {
         Field f = (Field) x;
         mod = f.getModifiers();
         t = f.getType();
      } else {
         AMember m = (AMember) x;
         mod = m.mods;
         t = m.type;
      }
      if (isPut) {
         op += field_put;
         undeclare(t);
      }
      if (Modifier.isStatic(mod)) {
         op += field_static;
      } else {
         undeclare(cls);
      }
      code.write(op);
      codeShort(fi);
      return isPut ? -1 : declare(t);
   }

   public int pushField(Object cls, String name) {
      return dofield(cls, name, false);
   }

   public void setField(Object cls, String name) {
      dofield(cls, name, true);
   }

   public int invoke(Object cls, String name, Class ptypes[]) {
      int mi = getMemberIndex(cls, name, ptypes);
      Object x = cv.elementAt(mi);
      int mod;
      Object rtype;
      int op = opc_invokevirtual;
      if (x instanceof Method) {
         Method m = (Method)x;
         mod = m.getModifiers();
         rtype = m.getReturnType();
         if (m.getDeclaringClass().isInterface()) {
            op = opc_invokeinterface;
         }
      } else if (x instanceof Constructor) {
         Constructor m = (Constructor)x;
         mod = m.getModifiers();
         rtype = Void.TYPE;
         op = opc_invokespecial;
      } else {
         AMember m = (AMember) x;
         mod = m.mods;
         rtype = m.type;
         if (m.asm.isInterface()) {
            op = opc_invokeinterface;
         }
      }
      if (Modifier.isStatic(mod)) {
         op = opc_invokestatic;
      } else {
         undeclare(cls);
      }
      for (int i = ptypes.length; --i >= 0; ) {
         undeclare(ptypes[i]);
      }
      code.write(op);
      codeShort(mi);
      return declare(rtype);
   }

   private int typeKind(Object t, boolean subwords) {
      if (t != null && t instanceof Class && ((Class)t).isPrimitive()) {
         if (t == Float.TYPE) {
            return kind_f;
         } else if (t == Long.TYPE) {
            return kind_l;
         } else if (t == Double.TYPE) {
            return kind_d;
         } else if (t == Integer.TYPE || !subwords) {
            return kind_i;
         } else if (t == Character.TYPE) {
            return kind_c;
         } else if (t == Short.TYPE) {
            return kind_s;
         } else {
            return kind_b;
         }
      } else {
         return kind_a;
      }
   }

   private void codeWide(int op, int n) {
      if (n < (1 << 8)) {
         code.write(op);
         code.write(n);
      } else {
         code.write(opc_wide);
         code.write(op);
         codeShort(n);
      }
   }

   private void codeShort(int v) {
      code.write(v >>> 8);
      code.write(v);
   }

   private static boolean isMethodSig(String sig) {
      return (sig.indexOf('(') >= 0);
   }

   public byte[] getCode() {
      try {
         return internalGetCode();
      } catch (IOException ee) {
         throw new RuntimeException(ee.toString());
      }
   }

   public byte[] internalGetCode() throws IOException {
      // first, flush out all references to the cpool
      getIndex(this);
      getIndex(superClass);
      for (int i = 0; i < interfaces.length; i++) {
         getIndex(interfaces[i]);
      }
      int nfields = 0;
      int nmethods = 0;
      for (int i = 0; i < members.size(); i++) {
         AMember m = (AMember) members.elementAt(i);

         if (m.code != null) {
            byte[] codeAttr = getMethodCode(m, m.code);
            if (codeAttr != null) {
               addAttribute(m, "Code", codeAttr);
            }
         }

         for (int j = 0; j < m.attr.size(); j++) {
            Attr a = (Attr) m.attr.elementAt(j);
            getUtfIndex(a.name);
         }

         if (m.name.length() == 0) {
            continue;
         }
         getUtfIndex(m.name);
         getUtfIndex(m.sig);
         if (isMethodSig(m.sig)) {
            nmethods += 1;
         } else {
            nfields += 1;
         }
      }
      // next, deal with internal references in the cpool
      for (int i = 0; i < cv.size(); i++) {
         Object x = cv.elementAt(i);
         if (x == null) {
            continue;
         } else if (x instanceof String) {
            String s = (String)x;
            short si = getUtfIndex(s);
            short data[] = { CONSTANT_STRING, si };
            x = data;
         }
         else if (x instanceof Class) {
            Class c = (Class)x;
            short ci = getUtfIndex(c.getName().replace('.', '/'));
            short data[] = { CONSTANT_CLASS, ci };
            x = data;
         } else if (x instanceof Field) {
            Field m = (Field)x;
            short ci = getIndex(m.getDeclaringClass());
            short nt = getNTIndex(m.getName(),
                                  getSig(m.getType()));
            short data[] = { CONSTANT_FIELD, ci, nt };
            x = data;
         } else if (x instanceof Constructor) {
            Constructor m = (Constructor)x;
            short ci = getIndex(m.getDeclaringClass());
            short nt = getNTIndex("<init>",
                                  getSig(Void.TYPE,
                                         m.getParameterTypes()));
            short data[] = { CONSTANT_METHOD, ci, nt };
            x = data;
         } else if (x instanceof Method) {
            Method m = (Method)x;
            Class c = m.getDeclaringClass();
            short kind = c.isInterface() ? CONSTANT_INTERFACEMETHOD
               : CONSTANT_METHOD;
            short ci = getIndex(c);
            short nt = getNTIndex(m.getName(),
                                  getSig(m.getReturnType(),
                                         m.getParameterTypes()));
            short data[] = {  kind, ci, nt };
            x = data;
         } else if (x instanceof ProxyAssembler) {
            ProxyAssembler asm = (ProxyAssembler)x;
            short ci = getUtfIndex(asm.className.replace('.', '/'));
            short data[] = { CONSTANT_CLASS, ci };
            x = data;
         } else if (x instanceof AMember) {
            AMember m = (AMember) x;
            short kind = !isMethodSig(m.sig) ? CONSTANT_FIELD
               : m.asm.isInterface() ? CONSTANT_INTERFACEMETHOD
               : CONSTANT_METHOD;
            short ci = getIndex(m.asm);
            short nt = getNTIndex(m.name, m.sig);
            short data[] = { kind, ci, nt };
            x = data;
         } else if (x instanceof NameAndType) {
            NameAndType nt = (NameAndType) x;
            short data[] = { CONSTANT_NAMEANDTYPE, nt.name, nt.sig };
            x = data;
         }
         cv.setElementAt(x, i); // update
      }

      ByteArrayOutputStream bytes = new ByteArrayOutputStream(400);
      DataOutputStream ds = new DataOutputStream(bytes);
      ds.writeInt(JAVA_MAGIC);
      ds.writeShort(JAVA_MINOR_VERSION);
      ds.writeShort(JAVA_VERSION);
      int cvsize = cv.size();
      ds.writeShort(cvsize);
      for (int i = 0; i < cv.size(); i++) {
         Object x = cv.elementAt(i);
         if (x == null) {
            continue;
         } else if (x instanceof short[]) {
            short data[] = (short[])x;
            ds.writeByte(data[0]);
            for (int j = 1; j < data.length; j++) {
               ds.writeShort(data[j]);
            }
         } else if (x instanceof byte[]) {
            ds.write((byte[])x);
         } else if (x instanceof Integer) {
            ds.writeByte(CONSTANT_INTEGER);
            ds.writeInt(((Integer)x).intValue());
            // (do other primitive literal types?)
         } else {
            throw new RuntimeException("unexpected");
         }
      }
      ds.writeShort(modifiers);
      ds.writeShort(getIndex(this));
      ds.writeShort(getIndex(superClass));
      ds.writeShort(interfaces.length);
      for (int i = 0; i < interfaces.length; i++) {
         ds.writeShort(getIndex(interfaces[i]));
      }
      for (int pass = 0; pass <= 1; pass++) {
         boolean methods = (pass > 0);
         ds.writeShort(methods ? nmethods : nfields);
         for (int i = 0; i < members.size(); i++) {
            AMember m = (AMember) members.elementAt(i);
            if (m.name.length() == 0 || isMethodSig(m.sig) != methods) {
               continue;
            }
            ds.writeShort(m.mods);
            ds.writeShort(getUtfIndex(m.name));
            ds.writeShort(getUtfIndex(m.sig));
            writeAttrs(ds, m.attr);
         }
      }
      AMember m0 = (AMember) members.elementAt(0);
      writeAttrs(ds, (Vector) m0.attr);

      // sanity check
      if (cvsize != cv.size()) {
         throw new RuntimeException("cvsize");
      }

      return bytes.toByteArray();
   }

   private byte[] getMethodCode(AMember m, ByteArrayOutputStream code) throws IOException {
      if (code.size() == 0) {
         if ((current.mods & (Modifier.NATIVE | Modifier.ABSTRACT)) == 0) {
            current.mods |= Modifier.ABSTRACT;
            modifiers |= Modifier.ABSTRACT;
         }
         return null;
      }
      ByteArrayOutputStream bytes
         = new ByteArrayOutputStream(code.size()+30);
      DataOutputStream ds = new DataOutputStream(bytes);
      int slop = 10; // ??
      int max_stack  = current.locmax + slop;
      int max_locals = current.spmax  + slop;
      ds.writeShort(max_stack);
      ds.writeShort(max_locals);
      ds.writeInt(code.size());
      code.writeTo(ds);
      ds.writeShort(0);	// exception_table.length

      Vector attrs = new Vector();
      for (int i = m.attr.size(); --i >= 0; ) {
         Attr ma = (Attr) m.attr.elementAt(i);
         if (ma.name.startsWith("Code.")) {
            m.attr.removeElementAt(i);
            ma.name = ma.name.substring("Code.".length());
            attrs.addElement(ma);
            getUtfIndex(ma.name);
         }
      }
      writeAttrs(ds, attrs);

      return bytes.toByteArray();
   }


   private void writeAttrs(DataOutputStream ds, Vector attrs) throws IOException {
      ds.writeShort(attrs.size());
      for (int i = 0; i < attrs.size(); i++) {
         Attr a = (Attr) attrs.elementAt(i);
         ds.writeShort(getUtfIndex(a.name));
         if (a.data instanceof byte[]) {
            byte[] xa = (byte[])a.data;
            ds.writeInt(xa.length);
            ds.write(xa);
         } else {
            throw new RuntimeException("unexpected");
         }
      }
   }

   private static final int JAVA_MAGIC                   = 0xcafebabe;

   private static final short JAVA_VERSION               = 45,
      JAVA_MINOR_VERSION         = 3;

   private static final short CONSTANT_UTF8              = 1,
      CONSTANT_UNICODE           = 2,
      CONSTANT_INTEGER           = 3,
      CONSTANT_FLOAT             = 4,
      CONSTANT_LONG              = 5,
      CONSTANT_DOUBLE            = 6,
      CONSTANT_CLASS             = 7,
      CONSTANT_STRING            = 8,
      CONSTANT_FIELD             = 9,
      CONSTANT_METHOD            = 10,
      CONSTANT_INTERFACEMETHOD   = 11,
      CONSTANT_NAMEANDTYPE       = 12;

}

