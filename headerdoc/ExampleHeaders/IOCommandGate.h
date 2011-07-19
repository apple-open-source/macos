/* Sample C++ Header -- Used for HeaderDoc testing.*/
/*!  @language embedded-c++ */

#ifndef _IOKIT_IOCOMMANDGATE_H
#define _IOKIT_IOCOMMANDGATE_H

#include <IOKit/IOEventSource.h>

/*!
     @class IOCommandGate
     @abstract Single-threaded work-loop client request mechanism.
     @discussion An IOCommandGate instance is an extremely light way mechanism
that executes an action on the driver's work-loop.  'On the work-loop' is
actually a lie but the work-loop single threaded semantic is 
maintained for this
event source.  Using the work-loop gate rather than execution by the workloop.
The command gate tests for a potential self dead lock by checking if the
runCommand request is made from the work-loop's thread, it doens't check for a
mutual dead lock though where a pair of work loop's dead lock each other.

     The IOCommandGate is a lighter weight version of the IOCommandQueue and
should be used in preference.  Generally use a command queue whenever 
you need a
client to submit a request to a work loop.  A typical command gate action would
check if the hardware is active, if so it will add the request to a pending
queue internal to the device or the device's family.  Otherwise if the hardware
is inactive then this request can be acted upon immediately.

     The run functions can not be called from an interrupt context.

*/
class IOCommandGate : public IOEventSource
{
     OSDeclareDefaultStructors(IOCommandGate)

public:
/*!
     @typedef Action
     @discussion Type and arguments of callout C function that is used when
a runCommand is executed by a client.  Cast to this type when you want a C++
member function to be used.  Note the arg1 - arg3 parameters are straight pass
through from the runCommand to the action callout.
     @param owner
         Target of the function, can be used as a refcon.  The owner is set
during initialisation of the IOCommandGate instance.	 Note if a C++ function
was specified this parameter is implicitly the first paramter in the target
member function's parameter list.
     @param arg0 Argument to action from run operation.
     @param arg1 Argument to action from run operation.
     @param arg2 Argument to action from run operation.
     @param arg3 Argument to action from run operation.
*/
     typedef IOReturn (*Action)(OSObject *owner,
                                void *arg0, void *arg1,
                                void *arg2, void *arg3);

protected:
/*!
     @function checkForWork
     @abstract Not used.
     @discussion This function is a pure virtual function in IOEventSource and
thus MUST be implemented in IOCommandGate, but the command gate short circuits
the traditional IOEventSource/IOWorkLoop relationship.
     @param moreP
	Return boolean used to inform the workloop that this event source has
more work to be processed later.
     @param wakeupTimeP
	Return parameter to indicate a timeout when this IOEventSource must
be checked again, the time is currently absolute.
*/
     virtual void checkForWork(bool *moreP, mach_timespec_t *wakeupTimeP);

/*! @group group_1 */

/*!
     @function inlineFunctionOneLine
     @abstract Test of the handling of inline functions.
     @discussion This function is inlined and defined all on one line.
*/
    void inlineFunctionOneLine()
{ return foo; };

/*! @group group_2 */
/*!
     @function inlineFunctionMultiLine
     @abstract Test of the handling of inline functions.
     @discussion This function is inlined and defined on several lines.
*/
    void inlineFunctionMultiLine() { 
        return foo; 
    };

public:
/*!
     @function commandGate
     @abstract Static function used as a factory to create new 
IOCommandGate.	See init.
     @param owner
	Owner of this instance of the IOCommandGate.  This argument will be
used as the first parameter in the action callout.
     @param action
	Pointer to a C function that is called whenever a client of the
IOCommandGate calls runCommand.	 NB Can be a C++ member function but caller
must cast the member function to IOCommandGateAction and they will get a
compiler warning.  Defaults to zero, see IOEventSource::setAction.
*/
     static IOCommandGate *
         commandGate(OSObject *owner, IOCommandGate::Action action = 0);
/*!
     @function init
     @abstract Class initialiser.
     @discussion Initialiser for IOCommandGate operates only on newly 'newed'
objects.  Shouldn't be used to re-init an existing instance.
     @param owner Owner of this instance of the IOCommandGate.
     @param action
	Pointer to a C function that is called whenever a client of the
IOCommandGate calls runCommand.  Defaults to zero, see 
IOEventSource::setAction.
     @result True if inherited classes initialise successfully.
*/
     virtual bool init(OSObject *owner, IOCommandGate::Action action = 0);

/*!
     @function runCommand
     @abstract Single thread a command with the target work-loop.
     @discussion Client function that causes the current action to be called in
a single threaded manner.  Beware the work-loop's gate is recursive and command
gates can cause direct or indirect re-entrancy.  When the executing on a
client's thread runCommand will sleep until the work-loop's gate opens for
execution of client actions, the action is single threaded against all other
work-loop event sources.
     @param arg0 Parameter for action of command gate, defaults to 0.
     @param arg1 Parameter for action of command gate, defaults to 0.
     @param arg2 Parameter for action of command gate, defaults to 0.
     @param arg3 Parameter for action of command gate, defaults to 0.
     @result kIOReturnSuccess if successful. kIOReturnNotPermitted if this
event source is currently disabled, kIOReturnNoResources if no action 
available.
*/
     virtual IOReturn runCommand(void *arg0 = 0, void *arg1 = 0,
                                 void *arg2 = 0, void *arg3 = 0);

/*! @group group_1 */
/*!
     @function runAction
     @abstract Single thread a call to an action with the target work-loop.
     @discussion Client function that causes the given action to be called in
a single threaded manner.  Beware the work-loop's gate is recursive and command
gates can cause direct or indirect re-entrancy.  When the executing on a
client's thread runCommand will sleep until the work-loop's gate opens for
execution of client actions, the action is single threaded against all other
work-loop event sources.
     @param action Pointer to function to be executed in work-loop context.
     @param arg0 Parameter for action of command gate, defaults to 0.
     @param arg1 Parameter for action of command gate, defaults to 0.
     @param arg2 Parameter for action of command gate, defaults to 0.
     @param arg3 Parameter for action of command gate, defaults to 0.
     @result kIOReturnSuccess if successful. kIOReturnBadArgument if 
action is not defined, kIOReturnNoResources if no action available.
*/
     virtual IOReturn runAction(Action action,
                                void *arg0 = 0, void *arg1 = 0,
                                void *arg2 = 0, void *arg3 = 0);
};

#endif /* !_IOKIT_IOCOMMANDGATE_H */
