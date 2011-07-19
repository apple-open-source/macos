#!/usr/bin/perl

@ran_commands_list = {};
@all_tools_output = {};

run_harness();

$last_leak_count_configd = 0;
$last_leak_count_powerd = 0;

sub print_leaks
{
    $leaks_count_configd = `leaks configd | grep -c "Leak:"`;
    if ($leaks_count_configd > $last_leak_count_configd) {
    	push(@all_tools_output, "[WARN] configd leak count increased");
    }
    push(@all_tools_output, "configd leaks = ".$leaks_count_configd."\n");
    $last_leak_count_configd = $leaks_count_configd;

    
    $leaks_count_powerd = `leaks powerd | grep -c "Leak:"`;
    if ($leaks_count_powerd > $last_leak_count_powerd) {
    	push(@all_tools_output, "[WARN] powerd leak count increased");
    }
    push(@all_tools_output, "powerd leaks = ".$leaks_powerd_output);
    $last_leak_count_powerd = $leaks_count_powerd;
}

sub run_tool
{
    my($tool_path) = @_;
    print "Running tool \"".$tool_path."\" now\n";
    
    push(@ran_commands_list, $tool_path);
    
    $tool_output = `$tool_path`;
    push(@all_tools_output, $tool_output);

    print_leaks();
}

sub run_harness
{
    print "Running tests!\n";
    
    push(@all_tools_output, localtime);
    push(@all_tools_output, "Starting tests"); 
    
    print_leaks();
    
    opendir(DIR, "Sources");

    @files = readdir(DIR);

    foreach $file(@files) {
    	$fullpath = "Sources/".$file;
        if (!(-d $fullpath) && -x $fullpath) {
            run_tool($fullpath);
        }
    }
    print "All done.\n";
            
    @now = localtime();
    $timeStamp = sprintf("%02d_%02d_%02d-%02d_%02d_%02d", 
                        $now[4]+1, $now[3], $now[5]%100, 
                        $now[2],      $now[1],   $now[0]);

    $TestResultsFileName = "PMTestResults-$timeStamp.log";
    print $TestResultsFileName;
    open(FILE, ">", $TestResultsFileName) or die $!;
    print FILE @all_tools_output;
    close FILE;
    
    
    print "\n Showing Test Results\n";
    print `"./Tools/pm_bsdtestsummarize" < $TestResultsFileName`;

}