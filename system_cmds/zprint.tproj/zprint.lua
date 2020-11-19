require 'strict'

-- # zprint
--
-- Parse the output of zprint into tables.

local zprint = {}

-- Return the lines inside "dashed" lines -- that is, lines that are entirely
-- made up of dashes (-) in the string `str`.  The `skip_dashed` argument
-- controls how many dashed lines to skip before returning the lines between it
-- and the next dashed line.
local function lines_inside_dashes(str, skip_dashed)
  local start_pos = 1
  for _ = 1, skip_dashed do
    _, start_pos = str:find('\n[-]+\n', start_pos)
  end
  assert(start_pos, 'found dashed line in output')
  local end_pos, _ = str:find('\n[-]+\n', start_pos + 1)
  assert(end_pos, 'found ending dashed line in output')

  return str:sub(start_pos + 1, end_pos - 1)
end

-- Iterate through the zones listed in the given zprint(1) output `zpout`.
--
--     for zone in zprint_zones(io.stdin:read('*a')) do
--         print(zone.name, zone.size, zone.used_size)
--     end
function zprint.zones(zpout)
  -- Get to the first section delimited by dashes.  This is where the zones are
  -- recorded.
  local zones = lines_inside_dashes(zpout, 1)

  -- Create an iterator for each line, for use in our own iteration function.
  local lines = zones:gmatch('([^\n]+)')

  return function ()
    -- Grab the next line.
    local line = lines()
    if not line then
      return nil
    end

    -- Match each column from zprint's output.
    local name, eltsz, cursz_kb, maxsz_kb, nelts, nelts_max, nused,
        allocsz_kb = line:match(
        '([%S]+)%s+(%d+)%s+(%d+)K%s+(%d+)K%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)')
    -- Convert numeric fields to numbers, and into bytes if specified in KiB.
    eltsz = tonumber(eltsz)
    local cursz = tonumber(cursz_kb) * 1024
    local maxsz = tonumber(maxsz_kb) * 1024
    local usedsz = tonumber(nused) * eltsz
    local allocsz = tonumber(allocsz_kb) * 1024

    -- Return a table representing the zone.
    return {
      name = name, -- the name of the zone
      size = cursz, -- the size of the zone
      max_size = maxsz, -- the maximum size of the zone
      used_size = usedsz, -- the size of all used elements in the zone
      element_size = eltsz, -- the size of each element in the zone
      allocation_size = allocsz, -- the size of allocations made for the zone
    }
  end
end

