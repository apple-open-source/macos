
/*!
        @header         AEventObject.h
        @discussion     Header for the AEventObject class
*/

#pragma once

#include "ACarbonEvent.h"

#include FW(Carbon,CarbonEvents.h)

#include <vector>

/*!
        @typedef        ApplicationRef
        @abstract       A sort of a fake reference type to refer to the application,
                                to match other types like WindowRef and ControlRef.
*/
typedef struct OpaqueApplicationRef *ApplicationRef;    // Just to have something

// ---------------------------------------------------------------------------

/*!
        @class          AEventObject
        @abstract       A Carbon event handler, wrapping EventHandlerRef. Override
                                HandleEvent to handle specific events.
*/
class AEventObject
{
public:
        /*!
                @function               AEventObject
                @abstract               Empty constructor.
        */
                AEventObject()
                : mEventHandlerRef(NULL) {}
        /*!
                @function               AEventObject
                @abstract               Constructor template.
                @param  inObjectRef             The object on which the handler will be installed.
                @templatefield  T               One of ApplicationRef, WindowRef, ControlRef,
                                                                MenuRef, or EventTargetRef.
        */
        template <class T>
                AEventObject(
                                T inObjectRef)
                : mEventHandlerRef(NULL)
                { InstallHandler(inObjectRef); }
        virtual
                ~AEventObject();
        
        /*!
                @function               EventHandlerRef
                @abstract               Return the event handler reference.
                @result                 The event handler reference.
        */
        ::EventHandlerRef
                EventHandlerRef() const
                { return mEventHandlerRef; }
        
        /*!
                @function               AddTypes
                @abstract               Add event types to be handled. Wraps AddEventTypesToHandler.
                @param  inTypes         An array of event types.
                @param  inTypeCount     The number of types to add.
                @throws OSStatus
        */
        void
                AddTypes(
                                const EventTypeSpec *inTypes,
                                UInt32 inTypeCount);
        /*!
                @function               AddType
                @abstract               Add a single event type to be handled.
                @param  inType          The event type to be added.
        */
        void
                AddType(
                                const EventTypeSpec &inType)
                { return AddTypes(&inType,1); }
        /*!
                @function               AddType
                @abstract               Add a single event type by class and kind.
                @param  inClass         The event type.
                @param  inKind          The even kind.
        */
        void
                AddType(
                                UInt32 inClass,
                                UInt32 inKind)
                {
                        EventTypeSpec eventType = { inClass,inKind };
                        AddTypes(&eventType,1);
                }
        /*!
                @function               RemoveTypes
                @abstract               Remove a list of types from those that are handled.
                @param  inTypes         The list of types to be removed.
                @param  inTypeCount     The size of the list.
                @throws OSStatus
        */
        void
                RemoveTypes(
                                const EventTypeSpec *inTypes,
                                UInt32 inTypeCount);
        /*!
                @function               RemoveType
                @abstract               Remove a single event type from those that are handled.
                @param  inType          The event type.
        */
        void
                RemoveType(
                                const EventTypeSpec &inType)
                { return RemoveTypes(&inType,1); }
        
        /*!
                @function               GetCurrentEvent
                @abstract               Return a reference to the event currently being handled.
                @result                 A reference to the event currently being handled.
        */
        static EventRef
                GetCurrentEvent()
                { return sCurrentEvent; }
        /*!
                @function               GetEventHandlerUPP
                @abstract               Get the event handler UPP.
                @result                 The event handled UPP.
        */
        static EventHandlerUPP
                GetEventHandlerUPP()
                { return sEventHandlerUPP; }
        
protected:
        /*!     @var    mEventHandlerRef
                                The event handler reference. */
        ::EventHandlerRef mEventHandlerRef;
        
        /*!     @var    sEventHandledTime
                                The time of the event currently being handled. */
        static EventTime sHandledEventTime;
        /*! @var        sCurrentEvent
                                The EventRef for the event currently being handled. */
        static EventRef sCurrentEvent;
        /*!     @var    sCurrentCallRef
                                The EventHandlerCallRef for the current event. */
        static EventHandlerCallRef sCurrentCallRef;
        /*!     @var    sEventHandlerUPP
                                The function pointer used to install event handlers. */
        static EventHandlerUPP sEventHandlerUPP;
        
        /*!
                @function               GetHandledEventTime
                @abstract               Get the time of the currently handled event.
                @result                 The time of the currently handled event.
        */
        static EventTime
                GetHandledEventTime();
        
