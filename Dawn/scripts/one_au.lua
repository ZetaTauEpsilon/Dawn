-- Red War / 1AU. The registered C++ module owns encounters, cinematics,
-- native receipts and checkpoint recovery. Lua supplies the server entry point.
local composition = graph("composition", "1AU", sequence(
    step("mission", parallel("mission.native", "mission.finished"))
))

return mission{
    id = "one_au",
    graphs = {composition},
    roles = {mission = "composition"},
    entry = "composition",
    modules = {"native"},
    observations = {"mission.finished"},
}
