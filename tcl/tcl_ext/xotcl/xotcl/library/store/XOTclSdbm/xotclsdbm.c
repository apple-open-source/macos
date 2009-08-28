/*
 * xotclsdbm.c
 *
 * based on Tclndbm 0.5 by John Ellson (ellson@lucent.com)
 */

#include <stdio.h>
#include <tcl.h>
#include "sdbm.h"
#include <fcntl.h>
#include <xotcl.h>

#if (TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION<1)
# define TclObjStr(obj) Tcl_GetStringFromObj(obj, ((int*)NULL))
#else
# define TclObjStr(obj) Tcl_GetString(obj)
#endif

/*
 * a database ..
 */

typedef struct db_s {
  int mode;
  DBM *db;
} db_t ;

static int
XOTclSdbmOpenMethod(ClientData cd, Tcl_Interp* in, int objc, Tcl_Obj* CONST objv[]) {
  int mode;
  db_t *db;
  XOTcl_Object* obj = (XOTcl_Object *) cd;
/*
  int i;
  fprintf(stderr, "Method=XOTclSdbmOpenMethod\n");
  for (i=0; i< objc; i++)
    fprintf(stderr, "   objv[%d]=%s\n",i,TclObjStr(objv[i]));
*/
  if (!obj) return XOTclObjErrType(in, obj->cmdName, "Object");
  if (objc != 2)
    return XOTclObjErrArgCnt(in, obj->cmdName, "open filename");

    /*
     * check mode string if given
     *
    mode = O_RDONLY ;
    if (argc == 3) {

        if (strcmp(argv[2],"r")==0)
            mode = O_RDONLY ;
        else if (strcmp(argv[2],"rw")==0)
            mode = O_RDWR | O_SYNC ;
        else if (strcmp(argv[2],"rwc")==0)
            mode = O_CREAT | O_RDWR | O_SYNC ;
        else if (strcmp(argv[2],"rwn")==0)
            mode = O_CREAT | O_EXCL | O_RDWR | O_SYNC ;
        else {
            sprintf(buf, BAD_MODE, argv[0], argv[2]);
            Tcl_AppendResult (interp,buf,(char *)0);
            return (TCL_ERROR);
        }
    }
   */
  /* Storage interface at the moment assumes mode=rwc */
#ifdef O_SYNC
  mode = O_CREAT | O_RDWR | O_SYNC;
#else
  mode = O_CREAT | O_RDWR;
#endif

  /* name not in hashtab - create new db */
  if (XOTclGetObjClientData(obj))
    return XOTclVarErrMsg(in, "Called open on '", TclObjStr(obj->cmdName),
			  "', but open database was not closed before.", 0);

  db = (db_t*) ckalloc (sizeof(db_t));

  /*
   * create new name and malloc space for it
   * malloc extra space for name
  db->name = (char *) malloc (strlen(buf)+1) ;
    if (!db->name) {
        perror ("malloc for name in db_open");
        exit (-1);
        }
    strcpy(db->name,buf);
  */

  db->mode = mode;
  db->db = sdbm_open(TclObjStr(objv[1]), mode, 0644);

  if (!db->db) {
        /*
         * error occurred
         * free previously allocated memory
         */
    /*ckfree ((char*) db->name);*/
    ckfree ((char*) db);
    db = (db_t*) NULL ;

    return XOTclVarErrMsg(in, "Open on '", TclObjStr(obj->cmdName),
			  "' failed with '", TclObjStr(objv[1]),"'.", 0);
  } else {
    /*
     * success
     */
    XOTclSetObjClientData(obj, (ClientData) db);
    return TCL_OK;
  }
}

static int
XOTclSdbmCloseMethod(ClientData cd, Tcl_Interp* in, int objc, Tcl_Obj* CONST objv[]) {
  db_t *db;
  XOTcl_Object* obj = (XOTcl_Object *) cd;

  if (!obj) return XOTclObjErrType(in, obj->cmdName, "Object");
  if (objc != 1)
    return XOTclObjErrArgCnt(in, obj->cmdName, "close");

  db = (db_t*) XOTclGetObjClientData(obj);
  if (!db)
    return XOTclVarErrMsg(in, "Called close on '", TclObjStr(obj->cmdName),
			  "', but database was not opened yet.", 0);
  sdbm_close (db->db);

  /*ckfree((char*)db->name);*/
  ckfree ((char*)db);
  XOTclSetObjClientData(obj, 0);

  return TCL_OK;
}

