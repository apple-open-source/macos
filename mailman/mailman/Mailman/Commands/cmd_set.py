# Copyright (C) 2002-2003 by the Free Software Foundation, Inc.
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

from email.Utils import parseaddr, formatdate

from Mailman import mm_cfg
from Mailman import Errors
from Mailman import MemberAdaptor
from Mailman import i18n

def _(s): return s

OVERVIEW = _("""
    set ...
        Set or view your membership options.

        Use `set help' (without the quotes) to get a more detailed list of the
        options you can change.

        Use `set show' (without the quotes) to view your current option
        settings.
""")

DETAILS = _("""
    set help
        Show this detailed help.

    set show [address=<address>]
        View your current option settings.  If you're posting from an address
        other than your membership address, specify your membership address
        with `address=<address>' (no brackets around the email address, and no
        quotes!).

    set authenticate <password> [address=<address>]
        To set any of your options, you must include this command first, along
        with your membership password.  If you're posting from an address
        other than your membership address, specify your membership address
        with `address=<address>' (no brackets around the email address, and no
        quotes!).

    set ack on
    set ack off
        When the `ack' option is turned on, you will receive an
        acknowledgement message whenever you post a message to the list.

    set digest plain
    set digest mime
    set digest off
        When the `digest' option is turned off, you will receive postings
        immediately when they are posted.  Use `set digest plain' if instead
        you want to receive postings bundled into a plain text digest
        (i.e. RFC 1153 digest).  Use `set digest mime' if instead you want to
        receive postings bundled together into a MIME digest.

    set delivery on
    set delivery off
        Turn delivery on or off.  This does not unsubscribe you, but instead
        tells Mailman not to deliver messages to you for now.  This is useful
        if you're going on vacation.  Be sure to use `set delivery on' when
        you return from vacation!

    set myposts on
    set myposts off
        Use `set myposts off' to not receive copies of messages you post to
        the list.  This has no effect if you're receiving digests.

    set hide on
    set hide off
        Use `set hide on' to conceal your email address when people request
        the membership list.

    set duplicates on
    set duplicates off
        Use `set duplicates off' if you want Mailman to not send you messages
        if your address is explicitly mentioned in the To: or Cc: fields of
        the message.  This can reduce the number of duplicate postings you
        will receive.

    set reminders on
    set reminders off
        Use `set reminders off' if you want to disable the monthly password
        reminder for this mailing list.
""")

_ = i18n._

STOP = 1



def gethelp(mlist):
    return _(OVERVIEW)



