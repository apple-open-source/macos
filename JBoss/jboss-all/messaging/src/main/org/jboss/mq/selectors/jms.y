//NL: This file should be used with Byacc.

%{

package org.spydermq.selectors;

import java.util.StringTokenizer;
import java.util.HashMap;
import java.util.HashSet;

%}
     
// YACC Declarations 

%token IDENTIFIER STRING DOUBLE LONG CST 
%token NOT EQUAL GT GE LT LE DIFFERENT NEG BETWEEN AND2 ESCAPE LIKE NULL IN IS 

%left OR
%left AND
%nonassoc EQUAL GT GE LT LE DIFFERENT BETWEEN LIKE IN
%left '+' '-'
%left '*' '/'
%right NOT
%right NEG

%start total
     
// Grammar follows
%%

total:	comp '|'							{ selector = $1.obj; }
		;

unary:	IDENTIFIER				
		| STRING				
		| DOUBLE				
		| LONG					
		| CST
		| '(' comp ')'						{ $$ = $2; }
		;

comp:	unary
		| comp '+' comp		    			{ $$.obj = new Operator(Operator.ADD,$1.obj,$3.obj); }
		| comp '-' comp		    			{ $$.obj = new Operator(Operator.SUB,$1.obj,$3.obj); }
		| comp '*' comp		    			{ $$.obj = new Operator(Operator.MUL,$1.obj,$3.obj); }
		| comp '/' comp		    			{ $$.obj = new Operator(Operator.DIV,$1.obj,$3.obj); }
		| comp AND comp		    			{ $$.obj = new Operator(Operator.AND,$1.obj,$3.obj); }
		| comp OR comp		    			{ $$.obj = new Operator(Operator.OR,$1.obj,$3.obj); }
		| comp EQUAL comp					{ $$.obj = new Operator(Operator.EQUAL,$1.obj,$3.obj); }
		| comp GT comp						{ $$.obj = new Operator(Operator.GT,$1.obj,$3.obj); }
		| comp GE comp						{ $$.obj = new Operator(Operator.GE,$1.obj,$3.obj); }
		| comp LT comp						{ $$.obj = new Operator(Operator.LT,$1.obj,$3.obj); }
		| comp LE comp						{ $$.obj = new Operator(Operator.LE,$1.obj,$3.obj); }
		| comp DIFFERENT comp				{ $$.obj = new Operator(Operator.DIFFERENT,$1.obj,$3.obj); }
		| comp IS NULL						{ $$.obj = new Operator(Operator.IS_NULL,$1.obj); }
		| comp IS NOT NULL					{ $$.obj = new Operator(Operator.IS_NOT_NULL,$1.obj); }
		| NOT comp							{ $$ = new parserval(new Operator(Operator.NOT,$2.obj)); }
		| '-' comp %prec NEG				{ $$ = new parserval(new Operator(Operator.NEG,$2.obj)); }
		| '+' comp %prec NEG				{ $$ = $2; }
		| comp BETWEEN comp AND2 comp		{ $$.obj = new Operator(Operator.BETWEEN,$1.obj,$3.obj,$5.obj); }
		| comp NOT BETWEEN comp AND2 comp	{ $$.obj = new Operator(Operator.NOT_BETWEEN,$1.obj,$4.obj,$6.obj); }
		| comp LIKE comp					{ $$.obj = new Operator(Operator.LIKE,$1.obj,$3.obj); }
		| comp NOT LIKE comp				{ $$.obj = new Operator(Operator.NOT_LIKE,$1.obj,$4.obj); }
		| comp LIKE comp ESCAPE comp		{ $$.obj = new Operator(Operator.LIKE_ESCAPE,$1.obj,$3.obj,$5.obj); }
		| comp NOT LIKE comp ESCAPE comp	{ $$.obj = new Operator(Operator.NOT_LIKE_ESCAPE,$1.obj,$4.obj,$6.obj); }
		| comp IN '(' strings ')'			{ $$.obj = new Operator(Operator.IN,$1.obj,$4.obj); }
		| comp NOT IN '(' strings ')'		{ $$.obj = new Operator(Operator.NOT_IN,$1.obj,$5.obj); }
		;
		
