#!/bin/sh

BLOJSOM_WEBINF="$DSTROOT/Library/Tomcat/blojsom_root/webapps/ROOT/WEB-INF/"
mv "$BLOJSOM_WEBINF/default/templates/html.vm" "$BLOJSOM_WEBINF/default/templates/html_bk.vm"
mv "$BLOJSOM_WEBINF/templates/html.vm" "$BLOJSOM_WEBINF/templates/html_bk.vm"
mv "$BLOJSOM_WEBINF/templates/html-comments.vm" "$BLOJSOM_WEBINF/templates/html-comments_bk.vm"
mv "$BLOJSOM_WEBINF/templates/html-trackback.vm" "$BLOJSOM_WEBINF/templates/html-trackback_bk.vm"