-- Match the output of a vm_tag line
-- This line has a variable number of columns.
-- This function returns the name and a table containing each numeric column's
-- value.
local function match_tag(line, ncols)
  -- First try to match names with C++ symbol names.
  -- These can have whitespace in the argument list.
  local name_pattern = '^(%S+%b()%S*)'
  local name = line:match(name_pattern)
  if not name then
    name = line:match('(%S+)')
    if not name then
      return nil
    end
  end
  local after_name = line:sub(#name)
  local t = {}
  for v in line:gmatch('%s+(%d+)K?') do
    table.insert(t, v)
  end   
  return name, t
end

-- Iterate through the tags listed in the given zprint(1) output `zpout`.
function zprint.tags(zpout)
  -- Get to the third zone delimited by dashes, where the tags are recorded.
  local tags = lines_inside_dashes(zpout, 3)

  local lines = tags:gmatch('([^\n]+)')

  return function ()
    local line = lines()
    if not line then
      return nil
    end

    -- Skip any unloaded kmod lines.
    while line:match('(unloaded kmod)') do
      line = lines()
    end
    -- End on the zone tags line, since it's not useful.
    if line:match('zone tags') then
      return nil
    end

    local name, matches = match_tag(line)
    if not name or #matches == 0 then
      return nil
    end

    local cursz_kb = matches[#matches]
    -- If there are fewer than 3 numeric columns, there's no reported peak size
    local maxsz_kb = nil
    if #matches > 3 then
      maxsz_kb = matches[#matches - 1]
    end

    -- Convert numeric fields to numbers and then into bytes.
    local cursz = tonumber(cursz_kb) * 1024
    local maxsz = maxsz_kb and (tonumber(maxsz_kb) * 1024)

    -- Return a table representing the region.
    return {
      name = name,
      size = cursz,
      max_size = maxsz,
    }
  end
end

-- Iterate through the maps listed in the given zprint(1) output `zpout`.
function zprint.maps(zpout)
  local maps = lines_inside_dashes(zpout, 5)
  local lines = maps:gmatch('([^\n]+)')

  return function()
    -- Grab the next line.
    local line = lines()
    if not line then
      return nil
    end

    -- The line can take on 3 different forms. Check for each of them

    -- Check for 3 columns
    local name, free_kb, largest_free_kb, curr_size_kb = line:match(
        '(%S+)%s+(%d+)K%s+(%d+)K%s+(%d+)K')
    local free, largest_free, peak_size_kb, peak_size, size
    if not name then
      -- Check for 2 columns
      name, peak_size_kb, curr_size_kb = line:match('(%S+)%s+(%d+)K%s+(%d+)K')
      if not name then
        -- Check for a single column
        name, curr_size_kb = line:match('(%S+)%s+(%d+)K')
        assert(name)
      else
        peak_size = tonumber(peak_size_kb) * 1024
      end
    else
      free = tonumber(free_kb) * 1024
      largest_free = tonumber(largest_free_kb) * 1024
    end
    size = tonumber(curr_size_kb) * 1024

    return {
      name = name,
      size = size,
      max_size = peak_size,
      free = free,
      largest_free = largest_free
    }
  end
end

-- Iterate through the zone views listed in the given zprint(1) output `zpout`.
function zprint.zone_views(zpout)
  -- Skip to the zone views
  local prev_pos = 1
  -- Look for a line that starts with "zone views" and is followed by a -- line.
  while true do
    local start_pos, end_pos = zpout:find('\n[-]+\n', prev_pos)
    if start_pos == nil then
      return nil
    end
    local before = zpout:sub(prev_pos, start_pos)
    local zone_views_index = zpout:find('\n%s*zone views%s+[^\n]+\n', prev_pos + 1)
    prev_pos = end_pos
    if  zone_views_index and zone_views_index < end_pos then
      break
    end
  end

  local zone_views
  local zone_totals_index = zpout:find("\nZONE TOTALS")
  if zone_totals_index then
    zone_views = zpout:sub(prev_pos + 1, zone_totals_index)
  else
    zone_views = zpout:sub(prev_pos+ 1)
  end

  local lines = zone_views:gmatch('([^\n]+)')

  return function()
    -- Grab the next line.
    local line = lines()
    if not line then
      return nil
    end

    local name, curr_size_kb = line:match('(%S+)%s+(%d+)')
    local size = tonumber(curr_size_kb) * 1024

    return {
      name = name,
      size = size,
    }
  end
end

function zprint.total(zpout)
  local total = zpout:match('total[^%d]+(%d+.%d+)M of')
  local bytes = tonumber(total) * 1024 * 1024
  return bytes
end

-- Return a library object, if called from require or dofile.
local calling_func = debug.getinfo(2).func
if calling_func == require or calling_func == dofile then
  return zprint
end

-- Otherwise, 'recon zprint.lua ...' runs as a script.

local cjson = require 'cjson'

if not arg[1] then
  io.stderr:write('usage: ', arg[0], ' <zprint-output-path>\n')
  os.exit(1)
end

local file
if arg[1] == '-' then
  file = io.stdin
else
  local err
  file, err = io.open(arg[1])
  if not file then
    io.stderr:write('zprint.lua: ', arg[1], ': open failed: ', err, '\n')
    os.exit(1)
  end
end

local zpout = file:read('all')
file:close()

local function collect(iter, arg)
  local tbl = {}
  for elt in iter(arg) do
    tbl[#tbl + 1] = elt
  end
  return tbl
end

local zones = collect(zprint.zones, zpout)
local tags = collect(zprint.tags, zpout)
local maps = collect(zprint.maps, zpout)
local zone_views = collect(zprint.zone_views, zpout)

print(cjson.encode({
  zones = zones,
  tags = tags,
  maps = maps,
  zone_views = zone_views,
}))
