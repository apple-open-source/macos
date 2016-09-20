
function test ()
  io.write(string.format("+mousestress\n"))

  local properties = [[
    <?xml version="1.0" encoding="UTF-8"?>
    <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
    <plist version="1.0">
    <dict>
      <key>VendorID</key>
      <integer>5</integer>
      <key>ProductID</key>
      <integer>6</integer>
      <key>ReportInterval</key>
      <integer>10000</integer>
      <key>RequestTimeout</key>
      <integer>5000000</integer>
    </dict>
    </plist>
  ]]

  local descriptor = {
                        05,0x01,0x09,0x02,0xa1,0x01,0x05,0x09,0x19,0x01,0x29,
                        0x04,0x15,0x00,0x25,0x01,0x95,0x04,0x75,0x01,0x81,0x02,
                        0x95,0x01,0x75,0x04,0x81,0x01,0x05,0x01,0x09,0x01,0xa1,
                        0x00,0x09,0x30,0x09,0x31,0x09,0x32,0x09,0x38,0x15,0x81,
                        0x25,0x7f,0x75,0x08,0x95,0x04,0x81,0x06,0xc0,0x05,0xff,
                        0x09,0xc0,0x75,0x08,0x95,0x01,0x81,0x02,0xc0
                     }


  local mouse = HIDUserDevice( properties, descriptor)

   for x=0,5,1 do
    for y=0,5,1 do
      mouse:SendReport ({0, x ,y, 0, 0, 0})
      util.usleep(1000)
      io.write(string.format("move x:%d, y:%d\n", x, y))
    end
  end
  for s=0,5,1 do
      mouse:SendReport ({0, 0, 0, 0, s, 0})
      util.usleep(1000)
      io.write(string.format("scroll s:%d\n", s))

  end

  io.write(string.format("-mousestress\n"))
end

function main ()
  test()
  collectgarbage()
end


main();
