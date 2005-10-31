#!/usr/bin/perl

$DSTROOT = $ENV{'DSTROOT'};
$BLOJSOM_WEBINF = "$DSTROOT/Library/Tomcat/blojsom_root/webapps/ROOT/WEB-INF";

# remove the old jar files
unlink "$BLOJSOM_WEBINF/lib/blojsom-core-2.14.jar" if (-e "$BLOJSOM_WEBINF/lib/blojsom-core-2.14.jar");
unlink "$BLOJSOM_WEBINF/lib/blojsom-extensions-2.14.jar" if (-e "$BLOJSOM_WEBINF/lib/blojsom-extensions-2.14.jar");
unlink "$BLOJSOM_WEBINF/lib/blojsom-plugins-2.14.jar" if (-e "$BLOJSOM_WEBINF/lib/blojsom-plugins-2.14.jar");
unlink "$BLOJSOM_WEBINF/lib/blojsom-plugins-templates-2.14.jar" if (-e "$BLOJSOM_WEBINF/lib/blojsom-plugins-templates-2.14.jar");
unlink "$BLOJSOM_WEBINF/lib/blojsom-resources-2.14.jar" if (-e "$BLOJSOM_WEBINF/lib/blojsom-resources-2.14.jar");
unlink "$BLOJSOM_WEBINF/lib/commons-codec-1.2.jar" if (-e "$BLOJSOM_WEBINF/lib/commons-codec-1.2.jar");
unlink "$BLOJSOM_WEBINF/lib/sandler-1.01.jar" if (-e "$BLOJSOM_WEBINF/lib/sandler-1.01.jar");
unlink "$BLOJSOM_WEBINF/lib/xmlrpc-1.2-b1.jar" if (-e "$BLOJSOM_WEBINF/lib/xmlrpc-1.2-b1.jar");

# add RSS enclosure and podcast upload plug-ins to plugin lookup file
$found_rss_enclosure_plugin = 0;
$found_podcast_upload_plugin = 0;

if ( ! ( open PLUGIN_LOOKUP_FILE, "$BLOJSOM_WEBINF/plugin.properties" ) ) {
	print STDERR "Couldn't open $BLOJSOM_WEBINF/plugin.properties file for reading!\n";
} else {
	while ( <PLUGIN_LOOKUP_FILE> ) {
		$found_rss_enclosure_plugin = 1 if /^rss-enclosure=.+/;
		$found_podcast_upload_plugin = 1 if /^podcast-upload=.+/;
	}
	close PLUGIN_LOOKUP_FILE;
	if ( ( $found_rss_enclosure_plugin == 0 ) or ( $found_podcast_upload_plugin == 0 ) ) {
		if ( ! ( open PLUGIN_LOOKUP_FILE, '>>', "$BLOJSOM_WEBINF/plugin.properties" ) ) {
			print STDERR "Couldn't open $BLOJSOM_WEBINF/plugin.properties file for writing!\n";
		} else {
			print PLUGIN_LOOKUP_FILE "rss-enclosure=org.blojsom.plugin.common.RSSEnclosurePlugin\n" if $found_rss_enclosure_plugin == 0;
			print PLUGIN_LOOKUP_FILE "podcast-upload=com.apple.blojsom.plugin.podcastupload.PodcastUploadPlugin\n" if $found_podcast_upload_plugin == 0;
			close PLUGIN_LOOKUP_FILE;
		}
	}
}

# change the allowed file upload types and sizes to accomodate podcasting
if ( ! ( open UPLOAD_PROPERTIES_FILE, "$BLOJSOM_WEBINF/plugin-admin-upload.properties" ) ) {
	print STDERR "Couldn't open $BLOJSOM_WEBINF/plugin-admin-upload.properties file for reading!\n";
} else {
	$upload_properties = '';
	$upload_properties .= $_ while ( <UPLOAD_PROPERTIES_FILE> );
	close UPLOAD_PROPERTIES_FILE;
	$upload_properties =~ s/maximum-upload-size=100000\n/maximum-upload-size=52428800\n/;
	$upload_properties =~ s/accepted-file-types=image\/jpeg, image\/gif, image\/png\n/accepted-file-types=image\/jpeg, image\/gif, image\/png, application\/pdf, audio\/mpeg, audio\/x-m4a, video\/mpeg, video\/mp4, video\/quicktime, video\/3gpp, video\/3gp2\n/;
	if ( ! ( open UPLOAD_PROPERTIES_FILE, '>', "$BLOJSOM_WEBINF/plugin-admin-upload.properties" ) ) {
		print STDERR "Couldn't open $BLOJSOM_WEBINF/plugin-admin-upload.properties file for writing!\n";
	} else {
		print UPLOAD_PROPERTIES_FILE $upload_properties;
		close UPLOAD_PROPERTIES_FILE;
	}
}

