# Copyright (C) 1998,1999,2000,2001,2002 by the Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software 
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

"""A mutiple sink logger.  Any message written goes to all sub-loggers."""

import sys
from Mailman.Logging.Utils import _logexc



class MultiLogger:
    def __init__(self, *args):
        self.__loggers = []
        for logger in args:
            self.__loggers.append(logger)

    def add_logger(self, logger):
        if logger not in self.__loggers:
            self.__loggers.append(logger)

    def del_logger(self, logger):
        if logger in self.__loggers:
            self.__loggers.remove(logger)

    def write(self, msg):
        for logger in self.__loggers:
            # you want to be sure that a bug in one logger doesn't prevent
            # logging to all the other loggers
            try:
                logger.write(msg)
            except:
                _logexc(logger, msg)

    def writelines(self, lines):
        for line in lines:
            self.write(line)

    def flush(self):
        for logger in self.__loggers:
            if hasattr(logger, 'flush'):
                # you want to be sure that a bug in one logger doesn't prevent
                # logging to all the other loggers
                try:
                    logger.flush()
                except:
                    _logexc(logger)

    def close(self):
        for logger in self.__loggers:
            # you want to be sure that a bug in one logger doesn't prevent
            # logging to all the other loggers
            try:
                if logger <> sys.__stderr__ and logger <> sys.__stdout__:
                    logger.close()
            except:
                _logexc(logger)

    def reprime(self):
        for logger in self.__loggers:
            try:
                logger.reprime()
            except AttributeError:
                pass
