/* $Id: dbdimp.c,v 1.2 2007/01/30 07:00:52 wkakes Exp $ */

#include "SQLiteXS.h"

DBISTATE_DECLARE;

#ifndef SvPV_nolen
#define SvPV_nolen(x) SvPV(x,PL_na)
#endif

#define SvPV_nolen_undef_ok(x) (SvOK(x) ? SvPV_nolen(x) : "undef")

#ifndef call_method
#define call_method(x,y) perl_call_method(x,y)
#endif

#ifndef call_sv
#define call_sv(x,y) perl_call_sv(x,y)
#endif

#define sqlite_error(h,xxh,rc,what) _sqlite_error(__FILE__, __LINE__, h, xxh, rc, what)
#if defined(__GNUC__) && (__GNUC__ > 2)
#  define sqlite_trace(level,fmt...) _sqlite_tracef(__FILE__, __LINE__, level, fmt)
#else
#  define sqlite_trace _sqlite_tracef_noline
#endif

void
sqlite_init(dbistate_t *dbistate)
{
    dTHR;
    DBIS = dbistate;
}

static void
_sqlite_error(char *file, int line, SV *h, imp_xxh_t *imp_xxh, int rc, char *what)
{
    dTHR;

    SV *errstr = DBIc_ERRSTR(imp_xxh);
    sv_setiv(DBIc_ERR(imp_xxh), (IV)rc);
    sv_setpv(errstr, what);
    sv_catpvf(errstr, "(%d) at %s line %d", rc, file, line);
    
    if (DBIS->debug >= 3) {
        PerlIO_printf(DBILOGFP, "sqlite error %d recorded: %s at %s line %d\n",
            rc, what, file, line);
    }
}

static void
_sqlite_tracef(char *file, int line, int level, const char *fmt, ...)
{
    dTHR;
    
    va_list ap;
    if (DBIS->debug >= level) {
        char format[8192];
        sqlite3_snprintf(8191, format, "sqlite trace: %s at %s line %d\n", fmt, file, line);
        va_start(ap, fmt);
        PerlIO_vprintf(DBILOGFP, format, ap);
        va_end(ap);
    }
}

static void
_sqlite_tracef_noline(int level, const char *fmt, ...)
{
    dTHR;
    
    va_list ap;
    if (DBIS->debug >= level) {
        char format[8192];
        sqlite3_snprintf(8191, format, "sqlite trace: %s\n", fmt);
        va_start(ap, fmt);
        PerlIO_vprintf(DBILOGFP, format, ap);
        va_end(ap);
    }
}

int
sqlite_db_login(SV *dbh, imp_dbh_t *imp_dbh, char *dbname, char *user, char *pass)
{
    dTHR;
    int retval;
    char *errmsg = NULL;

    if (DBIS->debug >= 3) {
        PerlIO_printf(DBILOGFP, "    login '%s' (version %s)\n",
            dbname, sqlite3_version);
    }

    if (sqlite3_open(dbname, &(imp_dbh->db)) != SQLITE_OK) {
        sqlite_error(dbh, (imp_xxh_t*)imp_dbh, 1, (char*)sqlite3_errmsg(imp_dbh->db));
        return FALSE;
    }
    DBIc_IMPSET_on(imp_dbh);

    imp_dbh->in_tran = FALSE;
    imp_dbh->unicode = FALSE;
    imp_dbh->functions = newAV();
    imp_dbh->aggregates = newAV();
    imp_dbh->timeout = SQL_TIMEOUT;
    
    imp_dbh->handle_binary_nulls = FALSE;

    sqlite3_busy_timeout(imp_dbh->db, SQL_TIMEOUT);

    if ((retval = sqlite3_exec(imp_dbh->db, "PRAGMA empty_result_callbacks = ON",
        NULL, NULL, &errmsg))
        != SQLITE_OK)
    {
        /*  warn("failed to set pragma: %s\n", errmsg); */
        sqlite_error(dbh, (imp_xxh_t*)imp_dbh, retval, errmsg);
        return FALSE;
    }

    if ((retval = sqlite3_exec(imp_dbh->db, "PRAGMA show_datatypes = ON",
        NULL, NULL, &errmsg))
        != SQLITE_OK)
    {
        /*  warn("failed to set pragma: %s\n", errmsg); */
        sqlite_error(dbh, (imp_xxh_t*)imp_dbh, retval, errmsg);
        return FALSE;
    }

    DBIc_ACTIVE_on(imp_dbh);

    return TRUE;
}

