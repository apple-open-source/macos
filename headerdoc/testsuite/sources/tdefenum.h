/*!
  @typedef IOHIDOptionsType
  @abstract Options for opening a device via IOHIDLib.
  @constant kIOHIDOptionsTypeNone Default option.
  @constant kIOHIDOptionsTypeSeizeDevice Used to open exclusive
    communication with the device.  This will prevent the system
    and other clients from receiving events from the device.
*/
enum IOHIDOptionsTypeEnum
{
    kIOHIDOptionsTypeNone	 = 0x00,
    kIOHIDOptionsTypeSeizeDevice = 0x01
};
typedef enum IOHIDOptionsTypeEnum IOHIDOptionsType;

