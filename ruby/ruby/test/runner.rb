require 'test/unit'

rcsid = %w$Id: runner.rb,v 1.11 2003/12/02 12:31:44 nobu Exp $
Version = rcsid[2].scan(/\d+/).collect!(&method(:Integer)).freeze
Release = rcsid[3].freeze

exit Test::Unit::AutoRunner.run(false, File.dirname($0))