static int
XOTclSdbmNamesMethod(ClientData cd, Tcl_Interp* in, int objc, Tcl_Obj* CONST objv[]) {
  XOTcl_Object* obj = (XOTcl_Object *) cd;
  Tcl_Obj *list;
  db_t *db;
  Tcl_DString result;
  datum key;

  if (!obj) return XOTclObjErrType(in, obj->cmdName, "Object");
  if (objc != 1)
    return XOTclObjErrArgCnt(in, obj->cmdName, "names");

  db = (db_t*) XOTclGetObjClientData(obj);
  if (!db)
    return XOTclVarErrMsg(in, "Called names on '", TclObjStr(obj->cmdName),
			  "', but database was not opened yet.", 0);
  Tcl_DStringInit(&result);

  key = sdbm_firstkey(db->db);
  if (!key.dptr) {
    /* empty db */
    return TCL_OK ;
  }

  /*
   * copy key to result and go to next key
   */
  list = Tcl_NewListObj(0, NULL);
  do {
    Tcl_ListObjAppendElement(in,list,Tcl_NewStringObj(key.dptr,(int)(key.dsize-1)));
      key = sdbm_nextkey(db->db);
  } while (key.dptr);
  Tcl_SetObjResult(in, list);

  return TCL_OK;
}

static int
XOTclSdbmSetMethod(ClientData cd, Tcl_Interp* in, int objc, Tcl_Obj* CONST objv[]) {
  XOTcl_Object* obj = (XOTcl_Object *) cd;
  db_t *db;
  datum key, content;

  if (!obj) return XOTclObjErrType(in, obj->cmdName, "Object");
  if (objc <2 || objc > 3)
    return XOTclObjErrArgCnt(in, obj->cmdName, "set key ?value?");

  db = (db_t*) XOTclGetObjClientData(obj);
  if (!db)
    return XOTclVarErrMsg(in, "Called set on '", TclObjStr(obj->cmdName),
			  "', but database was not opened yet.", 0);

  key.dptr = TclObjStr(objv[1]);
  key.dsize = objv[1]->length + 1;

  if (objc == 2) {
      /* get value */
      content = sdbm_fetch(db->db,key);
      if (content.dptr) {
	  /* found */
	Tcl_Obj *r = Tcl_NewStringObj(content.dptr, (int)(content.dsize-1));
	  Tcl_SetObjResult(in, r);
      } else {
	  /* key not found */
	  return XOTclVarErrMsg(in, "no such variable '", key.dptr,
				"'", 0);
      }
  } else {
      /* set value */
      if (db->mode == O_RDONLY) {
	  return XOTclVarErrMsg(in, "Trying to set '", TclObjStr(obj->cmdName),
				"', but database is in read mode.", 0);
      }
      content.dptr = TclObjStr(objv[2]);
      content.dsize = objv[2]->length + 1;
      if (sdbm_store(db->db, key, content, SDBM_REPLACE) == 0) {
	  /*fprintf(stderr,"setting %s to '%s'\n",key.dptr,content.dptr);*/
	  Tcl_SetObjResult(in, objv[2]);
      } else {
	  return XOTclVarErrMsg(in, "set of variable '", TclObjStr(obj->cmdName),
				"' failed.", 0);
      }
  }
  return TCL_OK;
}

static int
XOTclSdbmExistsMethod(ClientData cd, Tcl_Interp* in, int objc, Tcl_Obj* CONST objv[]) {
  XOTcl_Object* obj = (XOTcl_Object *) cd;
  db_t *db;
  datum key, content;

  if (!obj) return XOTclObjErrType(in, obj->cmdName, "Object");
  if (objc != 2)
    return XOTclObjErrArgCnt(in, obj->cmdName, "exists variable");

  db = (db_t*) XOTclGetObjClientData(obj);
  if (!db)
      return XOTclVarErrMsg(in, "Called exists on '", TclObjStr(obj->cmdName),
			    "', but database was not opened yet.", 0);

  key.dptr = TclObjStr(objv[1]);
  key.dsize = objv[1]->length + 1;

  content = sdbm_fetch(db->db,key);
  Tcl_SetIntObj(Tcl_GetObjResult(in), content.dptr != NULL);

  return TCL_OK;
}



static int
XOTclSdbmUnsetMethod(ClientData cd, Tcl_Interp* in, int objc, Tcl_Obj* CONST objv[]) {
  XOTcl_Object* obj = (XOTcl_Object *) cd;
  db_t *db;
  datum key;
  int ret;

  if (!obj) return XOTclObjErrType(in, obj->cmdName, "Object");
  if (objc != 2)
    return XOTclObjErrArgCnt(in, obj->cmdName, "unset key");

  db = (db_t*) XOTclGetObjClientData(obj);
  if (!db)
    return XOTclVarErrMsg(in, "Called unset on '", TclObjStr(obj->cmdName),
			  "', but database was not opened yet.", 0);
  /* check for read mode */
  if (db->mode == O_RDONLY) {
    return XOTclVarErrMsg(in, "Called unset on '", TclObjStr(obj->cmdName),
			  "', but database is in read mode.", 0);
  }

  key.dptr = TclObjStr(objv[1]);
  key.dsize = objv[1]->length + 1;

  ret = sdbm_delete(db->db, key);

  if (ret == 0) {
    return TCL_OK;
  } else {
    return XOTclVarErrMsg(in, "Tried to unset '", TclObjStr(objv[1]),
			  "' but key does not exist.", 0);
  }
}