# now iterate through the webinf dir looking for user settings directories
if ( ! ( opendir DIR, "$BLOJSOM_WEBINF" ) ) {
	print STDERR "Couldn't open directory $BLOJSOM_WEBINF for reading!\n";
} else {
	while ( $subdir = readdir DIR ) {
		if ( ! ( $subdir =~ /^\./ ) ) {
		
			# edit blog.properties file to add recursive categories pref and update accepted MIME types
			$blog_properties_file_loc = "$BLOJSOM_WEBINF/$subdir/blog.properties";
			if ( -e "$blog_properties_file_loc" ) {
				if ( ! ( open BLOG_PROPERTIES_FILE, "$blog_properties_file_loc" ) ) {
					print STDERR "Couldn't open file $blog_properties_file_loc for reading!\n";
				} else {
					$blog_properties = '';
					$blog_properties .= $_ while ( <BLOG_PROPERTIES_FILE> );
					close BLOG_PROPERTIES_FILE;
					$blog_properties .= 'recursive-categories=true\n' if ( ! $blog_properties =~ /recursive-categories=/ );
					$blog_properties =~ s/blojsom-extension-metaweblog-accepted-types=image\/jpeg, image\/gif, image\/png, img\n/blojsom-extension-metaweblog-accepted-types=audio\/mpeg, audio\/x-m4a, application\/pdf, image\/jpeg, image\/gif, image\/png, video\/mpeg, video\/mp4, video\/quicktime, video\/3gpp, video\/3gp2, img\n/;
					if ( ! ( open BLOG_PROPERTIES_FILE, '>', "$blog_properties_file_loc" ) ) {
						print STDERR "Couldn't open file $blog_properties_file_loc for writing!\n";
					} else {
						print BLOG_PROPERTIES_FILE $blog_properties;
						close BLOG_PROPERTIES_FILE;
					}
				}
			}
			
			# edit plugin.properties file to add new plug-ins to their respective flavor chains
			$plugin_properties_file_loc = "$BLOJSOM_WEBINF/$subdir/plugin.properties";
			if ( -e "$plugin_properties_file_loc" ) {
				if ( ! ( open PLUGIN_PROPERTIES_FILE, "$plugin_properties_file_loc" ) ) {
					print STDERR "Couldn't open file $plugin_properties_file_loc for reading!\n";
				} else {
					$plugin_properties = '';
					$plugin_properties .= $_ while ( <PLUGIN_PROPERTIES_FILE> );
					close PLUGIN_PROPERTIES_FILE;
					$plugin_properties =~ s/blojsom-plugin-chain=localizer, addhost, conditional-get, limiter, http-acl, convert-line-breaks, user-data\n/blojsom-plugin-chain=localizer, addhost, meta, rss-enclosure, conditional-get, limiter, http-acl, convert-line-breaks, user-data\n/;
					$plugin_properties =~ s/html.blojsom-plugin-chain=localizer, addhost, meta, inline-admin, referer-log, calendar-gui, calendar-filter, comment, trackback, sendemail, limiter, emoticons, macro-expansion, simple-search, acl, activate-urls, escape-tags, convert-line-breaks, user-data\n/html.blojsom-plugin-chain=localizer, addhost, meta, rss-enclosure, podcast-upload, inline-admin, referer-log, calendar-gui, calendar-filter, comment, trackback, sendemail, limiter, emoticons, macro-expansion, simple-search, acl, activate-urls, escape-tags, convert-line-breaks, user-data\n/;
					if ( ! ( open PLUGIN_PROPERTIES_FILE, '>', "$plugin_properties_file_loc" ) ) {
						print STDERR "Couldn't open file $plugin_properties_file_loc for writing!\n";
					} else {
						print PLUGIN_PROPERTIES_FILE $plugin_properties;
						close PLUGIN_PROPERTIES_FILE;
					}
				}
			}
			
		}
	}
	closedir DIR;
}

1;