strings:	STRING							{ HashSet tmp=new HashSet(); tmp.add($1.obj); $$.obj=tmp; }
			| strings ',' STRING  			{ ((HashSet)$1.obj).add($3.obj); $$ = $1; }   
			//better than STRING ',' strings, this is a LALR parser :)
			;
 		
%%

	StringTokenizer st;
	String nextToken=null;
	Object selector;
	HashMap map;
	
	void yyerror(String s)
	{
		throw new RuntimeException("PARSER ERROR: "+s);
	}

	void aff(parserval val,String st)
	{
		System.out.print("["+st+"] ");
		if (val==null) System.out.println("null");
		else {
			if (val.obj==null) System.out.println(".obj=null");
			else System.out.println(".obj="+val.obj.toString());
		}
	}

	void next()
	{
		if (st.hasMoreTokens()) nextToken=st.nextToken();
		else nextToken=null;
	}
	
	int yylex()
	{
		yylval=null;

		String s = nextToken;
		if (s==null) return 0;
		next();		
		
		//Whitespace
		while (s.equals(" ")) {
			s=nextToken;
			if (s==null) return 0;
			next();
		}
		
		//String
		if (s.equals("'")) {
			
			String string="";
			while ((nextToken!=null)&&(!nextToken.equals("'"))) {
				string+=nextToken;
				next();
			}
			
			if (nextToken==null) return -1;
			next();
			
			yylval=new parserval((Object)string); 
			return STRING;
		}
		
		//Is it an integer/double ?
		if (Character.isDigit(s.charAt(0))) {
			try {
				yylval = new parserval(Long.valueOf(s)); 
				return LONG;
			} catch (NumberFormatException e) {
			}
			
			try {
				yylval = new parserval(Double.valueOf(s)); 
				return DOUBLE;
			} catch (NumberFormatException e) {
				return -1;
			}
		}
		
		//CST group
		if (s.equals("TRUE")) {
			yylval = new parserval((Object)Boolean.TRUE);
			return CST;
		}
		if (s.equals("FALSE")) {
			yylval = new parserval((Object)Boolean.FALSE);
			return CST;
		}
			
		//OPERATOR group 
		if (s.equals("NOT")) return NOT;
		if (s.equals("AND")) return AND;
		if (s.equals("OR")) return OR;
		if (s.equals("BETWEEN")) return BETWEEN;
		if (s.equals("and")) return AND2;
		if (s.equals("LIKE")) return LIKE;
		if (s.equals("IN")) return IN;
		if (s.equals("NULL")) return NULL;
		if (s.equals("IS")) return IS;
		if (s.equals("ESCAPE")) return ESCAPE;
		
		//BRACKET group 
		if (s.equals("(")) return '(';
		if (s.equals(")")) return ')';
		if (s.equals(",")) return ',';
		if (s.equals("|")) return '|';
		
		//COMP group
		if (s.equals("=")) return EQUAL;
		if (s.equals(">")) {
			if (nextToken!=null&&nextToken.equals("=")) { next(); return GE; }
			return GT;
		}
		if (s.equals("<")) {
			if (nextToken!=null&&nextToken.equals(">")) { next(); return DIFFERENT; }
			if (nextToken!=null&&nextToken.equals("=")) { next(); return LE; }
			return LT;
		}
		
		//CALC group
		if (s.equals("+")) return '+';
		if (s.equals("-")) return '-';
		if (s.equals("*")) return '*';
		if (s.equals("/")) return '/';
		
		//We should check if s is a _correct_ string
		Identifier id=(Identifier)map.get(s);
		if (id==null) {
			id=new Identifier(s);
			map.put(s,id);
		}
		yylval = new parserval(id);
		return IDENTIFIER;
	}

	Object parse(String sel,HashMap map)
      {
         parse(sel, map, false);
      }
	Object parse(String sel,HashMap map, boolean trace)
	{				
		selector=null;
		nextToken=null;
		this.map=map;
		
		sel+="|";
		st = new StringTokenizer(sel," '(),=><+-*/|",true);		
		next();
		yyparse();		
		return selector;
	}