int
dbd_set_sqlite3_busy_timeout ( SV *dbh, int timeout )
{
  D_imp_dbh(dbh);
  if (timeout) {
    imp_dbh->timeout = timeout;
    sqlite3_busy_timeout(imp_dbh->db, timeout);
  }
  return imp_dbh->timeout;
}

int
sqlite_db_disconnect (SV *dbh, imp_dbh_t *imp_dbh)
{
    dTHR;
    DBIc_ACTIVE_off(imp_dbh);

    if (DBIc_is(imp_dbh, DBIcf_AutoCommit) == FALSE) {
        sqlite_db_rollback(dbh, imp_dbh);
    }

    if (sqlite3_close(imp_dbh->db) == SQLITE_BUSY) {
        /* active statements! */
        warn("closing dbh with active statement handles");
    }
    imp_dbh->db = NULL;

    av_undef(imp_dbh->functions);
    imp_dbh->functions = (AV *)NULL;

    av_undef(imp_dbh->aggregates);
    imp_dbh->aggregates = (AV *)NULL;

    return TRUE;
}

void
sqlite_db_destroy (SV *dbh, imp_dbh_t *imp_dbh)
{
    dTHR;
    if (DBIc_ACTIVE(imp_dbh)) {
        sqlite_db_disconnect(dbh, imp_dbh);
    }
    DBIc_IMPSET_off(imp_dbh);
}

int
sqlite_db_rollback(SV *dbh, imp_dbh_t *imp_dbh)
{
    dTHR;
    int retval;
    char *errmsg;

    if (imp_dbh->in_tran) {
        sqlite_trace(2, "ROLLBACK TRAN");
        if ((retval = sqlite3_exec(imp_dbh->db, "ROLLBACK TRANSACTION",
            NULL, NULL, &errmsg))
            != SQLITE_OK)
        {
            sqlite_error(dbh, (imp_xxh_t*)imp_dbh, retval, errmsg);
            return FALSE;
        }
        imp_dbh->in_tran = FALSE;
    }

    return TRUE;
}

int
sqlite_db_commit(SV *dbh, imp_dbh_t *imp_dbh)
{
    dTHR;
    int retval;
    char *errmsg;

    if (DBIc_is(imp_dbh, DBIcf_AutoCommit)) {
        warn("commit ineffective with AutoCommit");
        return TRUE;
    }

    if (imp_dbh->in_tran) {
        sqlite_trace(2, "COMMIT TRAN");
        if ((retval = sqlite3_exec(imp_dbh->db, "COMMIT TRANSACTION",
            NULL, NULL, &errmsg))
            != SQLITE_OK)
        {
            sqlite_error(dbh, (imp_xxh_t*)imp_dbh, retval, errmsg);
            return FALSE;
        }
        imp_dbh->in_tran = FALSE;
    }
    return TRUE;
}

int
sqlite_discon_all(SV *drh, imp_drh_t *imp_drh)
{
    dTHR;
    return FALSE; /* no way to do this */
}

SV *
sqlite_db_last_insert_id(SV *dbh, imp_dbh_t *imp_dbh, SV *catalog, SV *schema, SV *table, SV *field, SV *attr)
{
    return newSViv(sqlite3_last_insert_rowid(imp_dbh->db));
}

int
sqlite_st_prepare (SV *sth, imp_sth_t *imp_sth,
                char *statement, SV *attribs)
{
    dTHR;
    D_imp_dbh_from_sth;
    const char *extra;
    int retval = 0;

    if (!DBIc_ACTIVE(imp_dbh)) {
      sqlite_error(sth, (imp_xxh_t*)imp_sth, retval, "attempt to prepare on inactive database handle");
      return FALSE;
    }

    if (strlen(statement) < 1) {
      sqlite_error(sth, (imp_xxh_t*)imp_sth, retval, "attempt to prepare empty statement");
      return FALSE;
    }

    sqlite_trace(2, "prepare statement: %s", statement);
    imp_sth->nrow = -1;
    imp_sth->retval = SQLITE_OK;
    imp_sth->params = newAV();
    imp_sth->col_types = newAV();
    Newz(0, imp_sth->statement, strlen(statement)+1, char);
    
    if ((retval = sqlite3_prepare(imp_dbh->db, statement, -1, &(imp_sth->stmt), &extra))
        != SQLITE_OK)
    {
        if (imp_sth->stmt) {
            sqlite3_finalize(imp_sth->stmt);
        }
        sqlite_error(sth, (imp_xxh_t*)imp_sth, retval, (char*)sqlite3_errmsg(imp_dbh->db));
        return FALSE;
    }
    
    /* store the query for later re-use if required */
    Copy(statement, imp_sth->statement, strlen(statement)+1, char);

    DBIc_NUM_PARAMS(imp_sth) = sqlite3_bind_parameter_count(imp_sth->stmt);
    DBIc_NUM_FIELDS(imp_sth) = sqlite3_column_count(imp_sth->stmt);
    DBIc_IMPSET_on(imp_sth);
    
    return TRUE;
}