class SetCommands:
    def __init__(self):
        self.__address = None
        self.__authok = 0

    def process(self, res, args):
        if not args:
            res.results.append(_(DETAILS))
            return STOP
        subcmd = args.pop(0)
        methname = 'set_' + subcmd
        method = getattr(self, methname, None)
        if method is None:
            res.results.append(_('Bad set command: %(subcmd)s'))
            res.results.append(_(DETAILS))
            return STOP
        return method(res, args)

    def set_help(self, res, args=1):
        res.results.append(_(DETAILS))
        if args:
            return STOP

    def _usage(self, res):
        res.results.append(_('Usage:'))
        return self.set_help(res)

    def set_show(self, res, args):
        mlist = res.mlist
        if not args:
            realname, address = parseaddr(res.msg['from'])
        elif len(args) == 1 and args[0].startswith('address='):
            # Send the results to the address, not the From: dude
            address = args[0][8:]
            res.returnaddr = address
        else:
            return self._usage(res)
        if not mlist.isMember(address):
            listname = mlist.real_name
            res.results.append(
                _('You are not a member of the %(listname)s mailing list'))
            return STOP
        res.results.append(_('Your current option settings:'))
        opt = mlist.getMemberOption(address, mm_cfg.AcknowledgePosts)
        onoff = opt and _('on') or _('off')
        res.results.append(_('    ack %(onoff)s'))
        # Digests are a special ternary value
        digestsp = mlist.getMemberOption(address, mm_cfg.Digests)
        if digestsp:
            plainp = mlist.getMemberOption(address, mm_cfg.DisableMime)
            if plainp:
                res.results.append(_('    digest plain'))
            else:
                res.results.append(_('    digest mime'))
        else:
            res.results.append(_('    digest off'))
        # If their membership is disabled, let them know why
        status = mlist.getDeliveryStatus(address)
        how = None
        if status == MemberAdaptor.ENABLED:
            status = _('delivery on')
        elif status == MemberAdaptor.BYUSER:
            status = _('delivery off')
            how = _('by you')
        elif status == MemberAdaptor.BYADMIN:
            status = _('delivery off')
            how = _('by the admin')
        elif status == MemberAdaptor.BYBOUNCE:
            status = _('delivery off')
            how = _('due to bounces')
        else:
            assert status == MemberAdaptor.UNKNOWN
            status = _('delivery off')
            how = _('for unknown reasons')
        changetime = mlist.getDeliveryStatusChangeTime(address)
        if how and changetime > 0:
            date = formatdate(changetime)
            res.results.append(_('    %(status)s (%(how)s on %(date)s)'))
        else:
            res.results.append('    ' + status)
        opt = mlist.getMemberOption(address, mm_cfg.DontReceiveOwnPosts)
        # sense is reversed
        onoff = (not opt) and _('on') or _('off')
        res.results.append(_('    myposts %(onoff)s'))
        opt = mlist.getMemberOption(address, mm_cfg.ConcealSubscription)
        onoff = opt and _('on') or _('off')
        res.results.append(_('    hide %(onoff)s'))
        opt = mlist.getMemberOption(address, mm_cfg.DontReceiveDuplicates)
        # sense is reversed
        onoff = (not opt) and _('on') or _('off')
        res.results.append(_('    duplicates %(onoff)s'))
        opt = mlist.getMemberOption(address, mm_cfg.SuppressPasswordReminder)
        # sense is reversed
        onoff = (not opt) and _('on') or _('off')
        res.results.append(_('    reminders %(onoff)s'))

    def set_authenticate(self, res, args):
        mlist = res.mlist
        if len(args) == 1:
            realname, address = parseaddr(res.msg['from'])
            password = args[0]
        elif len(args) == 2 and args[1].startswith('address='):
            password = args[0]
            address = args[1][8:]
        else:
            return self._usage(res)
        # See if the password matches
        if not mlist.isMember(address):
            listname = mlist.real_name
            res.results.append(
                _('You are not a member of the %(listname)s mailing list'))
            return STOP
        if not mlist.Authenticate((mm_cfg.AuthUser,
                                   mm_cfg.AuthListAdmin),
                                  password, address):
            res.results.append(_('You did not give the correct password'))
            return STOP
        self.__authok = 1
        self.__address = address

    def _status(self, res, arg):
        status = arg.lower()
        if status == 'on':
            flag = 1
        elif status == 'off':
            flag = 0
        else:
            res.results.append(_('Bad argument: %(arg)s'))
            self._usage(res)
            return -1
        # See if we're authenticated
        if not self.__authok:
            res.results.append(_('Not authenticated'))
            self._usage(res)
            return -1
        return flag

    def set_ack(self, res, args):
        mlist = res.mlist
        if len(args) <> 1:
            return self._usage(res)
        status = self._status(res, args[0])
        if status < 0:
            return STOP
        mlist.setMemberOption(self.__address, mm_cfg.AcknowledgePosts, status)
        res.results.append(_('ack option set'))

    def set_digest(self, res, args):
        mlist = res.mlist
        if len(args) <> 1:
            return self._usage(res)
        if not self.__authok:
            res.results.append(_('Not authenticated'))
            self._usage(res)
            return STOP
        arg = args[0].lower()
        if arg == 'off':
            try:
                mlist.setMemberOption(self.__address, mm_cfg.Digests, 0)
            except Errors.AlreadyReceivingRegularDeliveries:
                pass
        elif arg == 'plain':
            try:
                mlist.setMemberOption(self.__address, mm_cfg.Digests, 1)
            except Errors.AlreadyReceivingDigests:
                pass
            mlist.setMemberOption(self.__address, mm_cfg.DisableMime, 1)
        elif arg == 'mime':
            try:
                mlist.setMemberOption(self.__address, mm_cfg.Digests, 1)
            except Errors.AlreadyReceivingDigests:
                pass
            mlist.setMemberOption(self.__address, mm_cfg.DisableMime, 0)
        else:
            res.results.append(_('Bad argument: %(arg)s'))
            self._usage(res)
            return STOP
        res.results.append(_('digest option set'))

    def set_delivery(self, res, args):
        mlist = res.mlist
        if len(args) <> 1:
            return self._usage(res)
        status = self._status(res, args[0])
        if status < 0:
            return STOP
        # Delivery status is handled differently than other options.  If
        # status is true (set delivery on), then we enable delivery.
        # Otherwise, we have to use the setDeliveryStatus() interface to
        # specify that delivery was disabled by the user.
        if status:
            mlist.setDeliveryStatus(self.__address, MemberAdaptor.ENABLED)
            res.results.append(_('delivery enabled'))
        else:
            mlist.setDeliveryStatus(self.__address, MemberAdaptor.BYUSER)
            res.results.append(_('delivery disabled by user'))

    def set_myposts(self, res, args):
        mlist = res.mlist
        if len(args) <> 1:
            return self._usage(res)
        status = self._status(res, args[0])
        if status < 0:
            return STOP
        # sense is reversed
        mlist.setMemberOption(self.__address, mm_cfg.DontReceiveOwnPosts,
                              not status)
        res.results.append(_('myposts option set'))

    def set_hide(self, res, args):
        mlist = res.mlist
        if len(args) <> 1:
            return self._usage(res)
        status = self._status(res, args[0])
        if status < 0:
            return STOP
        mlist.setMemberOption(self.__address, mm_cfg.ConcealSubscription,
                              status)
        res.results.append(_('hide option set'))

    def set_duplicates(self, res, args):
        mlist = res.mlist
        if len(args) <> 1:
            return self._usage(res)
        status = self._status(res, args[0])
        if status < 0:
            return STOP
        # sense is reversed
        mlist.setMemberOption(self.__address, mm_cfg.DontReceiveDuplicates,
                              not status)
        res.results.append(_('duplicates option set'))

    def set_reminders(self, res, args):
        mlist = res.mlist
        if len(args) <> 1:
            return self._usage(res)
        status = self._status(res, args[0])
        if status < 0:
            return STOP
        # sense is reversed
        mlist.setMemberOption(self.__address, mm_cfg.SuppressPasswordReminder,
                              not status)
        res.results.append(_('reminder option set'))



def process(res, args):
    # We need to keep some state between set commands
    if not getattr(res, 'setstate', None):
        res.setstate = SetCommands()
    res.setstate.process(res, args)
