print "Help Desk -- What Editor do you use? ";
chomp($editor = <STDIN>);
if ($editor =~ /emacs/i) {
  print "Why aren't you using vi?\n";
} elsif ($editor =~ /vi/i) {
  print "Why aren't you using emacs?\n";
} else {
  print "I think that's the problem\n";
}