/*
 * ndbm_firstkey
 */

static int
XOTclSdbmFirstKeyMethod(ClientData cd, Tcl_Interp* in, int objc, Tcl_Obj* CONST objv[]) {
  XOTcl_Object* obj = (XOTcl_Object *) cd;
  db_t *db;
  datum key;

  if (!obj) return XOTclObjErrType(in, obj->cmdName, "Object");
  if (objc != 1)
    return XOTclObjErrArgCnt(in, obj->cmdName, "firstkey");

  db = (db_t*) XOTclGetObjClientData(obj);
  if (!db)
    return XOTclVarErrMsg(in, "Called unset on '", TclObjStr(obj->cmdName),
			  "', but database was not opened yet.", 0);


  key = sdbm_firstkey(db->db);
  if (!key.dptr) {
    /*
     * empty db
     */
    return TCL_OK;
  }

  Tcl_AppendResult (in, key.dptr, (char*)0);
  return TCL_OK;
}

static int
XOTclSdbmNextKeyMethod(ClientData cd, Tcl_Interp* in, int objc, Tcl_Obj* CONST objv[]) {
  XOTcl_Object* obj = (XOTcl_Object *) cd;
  db_t *db;
  datum  newkey;

  if (!obj) return XOTclObjErrType(in, obj->cmdName, "Object");
  if (objc != 1)
    return XOTclObjErrArgCnt(in, obj->cmdName, "nextkey");

  db = (db_t*) XOTclGetObjClientData(obj);
  if (!db)
    return XOTclVarErrMsg(in, "Called unset on '", TclObjStr(obj->cmdName),
			  "', but database was not opened yet.", 0);

  newkey = sdbm_nextkey(db->db);

  if (!newkey.dptr) {
    /*
     * empty db
     */
    return TCL_OK ;
  }

  Tcl_AppendResult (in, newkey.dptr, (char*)0);
  return TCL_OK ;
}

/*
 * Xotclsdbm_Init
 * register commands, init data structures
 */

/* this should be done via the stubs ... for the time being
   simply export */
#ifdef VISUAL_CC
DLLEXPORT extern int Xotclsdbm_Init(Tcl_Interp * in);
#endif

extern int
Xotclsdbm_Init(Tcl_Interp * in) {
  XOTcl_Class* cl;
  int result;

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(in, "8.1", 0) == NULL) {
        return TCL_ERROR;
    }
# ifdef USE_XOTCL_STUBS
    if (Xotcl_InitStubs(in, "1.1", 0) == NULL) {
        return TCL_ERROR;
    }
# endif
#else
    if (Tcl_PkgRequire(in, "Tcl", TCL_VERSION, 0) == NULL) {
        return TCL_ERROR;
    }
#endif
    Tcl_PkgProvide(in, "xotcl::store::sdbm", PACKAGE_VERSION);

#ifdef PACKAGE_REQUIRE_XOTCL_FROM_SLAVE_INTERP_WORKS_NOW
    if (Tcl_PkgRequire(in, "XOTcl", XOTCLVERSION, 0) == NULL) {
        return TCL_ERROR;
    }
#endif
    if (Tcl_PkgRequire(in, "xotcl::store", 0, 0) == NULL) {
        return TCL_ERROR;
    }
    result = Tcl_VarEval (in, "::xotcl::Class create Storage=Sdbm -superclass Storage",
			  (char *) NULL);
    if (result != TCL_OK)
      return result;
    /*{
      Tcl_Obj *res = Tcl_GetObjResult(in);
      fprintf(stderr,"res='%s'\n", TclObjStr(res));
      cl = XOTclGetClass(in, "Storage=Sdbm");
      fprintf(stderr,"cl=%p\n",cl);
      }*/

    cl = XOTclGetClass(in, "Storage=Sdbm");
    if (!cl) {
      return TCL_ERROR;
    }

    XOTclAddIMethod(in, cl, "open", XOTclSdbmOpenMethod, 0, 0);
    XOTclAddIMethod(in, cl, "close", XOTclSdbmCloseMethod, 0, 0);
    XOTclAddIMethod(in, cl, "set", XOTclSdbmSetMethod, 0, 0);
    XOTclAddIMethod(in, cl, "exists", XOTclSdbmExistsMethod, 0, 0);
    XOTclAddIMethod(in, cl, "names", XOTclSdbmNamesMethod, 0, 0);
    XOTclAddIMethod(in, cl, "unset", XOTclSdbmUnsetMethod, 0, 0);
    XOTclAddIMethod(in, cl, "firstkey", XOTclSdbmFirstKeyMethod, 0, 0);
    XOTclAddIMethod(in, cl, "nextkey", XOTclSdbmNextKeyMethod, 0, 0);

    Tcl_SetIntObj(Tcl_GetObjResult(in), 1);
    return TCL_OK;
}

extern int
Xotclsdbm_SafeInit(interp)
    Tcl_Interp *interp;
{
    return Xotclsdbm_Init(interp);
}
