package.cpath = package.cpath .. ";./build/?.so"

math.randomseed(os.clock()*100000)

local sfmt = string.format 
local fprint = function (s, ...)
    print(sfmt(s,...))
end

local mstimer = require "mstimer"

wt = mstimer.new()

local funcs = {}

local t1 = mstimer.gettime()

fprint("---time sequence test---")

local session = 30
for i=1, session do
    local t = math.random(1,1000)
    funcs[i] = function ()
        fprint("--time %sms callback at %sms--", t, (mstimer.gettime()-t1)*1000)
    end
    wt:add(i, t, 1)
end

while true do
    local s = wt:recv()
    if not s then
        fprint("error 1")
        break
    end
    if s == -1 then
        fprint("error 2")
        break
    end
    funcs[s]()
    funcs[s] = nil
    if next(funcs) == nil then
        break
    end
end

print("press enter to next ...")
io.read()

fprint("---time loop test---")
local loop_cnt = 30
local new_session = session + 1
local t0 = mstimer.gettime()
local t1 = t0
local interval = 33
fprint("push timer interval %sms loopcnt %s",interval,loop_cnt)
wt:add(new_session, interval, loop_cnt)

local cb_cnt = 0
funcs[new_session] = function ( ... )
    local t2 = mstimer.gettime()
    fprint("call back at %sms",(t2 -t1)*1000)
    t1 = t2
    cb_cnt = cb_cnt + 1
    if cb_cnt >= loop_cnt then
        funcs[new_session] = nil
        fprint("total use time %sms",(t2 - t0)*1000)
    end
end

while true do
    local s = wt:recv()
    if not s then
        fprint("error 1")
        break
    end
    if s == -1 then
        fprint("error 2")
        break
    end
    funcs[s]()
    if next(funcs) == nil then
        break
    end
end

fprint("---gc test---")
wt = nil
collectgarbage("collect")
os.execute("sleep 1")

fprint("---test-end-----")