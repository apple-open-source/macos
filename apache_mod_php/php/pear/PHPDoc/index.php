<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">

<html>
<head>
	<title>PHPDOC Version 1.0beta</title>
</head>

<body>
	<table width="560">
		<tr>
			<td align="left" valign="top">
				<font face="Arial, Helvetica" size="2">
					<h1>Welcome to PHPDoc!</h1>
				
					PHPDoc currently requires a late PHP4 version (4.0.3dev) to work.
					Some earlier version like to crash with memory trouble. 
					<p>
					PHPDoc was programmed without	using PHP4 features that means 
					it will run with version prior to	4.0.3dev on fine day. 
					But again: my PHP3.0.15 crashed when it started to render the 
					HTML documents :(.
					<p>
					I'll try to provide a solution as soon as possible.
				</font>
			</td>
		<tr>
			<td align="left" valign="top"><hr></td>
		</tr>
		<tr>
			<td align="left" valign="top">
				<pre>
				<?php
				$start = time();
				
				// WARNING: long runtimes! Make modifications 
				// to the php[3].ini if neccessary. A P3-500 
				// needs slightly more than 30 seconds to 
				// document phpdoc itself.
						
				// Directory with include files
				define("PHPDOC_INCLUDE_DIR", "c:/www/apache/doc/");
				// Important: set this to the Linebreak sign of your system!
				define("LINEBREAK", "\r\n");
		
				// main PHPDoc Include File
				include("./prepend.php");		
		
				$doc = new Phpdoc;
				
				// Sets the name of your application.
				// The name of the application gets used in many default templates.
				$doc->setApplication("PEAR Repository");
				
				// directory where your source files reside:
				$doc->setSourceDirectory("c:/www/apache/form/");
				
				// save the generated docs here:
				$doc->setTarget("c:/www/apache/doc/apidoc/");
				
				// use these templates:
				$doc->setTemplateDirectory("c:/www/apache/doc/renderer/html/templates/");
				
				// source files have one of these suffixes:
				$doc->setSourceFileSuffix( array ("php", "inc") );
		
				// parse and generate the xml files
				$doc->parse();
				
				// turn xml in to html using templates
				$doc->render();
				
				printf("%d seconds needed\n\n.", time() - $start);
				?>
				</pre>	
			</td>
		</tr>
		<tr>
			<td align="left" valign="top"><hr></td>
		</tr>
		<tr>
			<td align="left" valign="top">
				<font face="Arial, Helvetica" size="2">
					<h2>Finished!</h2>
					PHPDoc has finished his work. The generated XML and HTML files can be found in
					the directory specified with setTarget() in the above code. This is per default "installationdir/apidoc/".
					Within this directory is another directory named "keep/". It contains a stylesheet file and 
					a frameset you can use to browse the HTML files. 
					<p>
					Don't be disappointed if PHPDoc makes documentation mistakes. Always remember it's currently
					only grabbing not parsing. If PHPDoc will develop as a standard this will change as soon 
					as possible. Please be patient and wait until "... later this year." ;-).
					<p>
					Have fun!
					<p>
					<a href="mailto:ulf.wendel@phpdoc.de">Ulf</a>
					<p>
					... and what about the warning? I use xml_parse_into_struct() as documented, but PHP4 likes to throw a warning, ignore it.
				</font>
			</td>
		</tr>
	</table>
</body>
</html>
