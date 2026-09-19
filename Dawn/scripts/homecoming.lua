-- Red War / Homecoming. The registered C++ module owns encounters, cinematics,
-- dialogue timing and native receipts. Lua supplies the server entry point.
local composition = graph("composition", "Homecoming", sequence(
    step("mission", parallel("mission.native", "mission.finished"))
))

return mission{
    id = "homecoming",
    graphs = {composition},
    roles = {mission = "composition"},
    entry = "composition",
    modules = {"native"},
    observations = {"mission.finished"},
}