char *
sqlite_quote(imp_dbh_t *imp_dbh, SV *val)
{
    STRLEN len;
    char *cval = SvPV(val, len);
    SV *ret = sv_2mortal(NEWSV(0, SvCUR(val) + 2));
    sv_setpvn(ret, "", 0);

    while (len) {
      switch (*cval) {
        case '\'':
          sv_catpvn(ret, "''", 2);
          break;
        default:
          sv_catpvn(ret, cval, 1);
      }
      *cval++; len--;
    }
    return SvPV_nolen(ret);
}

void
sqlite_st_reset (SV *sth)
{
    D_imp_sth(sth);
    if (DBIc_IMPSET(imp_sth))
        sqlite3_reset(imp_sth->stmt);
}

int
sqlite_st_execute (SV *sth, imp_sth_t *imp_sth)
{
    dTHR;
    D_imp_dbh_from_sth;
    char *errmsg;
    const char *extra;
    int num_params = DBIc_NUM_PARAMS(imp_sth);
    int i;
    int retval;

    sqlite_trace(3, "execute");

    /* warn("execute\n"); */

    if (DBIc_ACTIVE(imp_sth)) {
         sqlite_trace(3, "execute still active, reset");
         if ((imp_sth->retval = sqlite3_reset(imp_sth->stmt)) != SQLITE_OK) {
             char *errmsg = (char*)sqlite3_errmsg(imp_dbh->db);
             sqlite_error(sth, (imp_xxh_t*)imp_sth, imp_sth->retval, errmsg);
             return FALSE;
         }
    }
    
    for (i = 0; i < num_params; i++) {
        SV *value = av_shift(imp_sth->params);
        SV *sql_type_sv = av_shift(imp_sth->params);
        int sql_type = SvIV(sql_type_sv);

        sqlite_trace(4, "params left in 0x%p: %d", imp_sth->params, 1+av_len(imp_sth->params));
        sqlite_trace(4, "bind %d type %d as %s", i, sql_type, SvPV_nolen_undef_ok(value));
        
        if (!SvOK(value)) {
            sqlite_trace(5, "binding null");
            retval = sqlite3_bind_null(imp_sth->stmt, i+1);
        }
        else if (sql_type >= SQL_NUMERIC && sql_type <= SQL_SMALLINT) {
#if defined(USE_64_BIT_INT)
            retval = sqlite3_bind_int64(imp_sth->stmt, i+1, SvIV(value));
#else
            retval = sqlite3_bind_int(imp_sth->stmt, i+1, SvIV(value));
#endif
        }
        else if (sql_type >= SQL_FLOAT && sql_type <= SQL_DOUBLE) {
            retval = sqlite3_bind_double(imp_sth->stmt, i+1, SvNV(value));
        }
        else if (sql_type == SQL_BLOB) {
            STRLEN len;
            char * data = SvPV(value, len);
            retval = sqlite3_bind_blob(imp_sth->stmt, i+1, data, len, SQLITE_TRANSIENT);
        }
        else {
            STRLEN len;
            char * data = SvPV(value, len);
            retval = sqlite3_bind_text(imp_sth->stmt, i+1, data, len, SQLITE_TRANSIENT);
        }
        
        if (value) {
            SvREFCNT_dec(value);
        }
        SvREFCNT_dec(sql_type_sv);
        if (retval != SQLITE_OK) {
            sqlite_error(sth, (imp_xxh_t*)imp_sth, retval, (char*)sqlite3_errmsg(imp_dbh->db));
            return -4;
        }
    }
    
    if ( (!DBIc_is(imp_dbh, DBIcf_AutoCommit)) && (!imp_dbh->in_tran) ) {
        sqlite_trace(2, "BEGIN TRAN");
        if ((retval = sqlite3_exec(imp_dbh->db, "BEGIN TRANSACTION",
            NULL, NULL, &errmsg))
            != SQLITE_OK)
        {
            sqlite_error(sth, (imp_xxh_t*)imp_sth, retval, errmsg);
            return -2;
        }
        imp_dbh->in_tran = TRUE;
    }
    
    imp_sth->nrow = 0;
    
    sqlite_trace(3, "Execute returned %d cols\n", DBIc_NUM_FIELDS(imp_sth));
    if (DBIc_NUM_FIELDS(imp_sth) == 0) {
        while ((retval = sqlite3_step(imp_sth->stmt)) != SQLITE_DONE) {
            if (retval == SQLITE_ROW) {
                continue;
            }
            sqlite3_finalize(imp_sth->stmt);
            sqlite_error(sth, (imp_xxh_t*)imp_sth, retval, (char*)sqlite3_errmsg(imp_dbh->db));
            return -5;
        }
        /* warn("Finalize\n"); */
        sqlite3_reset(imp_sth->stmt);
        imp_sth->nrow = sqlite3_changes(imp_dbh->db);
        DBIc_ACTIVE_on(imp_sth);
        /* warn("Total changes: %d\n", sqlite3_total_changes(imp_dbh->db)); */
        /* warn("Nrow: %d\n", imp_sth->nrow); */
        return imp_sth->nrow;
    }
    
    imp_sth->retval = sqlite3_step(imp_sth->stmt);
    switch (imp_sth->retval) {
        case SQLITE_ROW:
        case SQLITE_DONE: DBIc_ACTIVE_on(imp_sth);
                          sqlite_trace(5, "exec ok - %d rows, %d cols\n", imp_sth->nrow, DBIc_NUM_FIELDS(imp_sth));
                          return 0;
        default:          sqlite3_finalize(imp_sth->stmt);
                          sqlite_error(sth, (imp_xxh_t*)imp_sth, imp_sth->retval, (char*)sqlite3_errmsg(imp_dbh->db));
                          return -6;
    }
}

