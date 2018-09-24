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
  local lines = zones:gmatch('([^\n]+)\n')

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

-- Iterate through the tags listed in the given zprint(1) output `zpout`.
function zprint.tags(zpout)
  -- Get to the third zone delimited by dashes, where the tags are recorded.
  local tags = lines_inside_dashes(zpout, 3)

  local lines = tags:gmatch('([^\n]+)\n')

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

    -- The line representing a region can take 4 different forms, depending on
    -- the type of region.  Check for each of them.

    -- Check for 4 columns.
    local name, maxsz_kb, cursz_kb = line:match(
        '(%S+)%s+%d+%s+%d+%s+(%d+)K%s+(%d+)K$')
    if not name then
      -- Check for 3 columns.
      name, maxsz_kb, cursz_kb = line:match('(%S+)%s+%d+%s+(%d+)K%s+(%d+)K$')
      if not name then
        -- Check for a two columns.
        name, cursz_kb = line:match('(%S+)%s+%d+%s+(%d+)K')
        if not name then
          -- Check for a single column.
          name, cursz_kb = line:match('(%S+)%s+(%d+)K')
        end
      end
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

function zprint.total(zpout)
  local total = zpout:match('total[^%d]+(%d+.%d+)M of')
  local bytes = tonumber(total) * 1024 * 1024
  return bytes
end

return zprint
