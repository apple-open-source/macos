function dump(o)
   if type(o) == 'table' then
      local s = '{ '
      for k,v in pairs(o) do
         if type(k) ~= 'number' then k = '"'..k..'"' end
         s = s .. dump(v) .. ','
      end
      return s .. '} '
   else
      return string.format("0x%x" , o)
   end
end

function createReport (size)
  report = {n=size}
  for i=1,size,1 do
    table.insert(report,0);
  end
  return report
end

function setTableWithUInt8 (tbl, index, value)
  tbl[index] =  (value) & 0xFF
  return tbl
end

function setTableWithUInt16 (tbl, index, value)
  tbl[index] =  (value) & 0xFF
  tbl[index + 1] =  (value >> 8) & 0xFF
  return tbl
end

function setTableWithUInt32 (tbl, index, value)
  tbl[index] =  (value) & 0xFF
  tbl[index + 1] =  (value >> 8) & 0xFF
  tbl[index + 2] =  (value >>16) & 0xFF
  tbl[index + 3] =  (value >>24) & 0xFF
  return tbl
end

function setTableWithUInt64 (tbl, index, value)
  tbl = setTableWithUInt32(tbl, index, value)
  tbl = setTableWithUInt32(tbl, index + 4, value >> 32)
  return tbl
end


function createSensorReport (rate, confidence, generation, location, timestamp)
  report = createReport (14)
  report = setTableWithUInt8(report, 1, 1)
  report = setTableWithUInt8(report, 2, rate)
  report = setTableWithUInt8(report, 3, confidence)
  report = setTableWithUInt16(report, 4, generation)
  report = setTableWithUInt8(report, 6, location)
  report = setTableWithUInt64(report, 7, timestamp)
  return report
end


--
-- Callback handler for set report. For example hidutil property --match '{"ProductID":555, "VendorID":555}' --set  '{"ReportInterval":5000}'
--
function SetReportHandler (reportType, reportID, reportData)
  io.write(string.format("SetReportHandler reportType:%d reportID:%d repor:%s\n", reportType, reportID, dump(reportData)))
  collectgarbage()
end

--
-- Callback handler for get report. For example hidreport --vid 555 --pid 555 get 0
--
function GetReportHandler (reportType, reportID, reportData)
  io.write(string.format("GetReportHandler reportType:%d reportID:%d\n", reportType, reportID))
  reportData[1] = 1;
  reportData[2] = 2;
  reportData[3] = 3;
  reportData[4] = 4;
  collectgarbage()
end