int
sqlite_st_rows (SV *sth, imp_sth_t *imp_sth)
{
    return imp_sth->nrow;
}

/* bind parameter
 * NB: We store the params instead of bind immediately because
 *     we might need to re-create the imp_sth->stmt (see top of execute() function)
 *     and so we can't lose these params
 */
int
sqlite_bind_ph (SV *sth, imp_sth_t *imp_sth,
                SV *param, SV *value, IV sql_type, SV *attribs,
                                int is_inout, IV maxlen)
{
    int pos;
    if (!SvIOK(param)) {
        int len;
        char *paramstring;
        paramstring = SvPV(param, len);
        if( paramstring[len] == 0 && strlen(paramstring) == len) {
            pos = sqlite3_bind_parameter_index(imp_sth->stmt, paramstring);
            if (pos==0)
                croak("Unknown named parameter");
            pos = 2 * (pos - 1);
        }
        else {
            croak("<param> could not be coerced to a C string");
        }
    }
    else {
        if (is_inout) {
            croak("InOut bind params not implemented");
        }
    }
    pos = 2 * (SvIV(param) - 1);
    sqlite_trace(3, "bind into 0x%p: %d => %s (%d) pos %d\n",
      imp_sth->params, SvIV(param), SvPV_nolen_undef_ok(value), sql_type, pos);
    av_store(imp_sth->params, pos, SvREFCNT_inc(value));
    av_store(imp_sth->params, pos+1, newSViv(sql_type));
    
    return TRUE;
}

int
sqlite_bind_col(SV *sth, imp_sth_t *imp_sth, SV *col, SV *ref, IV sql_type, SV *attribs)
{
    /* store the type */
    av_store(imp_sth->col_types, SvIV(col)-1, newSViv(sql_type));
    
    /* Allow default implementation to continue */
    return 1;
}

