require 'osx/cocoa'
include OSX

class BulletsController < NSObject
	ib_outlets :menu

	ib_action :showAbout do |sender|
	end

	ib_action :showhelp do |sender|
	end

	ib_action :quit do |sender|
	end

	def awakeFromNib
	end

	def start_service
	end
end


