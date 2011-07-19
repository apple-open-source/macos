#!/bin/sh

/usr/bin/createuser -U _postgres -D -S -R roundcube
/usr/bin/createdb -U _postgres -O roundcube roundcubemail
sudo -u _postgres /usr/bin/psql -f - roundcubemail <<EOF
ALTER USER roundcube WITH PASSWORD 'roundcubemail';
\c - roundcube
\i /usr/share/webmail/SQL/postgres.initial.sql
EOF
