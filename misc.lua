
function newobject(class)
  local obj = {}
  setmetatable(obj, class)
  obj.class = class
  class.__index = class
  return obj
end

function each(array_or_eachable_obj)
  if array_or_eachable_obj.class then
    return array_or_eachable_obj:each()
  else
    local array = array_or_eachable_obj
    local i = 0
    return function ()
      i = i + 1
      if array[i] then return array[i] else return nil end
    end
  end
end

function breadth_first_traversal(obj, children_func)
  local seen = Set:new{obj}
  local queue = Queue:new(obj)
  while not queue:isempty() do
    local node = queue:dequeue()
    children = children_func(node) or {}
    for child_node in each(children) do
      if seen:contains(child_node) == false then
        seen:add(child_node)
        queue:enqueue(child_node)
      end
    end
  end
  return seen
end

function transitions_for(transitions, int)
  local targets = Set:new()
  for int_set, target in pairs(transitions) do
    if type(int_set) == "table" and int_set:contains(int) then
      targets:add(target)
    end
  end
  return targets
end

-- all ints within each IntSet are assumed to be equivalent.
-- Given this, return a new list of IntSets, where each IntSet
-- returned is considered equivalent across ALL IntSets.
function equivalence_classes(int_sets)
  local BEGIN = 0
  local END = 1

  local events = {}
  for int_set in each(int_sets) do
    if int_set.negated then int_set = int_set:invert() end
    for range in each(int_set.list) do
      table.insert(events, {range.low, BEGIN, int_set})
      table.insert(events, {range.high+1, END, int_set})
    end
  end

  local cmp_events = function(a, b)
    if a[1] == b[1] then
      return b[2] < a[2]   -- END events should go before BEGIN events
    else
      return a[1] < b[1]
    end
  end

  table.sort(events, cmp_events)

  local nested_regions = Set:new()
  local last_offset = nil
  classes = {}
  for event in each(events) do
    local offset, event_type, int_set = unpack(event)

    if last_offset and last_offset < offset and (not nested_regions:isempty()) then
      local hk = nested_regions:hash_key()
      classes[hk] = classes[hk] or IntSet:new()
      classes[hk]:add(Range:new(last_offset, offset-1))
    end

    if event_type == BEGIN then
      nested_regions:add(int_set)
    else
      nested_regions:remove(int_set)
    end
    last_offset = offset
  end

  local ret = {}
  for hk, int_set in pairs(classes) do
    table.insert(ret, int_set)
  end
  return ret
end