function test ()
  io.write(string.format("+heartrate\n"))

  local properties = [[
    <?xml version="1.0" encoding="UTF-8"?>
    <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
    <plist version="1.0">
    <dict>
      <key>VendorID</key>
      <integer>555</integer>
      <key>ProductID</key>
      <integer>555</integer>
      <key>ReportInterval</key>
      <integer>10000</integer>
    </dict>
    </plist>
  ]]


  local descriptor = {
   0x05, 0x20,                  -- (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page  */\
  0x09, 0x16,                  -- (LOCAL)  USAGE              0x00200016 Biometric: Heart Rate (CACP=Application or Physical Collection)  */\
  0xA1, 0x01,                  -- (MAIN)   COLLECTION         0x01 Application (Usage=0x00200016: Page=Sensor Device Page, Usage=Biometric: Heart Rate, Type=CACP) */\
  0x85, 0x01,                  --   (GLOBAL) REPORT_ID          0x01 (1)    */\
  0x0A, 0x0E, 0x03,            --   (LOCAL)  USAGE              0x0020030E Property: Report Interval (DV=Dynamic Value)    */\
  0x14,                        --   (GLOBAL) LOGICAL_MINIMUM    (0) <-- Redundant: LOGICAL_MINIMUM is already 0    */\
  0x27, 0xFF, 0xFF, 0xFF, 0x7F, --   (GLOBAL) LOGICAL_MAXIMUM    0x7FFFFFFF (2147483647)     */\
  0x75, 0x20,                  --   (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field     */\
  0x95, 0x01,                  --   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
  0xB1, 0x02,                  --   (MAIN)   FEATURE            0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
  0x0A, 0xB8, 0x04,            --   (LOCAL)  USAGE              0x002004B8 Data Field: Heart Rate (SV=Static Value)    */\
  0x26, 0xFF, 0x00,            --   (GLOBAL) LOGICAL_MAXIMUM    0x00FF (255)     */\
  0x75, 0x08,                  --   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field     */\
  0x95, 0x01,                  --   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
  0x81, 0x02,                  --   (MAIN)   INPUT              0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
  0x06, 0x15, 0xFF,            --   (GLOBAL) USAGE_PAGE         0xFF15 Vendor-defined    */\
  0x0A, 0x20, 0x01,            --   (LOCAL)  USAGE              0xFF150120     */\
  0x95, 0x01,                  --   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
  0x75, 0x08,                  --   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field <-- Redundant: REPORT_SIZE is already 8    */\
  0x81, 0x02,                  --   (MAIN)   INPUT              0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
  0x26, 0xFF, 0x7F,            --   (GLOBAL) LOGICAL_MAXIMUM    0x7FFF (32767)     */\
  0x06, 0x15, 0xFF,            --   (GLOBAL) USAGE_PAGE         0xFF15 Vendor-defined <-- Redundant: USAGE_PAGE is already 0xFF15   */\
  0x0A, 0x21, 0x01,            --   (LOCAL)  USAGE              0xFF150121     */\
  0x95, 0x01,                  --   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
  0x75, 0x10,                  --   (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field     */\
  0x81, 0x02,                  --   (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
  0x75, 0x08,                  --   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field     */\
  0x95, 0x01,                  --   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
  0x15, 0x01,                  --   (GLOBAL) LOGICAL_MINIMUM    0x01 (1)     */\
  0x25, 0x02,                  --   (GLOBAL) LOGICAL_MAXIMUM    0x02 (2)     */\
  0x1A, 0x04, 0x01,            --   (LOCAL)  USAGE_MINIMUM      0xFF150104     */\
  0x2A, 0x05, 0x01,            --   (LOCAL)  USAGE_MAXIMUM      0xFF150105     */\
  0x81, 0x00,                  --   (MAIN)   INPUT              0x00000000 (1 field x 8 bits) 0=Data 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
  0x06, 0x00, 0xFF,            --   (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined    */\
  0x09, 0x23,                  --   (LOCAL)  USAGE              0xFF000023     */\
  0xA1, 0x00,                  --   (MAIN)   COLLECTION         0x00 Physical (Usage=0xFF000023: Page=Vendor-defined, Usage=, Type=)   */\
  0x15, 0x00,                  --     (GLOBAL) LOGICAL_MINIMUM    0x00 (0)  <-- Info: Consider replacing 15 00 with 14     */\
  0x24,                        --     (GLOBAL) LOGICAL_MAXIMUM    (0)       */\
  0x06, 0x15, 0xFF,            --     (GLOBAL) USAGE_PAGE         0xFF15 Vendor-defined      */\
  0x09, 0x02,                  --     (LOCAL)  USAGE              0xFF150002       */\
  0x95, 0x08,                  --     (GLOBAL) REPORT_COUNT       0x08 (8) Number of fields       */\
  0x75, 0x08,                  --     (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field <-- Redundant: REPORT_SIZE is already 8      */\
  0x81, 0x02,                  --     (MAIN)   INPUT              0x00000002 (8 fields x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0xC0,                        --   (MAIN)   END_COLLECTION     Physical   */\
  0xC0,                        -- (MAIN)   END_COLLECTION     Application */\
  }


    local sensor = HIDUserDevice(properties,  descriptor)
    sensor:SetSetReportCallback(SetReportHandler)
    sensor:SetGetReportCallback(GetReportHandler);
    sensor:WaitForEventService(2000000)
    
    util.usleep(10000)
 
    left = 1
    right = 2
    
    heartrate = 55
    confidence = 0
    timestamp = 1000
    generation = 1
    report = createSensorReport (heartrate, confidence, generation, left, timestamp)
    io.write(string.format("dispatch sensor event report: %s \n", dump(report)))
    sensor:SendReport(report)

    heartrate = 60
    confidence = 127
    timestamp = 1100
    generation = 2
    report = createSensorReport (heartrate, confidence, generation, left, timestamp)
    io.write(string.format("dispatch sensor event report: %s \n", dump(report)))
    sensor:SendReport(report)


    heartrate = 55
    confidence = 255
    timestamp = 1200
    generation = 3
    report = createSensorReport (heartrate, confidence, generation, left, timestamp)
    io.write(string.format("dispatch sensor event report: %s \n", dump(report)))
    sensor:SendReport(report)
 
    util.StartRunLoop()

    io.write(string.format("-heartrate\n"))
end

function main ()
  test()
  collectgarbage()
end


main()
