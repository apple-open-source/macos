# PopDel.py  Copyright Richard Harris 2002
#
# author: Richard Harris (rover@emptydog.com)
# The author releases this file under the GPL
# license. See COPYING for specifics.
#
# See PopDel.manual for the use of this Python class.
# (this isn't currently available)
#
# created: 01 May 02
#
# change log:
# Joshua Crawford, November 2004:
# 	Out of range error fixed
#	Allow for all caps SUBJECT:
#	Display email address
#	Don't prompt for save if no changes
#	Don't clear the screen until we're displaying a menu
#	Check for invalid choice
#	Check all arguments exist
#	Check for errors in POP
#	Return 1 on errors, 0 otherwise
# Hacked to support message ranges by ESR, January 2003.
#
import os, poplib, string, sys

class PopDel:
	HDR = "\nPopDel - Delete messages from popmail - Ver. 0.1"
	BYE = "\n  PopDel Ver.0.1 by Richard Harris\n" +\
		  "     site - http://emptydog.com\n" +\
		  "     email - rover@emptydog.com\n"
	PROMPT1 = "Choose message number to delete or 'q' to quit: "
	PROMPT2 = "Quit or abort: "
	CHOICES = ["Save changes and quit.",
			   "Abort and make no deletions."]
	
	def __init__(self):
		self.done = 0
		self.dirty = 0
		return

	# get user to choose an element from thing
	def query(self, thing, prompt):
		length = len(thing)
		choice = [length+1]
		for i in range(0, length):
			print '(' + `i +  1` + ') ' + thing[i]
		while filter(lambda x: x > length, choice):
			choice = raw_input(prompt)
			if (choice == 'q'):
				self.done = 1
				choice = [-1]
			else:
				try:
					choice = map(int, string.split(choice, "-"))
				except:
					choice = [length + 1]
				if len(choice) > 1:
					choice = range(choice[0], choice[1]+1)
		return choice

	def run(self):
		#log in
		print self.HDR

		subjects = []

		if (len(sys.argv) < 4):
			print 'Usage: ' + sys.argv[0] + ' pop3.host.name username password'
			return 1

		try:
			M = poplib.POP3(sys.argv[1])
		except:
			print 'Could not reach ' + sys.argv[1]
			return 1
		try:
			M.user(sys.argv[2])
		except:
			print 'Bad username ' + sys.argv[2] + '@' + sys.argv[1]
			M.quit()
			return 1
		try:
			M.pass_(sys.argv[3])
		except:
			print 'Bad password for ' + sys.argv[2] + '@' + sys.argv[1]
			M.quit()
			return 1
#		M.set_debuglevel(1)
		try:
			messages = M.list()
		except:
			print 'Error reading listing for ' + sys.argv[2] + '@' + sys.argv[1]
			M.quit()
			return 1

		list = messages[1]
		if (len(list) == 0):
			M.quit()
			print '\nNo messages for ' + sys.argv[2] + '@' + sys.argv[1]
		else:
			for entry in list:
				tokens = string.split(entry)
				try:
					head = M.top(int(tokens[0]), 32)
				except:
					print 'Error issuing TOP command for ' + sys.argv[2] + '@' + sys.argv[1]
					if self.dirty:
						M.rset()
					M.quit()
					return 1
				for line in head[1]:
					if (string.find(string.upper(line), 'SUBJECT:') == 0):
						subject = string.replace(line, 'Subject:','')
						subject = string.replace(subject, 'SUBJECT:','')
						subject = subject + ' - ' + tokens[1] + ' octets'
						subjects.append(subject)
						break

			while not self.done:
				os.system('clear')
				print self.HDR
				print '\nMessages for ' + sys.argv[2] + '@' + sys.argv[1] + ':'
				msglist = self.query(subjects, self.PROMPT1)
				print "Choice:", msglist
				for msg in msglist:
					if (msg > 0):
						try:
							M.dele(msg)
						except:
							print 'Error deleting message #' + `msg`
							if self.dirty:
								M.rset()
							M.quit()
							return 1
						self.dirty = 1
						subjects[msg-1] = subjects[msg-1] + ' -X-'

			if not self.dirty:
				M.quit()
			else:
				print '\nExit Options:'
				choice = self.query(self.CHOICES, self.PROMPT2)
				print "Choice:", choice
				if (choice == [1]):		# commit changes and quit
					M.quit()
				else:				# reset and quit
					M.rset()
					M.quit()


		print self.BYE
		return


#-----------------main
obj = PopDel()
sys.exit(obj.run())
