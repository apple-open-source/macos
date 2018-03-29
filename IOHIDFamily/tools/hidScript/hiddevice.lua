
--
-- Dump table
--
function Dump(o)
   if type(o) == 'table' then
      local s = '{ '
      for k,v in pairs(o) do
         if type(k) ~= 'number' then k = '"'..k..'"' end
         s = s .. '['..k..'] = ' .. Dump(v) .. ','
      end
      return s .. '} '
   else
      return tostring(o)
   end
end

--
-- Callback handler for set report. For example hidreport --vid 555 --pid 555 set 0 1 1 1 1
--
function SetReportHandler (reportType, reportID, reportData)
  io.write(string.format("SetReportHandler reportType:%d reportID:%d repor:%s\n", reportType, reportID, Dump(reportData)))
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


function CreateTestDevice ()
  io.write(string.format("+vendor\n"))

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
      <key>RequestTimeout</key>
      <integer>5000000</integer>
    </dict>
    </plist>
  ]]


  local descriptor = {
    0x06, 0x00, 0xFF,            --/* (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined  */\
    0x09, 0x80,                  --/* (LOCAL)  USAGE              0xFF000080   */\
    0xA1, 0x01,                  --/* (MAIN)   COLLECTION         0x01 Application (Usage=0xFF000080: Page=Vendor-defined, Usage=, Type=) */\
    0x09, 0x81,                  --/*   (LOCAL)  USAGE              0xFF000081     */\
    0x06, 0x00, 0xFF,            --/*   (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined <-- Redundant: USAGE_PAGE is already 0xFF00   */\
    0x95, 0x01,                  --/*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
    0x75, 0x20,                  --/*   (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field     */\
    0x81, 0x02,                  --/*   (MAIN)   INPUT              0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
    0x09, 0x82,                  --/*   (LOCAL)  USAGE              0xFF000082     */\
    0x06, 0x00, 0xFF,            --/*   (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined <-- Redundant: USAGE_PAGE is already 0xFF00   */\
    0x95, 0x01,                  --/*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
    0x75, 0x20,                  --/*   (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field <-- Redundant: REPORT_SIZE is already 32    */\
    0x91, 0x02,                  --/*   (MAIN)   OUTPUT             0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
    0x09, 0x83,                  --/*   (LOCAL)  USAGE              0xFF000083     */\
    0x06, 0x00, 0xFF,            --/*   (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined <-- Redundant: USAGE_PAGE is already 0xFF00   */\
    0x95, 0x01,                  --/*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
    0x75, 0x20,                  --/*   (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field <-- Redundant: REPORT_SIZE is already 32    */\
    0xB1, 0x02,                  --/*   (MAIN)   FEATURE            0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
    0xC0,                        --/* (MAIN)   END_COLLECTION     Application */\
    }


    io.write(string.format("+start\n"))

    local device = HIDUserDevice(properties,  descriptor);

    device:SetSetReportCallback(SetReportHandler);
    device:SetGetReportCallback(GetReportHandler);

    util:StartRunLoop();
    io.write(string.format("-start\n"))
end

function main ()
  CreateTestDevice ()
  collectgarbage()
end


main();
