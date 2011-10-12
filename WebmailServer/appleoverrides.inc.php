<?php

$rcmail_config['include_host_config'] = true;
$rcmail_config['default_host'] = 'tls://%n';
$rcmail_config['smtp_server'] = '%h';
$rcmail_config['smtp_user'] = '%u';
$rcmail_config['smtp_pass'] = '%p';
$rcmail_config['des_key'] = '123456789012345678901234';
$rcmail_config['useragent'] = 'Apple Webmail/'.RCMAIL_VERSION;
$rcmail_config['product_name'] = 'Apple Webmail';
$rcmail_config['enable_spellcheck'] = false;
$rcmail_config['mime_param_folding'] = 0;
$rcmail_config['log_driver'] = 'file';
$rcmail_config['log_dir'] = '/var/log/webmail';
$rcmail_config['temp_dir'] = '/var/webmail';
$rcmail_config['preview_pane'] = true;
$rcmail_config['identities_level'] = 2;
$rcmail_config['password_charset'] = 'UTF-8';
$rcmail_config['default_charset'] = 'UTF-8';
$rcmail_config['message_sort_order'] = 'ASC';
$rcmail_config['list_cols'] = array('flag', 'from', 'subject', 'date');
$rcmail_config['htmleditor'] = true;
// To do: some password function
$rcmail_config['db_dsnw'] = 'pgsql://roundcube:roundcubemail@unix(/var/pgsql_socket)roundcubemail';
$rcmail_config['plugins'] = array('markasjunk', 'managesieve', 'disable_advanced_ui');
// special folder names to match those used by Mail.app
$rcmail_config['drafts_mbox'] = 'Drafts';
$rcmail_config['junk_mbox'] = 'Junk';
$rcmail_config['sent_mbox'] = 'Sent Messages';
$rcmail_config['trash_mbox'] = 'Deleted Messages';
$rcmail_config['default_imap_folders'] = array('INBOX', 'Drafts', 'Sent', 'Junk', 'Trash');