AV *
sqlite_st_fetch (SV *sth, imp_sth_t *imp_sth)
{
    AV *av;
    D_imp_dbh_from_sth;
    int numFields = DBIc_NUM_FIELDS(imp_sth);
    int chopBlanks = DBIc_is(imp_sth, DBIcf_ChopBlanks);
    int i, retval;

    sqlite_trace(6, "numFields == %d, nrow == %d\n", numFields, imp_sth->nrow);

    if (!DBIc_ACTIVE(imp_sth)) {
        return Nullav;
    }
    
    if (imp_sth->retval == SQLITE_DONE) {
        sqlite_st_finish(sth, imp_sth);
        return Nullav;
    }
    
    if (imp_sth->retval != SQLITE_ROW) {
        /* error */
        sqlite_st_finish(sth, imp_sth);
        sqlite_error(sth, (imp_xxh_t*)imp_sth, imp_sth->retval, (char*)sqlite3_errmsg(imp_dbh->db));
        return Nullav;
    }
    
    imp_sth->nrow++;
    
    av = DBIS->get_fbav(imp_sth);
    for (i = 0; i < numFields; i++) {
        int len;
        char * val;
        int col_type = sqlite3_column_type(imp_sth->stmt, i);
        SV **sql_type = av_fetch(imp_sth->col_types, i, 0);
        if (sql_type && SvOK(*sql_type)) {
            if (SvIV(*sql_type)) {
                col_type = SvIV(*sql_type);
            }
        }
        switch(col_type) {
            case SQLITE_INTEGER:
#if defined(USE_64_BIT_INT)
                sv_setiv(AvARRAY(av)[i], sqlite3_column_int64(imp_sth->stmt, i));
#else
                sv_setnv(AvARRAY(av)[i], (double)sqlite3_column_int64(imp_sth->stmt, i));
#endif
                break;
            case SQLITE_FLOAT:
                sv_setnv(AvARRAY(av)[i], sqlite3_column_double(imp_sth->stmt, i));
                break;
            case SQLITE_TEXT:
                val = (char*)sqlite3_column_text(imp_sth->stmt, i);
                len = sqlite3_column_bytes(imp_sth->stmt, i);
                if (chopBlanks) {
                    val = savepv(val);
                    while((len > 0) && (val[len-1] == ' ')) {
                       len--;
                    }
                    val[len] = '\0';
                }
                sv_setpvn(AvARRAY(av)[i], val, len);
                if (imp_dbh->unicode) {
                  SvUTF8_on(AvARRAY(av)[i]);
                } else {
                  SvUTF8_off(AvARRAY(av)[i]);
                }
                if (chopBlanks) Safefree(val);
                break;
            case SQLITE_BLOB:
                len = sqlite3_column_bytes(imp_sth->stmt, i);
                sv_setpvn(AvARRAY(av)[i], sqlite3_column_blob(imp_sth->stmt, i), len);
                SvUTF8_off(AvARRAY(av)[i]);
                break;
            default:
                sv_setsv(AvARRAY(av)[i], &PL_sv_undef);
                SvUTF8_off(AvARRAY(av)[i]);
                break;
        }
        SvSETMAGIC(AvARRAY(av)[i]);
    }
    
    imp_sth->retval = sqlite3_step(imp_sth->stmt);
    
    return av;
}

int
sqlite_st_finish (SV *sth, imp_sth_t *imp_sth)
{
    return sqlite_st_finish3(sth, imp_sth, 0);
}

int
sqlite_st_finish3 (SV *sth, imp_sth_t *imp_sth, int is_destroy)
{
    D_imp_dbh_from_sth;
    
    /* warn("finish statement\n"); */
    if (!DBIc_ACTIVE(imp_sth))
        return 1;

    DBIc_ACTIVE_off(imp_sth);
    
    av_clear(imp_sth->col_types);
    
    if (!DBIc_ACTIVE(imp_dbh))  /* no longer connected  */
        return 1;

	if (is_destroy) {
		return TRUE;
	}
	
    if ((imp_sth->retval = sqlite3_reset(imp_sth->stmt)) != SQLITE_OK) {
        char *errmsg = (char*)sqlite3_errmsg(imp_dbh->db);
        /* warn("finalize failed! %s\n", errmsg); */
        sqlite_error(sth, (imp_xxh_t*)imp_sth, imp_sth->retval, errmsg);
        return FALSE;
    }
    
    return TRUE;
}

void
sqlite_st_destroy (SV *sth, imp_sth_t *imp_sth)
{
    /* warn("destroy statement: %s\n", imp_sth->statement); */
    DBIc_ACTIVE_off(imp_sth);
    sqlite3_finalize(imp_sth->stmt);
    Safefree(imp_sth->statement);
    SvREFCNT_dec((SV*)imp_sth->params);
    SvREFCNT_dec((SV*)imp_sth->col_types);
    DBIc_IMPSET_off(imp_sth);
}

int
sqlite_st_blob_read (SV *sth, imp_sth_t *imp_sth,
                int field, long offset, long len, SV *destrv, long destoffset)
{
    return 0;
}

int
sqlite_db_STORE_attrib (SV *dbh, imp_dbh_t *imp_dbh, SV *keysv, SV *valuesv)
{
    dTHR;
    char *key = SvPV_nolen(keysv);
    char *errmsg;
    int retval;

    if (strEQ(key, "AutoCommit")) {
        if (SvTRUE(valuesv)) {
            /* commit tran? */
            if ( (!DBIc_is(imp_dbh, DBIcf_AutoCommit)) && (imp_dbh->in_tran) ) {
                sqlite_trace(2, "COMMIT TRAN");
                if ((retval = sqlite3_exec(imp_dbh->db, "COMMIT TRANSACTION",
                    NULL, NULL, &errmsg))
                    != SQLITE_OK)
                {
                    sqlite_error(dbh, (imp_xxh_t*)imp_dbh, retval, errmsg);
                    return TRUE;
                }
                imp_dbh->in_tran = FALSE;
            }
        }
        DBIc_set(imp_dbh, DBIcf_AutoCommit, SvTRUE(valuesv));
        return TRUE;
    }
    if (strEQ(key, "unicode")) {
      imp_dbh->unicode = !(! SvTRUE(valuesv));
      return TRUE;
    }
    return FALSE;
}