        /*!
                @function               InstallHandler
                @abstract               Install the handler on the application.
                @param  inApp           The application reference. The value of this
                                                        parameter has no meaning; it merely serves
                                                        to distinguish this version of InstallHanlder
                                                        from the others.
        */
        void
                InstallHandler(
                                ApplicationRef);
        /*!
                @function               InstallHandler
                @abstract               Install the handler on a window.
                @param  inTarget        The window.
        */
        void
                InstallHandler(
                                WindowRef inTarget);
        /*!
                @function               InstallHandler
                @abstract               Install the handler on a control.
                @param  inTarget        The control.
        */
        void
                InstallHandler(
                                ControlRef inTarget);
        /*!
                @function               InstallHandler
                @abstract               Install the handler on an HIObject.
                @param  inTarget        The object.
        */
#if UNIVERSAL_INTERFACES_VERSION >= 0x0400
        void
                InstallHandler(
                                HIObjectRef inTarget);
#endif
        /*!
                @function               InstallHandler
                @abstract               Install the handler on a menu.
                @param  inTarget        The menu.
        */
        void
                InstallHandler(
                                MenuRef inTarget);
        /*!
                @function               InstallHandler
                @abstract               Install the handler on an event target.
                @param  inTarget        The event target.
        */
        void
                InstallHandler(
                                EventTargetRef inTargetRef);
        
        /*!
                @function               HandleEvent
                @abstract               Handle a Carbon event.
                @result                 An OSStatus code.
                @param  inEvent                 The event.
                @param  outEventHandled On return, whether the event was handled.
                                                                If not, the event will be passed on to the
                                                                next handler on this target.
        */
        virtual OSStatus
                HandleEvent(
                                ACarbonEvent &inEvent,
                                bool &outEventHandled);
        
        /*!
                @function               CallNextHandler
                @abstract               Calls the next event handler.
                @discussion             This method is normally only called from
                                                within EventHalder, but in in certain
                                                special situations (such as HIObject
                                                initialization) it needs to be called at other
                                                times.
                @result                 An OSStatus code.
        */
        static OSStatus
                CallNextHandler();
        
        /*!
                @function               EventHandler
                @abstract               The event handler callback.
                @result                 An OSStatus code.
                @param  inHandlerCallRef        The value to be used in CallNextEventHandler.
                @param  inEvent                         The event to be handled.
                @param  inUserData                      A pointer to the AEventObject instance.
        */
        static pascal OSStatus
                EventHandler(
                                EventHandlerCallRef inHandlerCallRef,
                                EventRef inEvent,
                                void *inUserData);
};

const ApplicationRef kApplication = NULL;

// ---------------------------------------------------------------------------

/*!
        @class          StHandleEventTypes
        @abstract       A stack-based class for adding and removing event types.
*/
class StHandleEventTypes {
public:
        /*!
                @function               StHandleEventTypes
                @abstract               Constructor
                @param  inObject        The AEventObject to be mananged.
        */
                StHandleEventTypes(
                        AEventObject &inObject);
                ~StHandleEventTypes();
        
        /*!
                @function               AddTypes
                @abstract               Add event types to be handled. They will be removed
                                                when this object is destroyed.
                @param  inTypes         An array of event types.
                @param  inTypeCount     The size of the array.
        */
        void
                AddTypes(
                                const EventTypeSpec *inTypes,
                                UInt32 inTypeCount);
        /*!
                @function               AddType
                @abstract               Add a single event type.
                @param  inType          The event type to add.
        */
        void
                AddType(
                                const EventTypeSpec &inType)
                { AddTypes(&inType,1); }
        /*!
                @function               AddType
                @abstract               Add a single event type by class and kind.
                @param  inClass         The event class.
                @param  inKind          The event kind.
        */
        void
                AddType(
                                UInt32 inClass,
                                UInt32 inKind)
                { EventTypeSpec eventType = { inClass,inKind };
                  AddTypes(&eventType,1); }
        
protected:
        /*!
                @typedef                EventTypeArray
                @abstract               An STL vector of EventTypeSpec
        */
        typedef std::vector<EventTypeSpec> EventTypeArray;
        
        /*! @var        mObject
                                The AEventObject whose event types are being managed. */
        AEventObject &mObject;
        /*! @var        mTypes
                                The event types that have been added, and which will
                                be removed when this object is destroyed. */
        EventTypeArray mTypes;
};

// ---------------------------------------------------------------------------


