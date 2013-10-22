#line 1
package Module::Install::GithubMeta;

use strict;
use warnings;
use Cwd;
use base qw(Module::Install::Base);
use vars qw($VERSION);

$VERSION = '0.12';

sub githubmeta {
  my $self = shift;
  return unless $Module::Install::AUTHOR;
  return unless _under_git();
  return unless $self->can_run('git');
  return unless my ($git_url) = `git remote show -n origin` =~ /URL: (.*)$/m;
  return unless $git_url =~ /github\.com/; # Not a Github repository
  my $http_url = $git_url;
  $git_url =~ s![\w\-]+\@([^:]+):!git://$1/!;
  $http_url =~ s![\w\-]+\@([^:]+):!http://$1/!;
  $http_url =~ s!\.git$!/tree!;
  $self->repository(
      {
          type => 'git',
          url  => $git_url,
          web  => $http_url,
      },
  );
  $self->homepage( $http_url ) unless $self->homepage();
  return 1;
}

sub _under_git {
  return 1 if -e '.git';
  my $cwd = getcwd;
  my $last = $cwd;
  my $found = 0;
  while (1) {
    chdir '..' or last;
    my $current = getcwd;
    last if $last eq $current;
    $last = $current;
    if ( -e '.git' ) {
       $found = 1;
       last;
    }
  }
  chdir $cwd;
  return $found;
}

'Github';
__END__

#line 114