SV *
sqlite_db_FETCH_attrib (SV *dbh, imp_dbh_t *imp_dbh, SV *keysv)
{
    dTHR;
    char *key = SvPV_nolen(keysv);

    if (strEQ(key, "sqlite_version")) {
        return newSVpv(sqlite3_version,0);
    }
   if (strEQ(key, "unicode")) {
     return newSViv(imp_dbh->unicode ? 1 : 0);
   }

    return NULL;
}

int
sqlite_st_STORE_attrib (SV *sth, imp_sth_t *imp_sth, SV *keysv, SV *valuesv)
{
    char *key = SvPV_nolen(keysv);
    return FALSE;
}

int
type_to_odbc_type (int type)
{
    switch(type) {
        case SQLITE_INTEGER: return SQL_INTEGER;
        case SQLITE_FLOAT:   return SQL_DOUBLE;
        case SQLITE_TEXT:    return SQL_VARCHAR;
        case SQLITE_BLOB:    return SQL_BLOB;
        case SQLITE_NULL:    return SQL_UNKNOWN_TYPE;
        default:             return SQL_UNKNOWN_TYPE;
    }
}

SV *
sqlite_st_FETCH_attrib (SV *sth, imp_sth_t *imp_sth, SV *keysv)
{
    char *key = SvPV_nolen(keysv);
    SV *retsv = NULL;
    int i,n;

    if (!DBIc_ACTIVE(imp_sth)) {
    	return NULL;
    }
    
    /* warn("fetch: %s\n", key); */
    
    i = DBIc_NUM_FIELDS(imp_sth);

    if (strEQ(key, "NAME")) {
        AV *av = newAV();
/* warn("Fetch NAME fields: %d\n", i); */
        av_extend(av, i);
        retsv = sv_2mortal(newRV(sv_2mortal((SV*)av)));
        for (n = 0; n < i; n++) {
/* warn("Fetch col name %d\n", n); */
            const char *fieldname = sqlite3_column_name(imp_sth->stmt, n);
            if (fieldname) {
                /* warn("Name [%d]: %s\n", n, fieldname); */
                char *dot = instr(fieldname, ".");
                if (dot) /* drop table name from field name */
                    fieldname = ++dot;
                av_store(av, n, newSVpv(fieldname, 0));
            }
        }
    }
    else if (strEQ(key, "PRECISION")) {
        AV *av = newAV();
        retsv = sv_2mortal(newRV(sv_2mortal((SV*)av)));
    }
    else if (strEQ(key, "TYPE")) {
        AV *av = newAV();
        av_extend(av, i);
        retsv = sv_2mortal(newRV(sv_2mortal((SV*)av)));
        for (n = 0; n < i; n++) {
            const char *fieldtype = sqlite3_column_decltype(imp_sth->stmt, n);
            int type = sqlite3_column_type(imp_sth->stmt, n);
            /* warn("got type: %d = %s\n", type, fieldtype); */
            type = type_to_odbc_type(type);
            /* av_store(av, n, newSViv(type)); */
            if (fieldtype)
	            av_store(av, n, newSVpv(fieldtype, 0));
	        else
	        	av_store(av, n, newSVpv("VARCHAR", 0));
        }
    }
    else if (strEQ(key, "NULLABLE")) {
        AV *av = newAV();
        retsv = sv_2mortal(newRV(sv_2mortal((SV*)av)));
    }
    else if (strEQ(key, "SCALE")) {
        AV *av = newAV();
        retsv = sv_2mortal(newRV(sv_2mortal((SV*)av)));
    }
    else if (strEQ(key, "NUM_OF_FIELDS")) {
        retsv = sv_2mortal(newSViv(i));
    }

    return retsv;
}

static void
sqlite_db_set_result(sqlite3_context *context, SV *result, int is_error )
{
    STRLEN len;
    char *s;
    
    if ( is_error ) {
        s = SvPV(result, len);
        sqlite3_result_error( context, s, len );
        return;
    }

/* warn("result: %s\n", SvPV_nolen(result)); */
    if ( !SvOK(result) ) {
        sqlite3_result_null( context );
    } else if( SvIOK(result) ) {
        sqlite3_result_int( context, SvIV(result));
    } else if ( !is_error && SvIOK(result) ) {
        sqlite3_result_double( context, SvNV(result));
    } else {
        s = SvPV(result, len);
        sqlite3_result_text( context, s, len, SQLITE_TRANSIENT );
    }
}

