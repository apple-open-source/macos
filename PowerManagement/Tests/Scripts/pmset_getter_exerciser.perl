#!/usr/bin/perl

	$pmset_list = `pmset -g getters`;
	@split_list = split(/\W/, $pmset_list);
	
	foreach $arg(@split_list)
	{
		print "Invoking \"pmset -g $arg\"\n";
		print `pmset -g $arg`;
	}
