INCLUDES = -DCONFIG_DIR=\"$(sysconfdir)\"

bin_PROGRAMS = sm

noinst_HEADERS = sm.h
sm_SOURCES = aci.c \
             dispatch.c \
             feature.c \
             main.c \
             mm.c \
             object.c \
             pkt.c \
             pres.c \
             sess.c \
             sm.c \
             user.c \
             storage.c \
             storage_db.c \
             storage_fs.c \
             storage_mysql.c \
             storage_sqlite.c \
             storage_pgsql.c \
	     storage_oracle.c \
             storage_sqlite.c \
             mod_active.c \
             mod_announce.c \
             mod_deliver.c \
             mod_disco.c \
             mod_disco_publish.c \
             mod_echo.c \
             mod_help.c \
             mod_offline.c \
             mod_presence.c \
             mod_privacy.c \
             mod_roster.c \
             mod_session.c \
             mod_vacation.c \
             mod_validate.c \
             mod_iq_last.c \
             mod_iq_private.c \
             mod_iq_time.c \
             mod_iq_vcard.c \
             mod_iq_version.c \
             mod_template_roster.c

sm_LDADD = $(top_builddir)/sx/libsx.la \
           $(top_builddir)/mio/libmio.la \
           $(top_builddir)/util/libutil.la \
           $(top_builddir)/subst/libsubst.la \
           $(top_builddir)/expat/libexpat.la