static void
sqlite_db_func_dispatcher(sqlite3_context *context, int argc, sqlite3_value **value)
{
    dSP;
    int count;
    int i;
    SV *func;
    
    func = sqlite3_user_data(context);
    
    ENTER;
    SAVETMPS;
    
    PUSHMARK(SP);
    for ( i=0; i < argc; i++ ) {
        SV *arg;
        int len = sqlite3_value_bytes(value[i]);
        int type = sqlite3_value_type(value[i]);
        
        /* warn("func dispatch type: %d, value: %s\n", type, sqlite3_value_text(value[i])); */
        switch(type) {
            case SQLITE_INTEGER:
                arg = sv_2mortal(newSViv(sqlite3_value_int(value[i])));
                break;
            case SQLITE_FLOAT:
                arg = sv_2mortal(newSVnv(sqlite3_value_double(value[i])));
                break;
            case SQLITE_TEXT:
                arg = sv_2mortal(newSVpv(sqlite3_value_text(value[i]),len));
                break;
            case SQLITE_BLOB:
                arg = sv_2mortal(newSVpvn(sqlite3_value_blob(value[i]), len));
                break;
            default:
                arg = &PL_sv_undef;
        }

        XPUSHs(arg);
    }
    PUTBACK;

    count = call_sv(func, G_SCALAR|G_EVAL);
    
    SPAGAIN;

    /* Check for an error */
    if (SvTRUE(ERRSV) ) {
        sqlite_db_set_result( context, ERRSV, 1);
        POPs;
    } else if ( count != 1 ) {
        SV *err = sv_2mortal(newSVpvf( "function should return 1 argument, got %d",
                                       count ));

        sqlite_db_set_result( context, err, 1);
        /* Clear the stack */
        for ( i=0; i < count; i++ ) {
            POPs;
        }
    } else {
        sqlite_db_set_result( context, POPs, 0 );
    }
    
    PUTBACK;
    
    FREETMPS;
    LEAVE;
}

void
sqlite3_db_create_function( SV *dbh, const char *name, int argc, SV *func )
{
    D_imp_dbh(dbh);
    int rv;
    
    /* Copy the function reference */
    SV *func_sv = newSVsv(func);
    av_push( imp_dbh->functions, func_sv );

    /* warn("create_function %s with %d args\n", name, argc); */
    rv = sqlite3_create_function( imp_dbh->db, name, argc, SQLITE_UTF8,
                                 func_sv,
                                 sqlite_db_func_dispatcher, NULL, NULL );
    if ( rv != SQLITE_OK )
    {
        croak( "sqlite_create_function failed with error %s", 
               sqlite3_errmsg(imp_dbh->db) );
    }
}

typedef struct aggrInfo aggrInfo;
struct aggrInfo {
  SV *aggr_inst;
  SV *err;
  int inited;
};

static void
sqlite_db_aggr_new_dispatcher( sqlite3_context *context, aggrInfo *aggr_info )
{
    dSP;
    SV *pkg = NULL;
    int count = 0;

    aggr_info->err = NULL;
    aggr_info->aggr_inst = NULL;
    
    pkg = sqlite3_user_data(context);
    if ( !pkg )
        return;

    ENTER;
    SAVETMPS;
    
    PUSHMARK(SP);
    XPUSHs( sv_2mortal( newSVsv(pkg) ) );
    PUTBACK;

    count = call_method ("new", G_EVAL|G_SCALAR);
    SPAGAIN;

    aggr_info->inited = 1;

    if ( SvTRUE( ERRSV ) ) {
        aggr_info->err =  newSVpvf ("error during aggregator's new(): %s",
                                    SvPV_nolen (ERRSV));
        POPs;
    } else if ( count != 1 ) {
        int i;
        
        aggr_info->err = newSVpvf( "new() should return one value, got %d", 
                                  count );
        /* Clear the stack */
        for ( i=0; i < count; i++ ) {
            POPs;
        }
    } else {
        SV *aggr = POPs;
        if ( SvROK(aggr) ) {
            aggr_info->aggr_inst = newSVsv(aggr);
        } else{
            aggr_info->err = newSVpvf( "new() should return a blessed reference" );
        }
    }

    PUTBACK;

    FREETMPS;
    LEAVE;

    return;
}

