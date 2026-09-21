-- Red War / Adieu. The registered C++ module owns encounters, cinematics,
-- dialogue timing and native receipts. Lua supplies the server entry point.
local composition = graph("composition", "Adieu", sequence(
    step("mission", parallel("mission.native", "mission.finished"))
))

return mission{
    id = "adieu",
    graphs = {composition},
    roles = {mission = "composition"},
    entry = "composition",
    modules = {"native"},
    observations = {"mission.finished"},
}
