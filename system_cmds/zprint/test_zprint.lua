#!/usr/local/bin/recon

local darwin = require 'darwin'
local proc = require 'proc'
local zprint = require 'zprint'

if darwin.geteuid() ~= 0 then
  io.stderr:write(arg[0], ': must be run as root (under sudo)\n')
  os.exit(1)
end

local function test_output(zpout)
  -- These should be present in the output of zprint.
  local expectations = {
    {
      region = 'zones',
      name = 'vm.pages',
    }, {
      region = 'tags',
      name = 'VM_KERN_MEMORY_DIAG',
    }, {
      region = 'maps',
      name = 'VM_KERN_COUNT_WIRED_STATIC_KERNELCACHE',
    }, {
      region = 'zone_views',
      -- This ties the kernel's hands when it comes to naming.
      name = 'data.kalloc.16[raw]',
    }
  }

  local found_all = true
  for i = 1, #expectations do
    local region = expectations[i].region
    local name = expectations[i].name
    local iter = zprint[region]
    if not iter then
      io.stderr:write('zprint library has no iterator for ', region, '\n')
      os.exit(4)
    end

    local found = false
    for elt in zprint[region](zpout) do
      if elt.name == name then
        found = true
        break
      end
    end
    if found then
      io.stdout:write('PASS: found ', name, ' in ', region, '\n')
    else
      io.stdout:write('FAIL: could not find ', name, ' in ', region, '\n')
      found_all = false
    end
  end
  return found_all
end

local function run_zprint(args)
  if not args then
    args = {}
  end

  table.insert(args, 1, 'zprint')
  local zpout, err, status, code = proc.run(args)
  if not zpout then
    io.stderr:write(arg[0], ': failed to run zprint: ', err, '\n')
    os.exit(2)
  end

  if code ~= 0 then
    io.stderr:write(arg[0], ': zprint ', status, 'ed with code ', tostring(code),
        ', stderr = ', err, '\n')
    os.exit(3)
  end
  return zpout
end

local function run_and_test(...)
  local zpout = run_zprint(table.pack(...))
  local passed = test_output(zpout)
  if not passed then
    os.exit(5)
  end
end

print("TEST: zprint output")
run_and_test()
print("\nTEST: zprint -t output")
run_and_test("-t")
