-- include QCStudio build script

include "support/qbs.lua"

-- build the workspace and projects

local config = {
    workspace_name = "tx-queue",
    link_options = {"/ignore:4006,4099"},
    start_project = "intra",
}

setup(config)