static void
sqlite_db_aggr_step_dispatcher (sqlite3_context *context,
                                int argc, sqlite3_value **value)
{
    dSP;
    int i;
    aggrInfo *aggr;

    aggr = sqlite3_aggregate_context (context, sizeof (aggrInfo));
    if ( !aggr )
        return;
    
    ENTER;
    SAVETMPS;
    
    /* initialize on first step */
    if ( !aggr->inited ) {
        sqlite_db_aggr_new_dispatcher( context, aggr );
    }

    if ( aggr->err || !aggr->aggr_inst ) 
        goto cleanup;

    PUSHMARK(SP);
    XPUSHs( sv_2mortal( newSVsv( aggr->aggr_inst ) ));
    for ( i=0; i < argc; i++ ) {
        SV *arg;
        int len;
        int type = sqlite3_value_type(value[i]);
        
        switch(type) {
            case SQLITE_INTEGER:
                arg = sv_2mortal(newSViv(sqlite3_value_int(value[i])));
                break;
            case SQLITE_FLOAT:
                arg = sv_2mortal(newSVnv(sqlite3_value_double(value[i])));
                break;
            case SQLITE_TEXT:
                arg = sv_2mortal(newSVpv(sqlite3_value_text(value[i]), 0));
                break;
            case SQLITE_BLOB:
                len = sqlite3_value_bytes(value[i]);
                arg = sv_2mortal(newSVpvn(sqlite3_value_blob(value[i]), len));
                break;
            default:
                arg = &PL_sv_undef;
        }

        XPUSHs(arg);
    }
    PUTBACK;

    call_method ("step", G_SCALAR|G_EVAL|G_DISCARD);

    /* Check for an error */
    if (SvTRUE(ERRSV) ) {
      aggr->err = newSVpvf( "error during aggregator's step(): %s",
                            SvPV_nolen(ERRSV));
      POPs;
    }

 cleanup:
    FREETMPS;
    LEAVE;
}

static void
sqlite_db_aggr_finalize_dispatcher( sqlite3_context *context )
{
    dSP;
    aggrInfo *aggr, myAggr;
    int count = 0;

    aggr = sqlite3_aggregate_context (context, sizeof (aggrInfo));
    
    ENTER;
    SAVETMPS;
    
    if ( !aggr ) {
        /* SQLite seems to refuse to create a context structure
           from finalize() */
        aggr = &myAggr;
        aggr->aggr_inst = NULL;
        aggr->err = NULL;
        sqlite_db_aggr_new_dispatcher (context, aggr);
    } 

    if  ( ! aggr->err && aggr->aggr_inst ) {
        PUSHMARK(SP);
        XPUSHs( sv_2mortal( newSVsv( aggr->aggr_inst )) );
        PUTBACK;

        count = call_method( "finalize", G_SCALAR|G_EVAL );
        SPAGAIN;

        if ( SvTRUE(ERRSV) ) {
            aggr->err = newSVpvf ("error during aggregator's finalize(): %s",
                                  SvPV_nolen(ERRSV) ) ;
            POPs;
        } else if ( count != 1 ) {
            int i;
            aggr->err = newSVpvf( "finalize() should return 1 value, got %d",
                                  count );
            /* Clear the stack */
            for ( i=0; i<count; i++ ) {
                POPs;
            }
        } else {
            sqlite_db_set_result( context, POPs, 0 );
        }
        PUTBACK;
    }
    
    if ( aggr->err ) {
        warn( "DBD::SQLite: error in aggregator cannot be reported to SQLite: %s",               SvPV_nolen( aggr->err ) );
        
        /* sqlite_db_set_result( context, aggr->err, 1 ); */
        SvREFCNT_dec( aggr->err );
        aggr->err = NULL;
    }
    
    if ( aggr->aggr_inst ) {
         SvREFCNT_dec( aggr->aggr_inst );
         aggr->aggr_inst = NULL;
    }
    
    FREETMPS;
    LEAVE;
}

void
sqlite3_db_create_aggregate( SV *dbh, const char *name, int argc, SV *aggr_pkg )
{
    D_imp_dbh(dbh);
    int rv;
    
    /* Copy the aggregate reference */
    SV *aggr_pkg_copy = newSVsv(aggr_pkg);
    av_push( imp_dbh->aggregates, aggr_pkg_copy );

    rv = sqlite3_create_function( imp_dbh->db, name, argc, SQLITE_UTF8,
                                  aggr_pkg_copy,
                                  NULL,
                                  sqlite_db_aggr_step_dispatcher, 
                                  sqlite_db_aggr_finalize_dispatcher
                                );
    
    if ( rv != SQLITE_OK )
    {
        croak( "sqlite_create_aggregate failed with error %s", 
               sqlite3_errmsg(imp_dbh->db) );
    }
}

/* end */
