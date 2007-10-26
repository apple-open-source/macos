#!/usr/bin/env ruby

# $Id: install-ruby.rb 979 2006-05-29 01:18:25Z hisa $

DSTDIR = '/Library/Ruby'
DOCDIR = DSTDIR
BINDIR = '/usr/local/bin'
MANDIR = '/usr/local/share/man'
LIBDIR = '/usr/local/lib'

def usage
  $stderr.puts "usage: ruby install-ruby.rb {rubysrcdir}"
  exit 1
end

def chdir(dir)
  $stderr.puts "chdir '#{dir}' ..."
  Dir.chdir dir
end

def command(cmd)
  $stderr.puts "execute '#{cmd}' ..."
  raise(RuntimeError, cmd) unless system(cmd)
  $stderr.puts "execute '#{cmd}' done"
end


usage if ARGV.size != 1
usage unless File.directory?(ARGV[0])

SRCDIR = ARGV.shift


CONF_OPTION = "--enable-shared --prefix=#{DSTDIR} --bindir=#{BINDIR} --mandir=#{MANDIR}"

DOCS = [ 'COPYING', 'COPYING.ja', 'ChangeLog', 'GPL', 'LEGAL', 'LGPL',
         'MANIFEST','README','README.EXT','README.EXT.ja','README.ja','ToDo',
         'ReadMe.ascii.html', 'ReadMe.sjis.html' ]

chdir SRCDIR
command "./configure #{CONF_OPTION}"
command "make"
command "make test"
command "sudo make install"

command "sudo mkdir #{DOCDIR}" unless File.directory?(DOCDIR)
command "sudo cp -p #{DOCS.join(' ')} #{DOCDIR}"

chdir "#{DSTDIR}/lib"
command "tar cf - libruby* | ( cd #{LIBDIR} ; sudo tar xf - )"
