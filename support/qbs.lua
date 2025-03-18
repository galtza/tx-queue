--[[
    =====================================================================================
    setup(_configurations)

    Initializes a premake5 workspace and projects

    Parameters:
        _configurations (table): the setup parameters with:

        ● "workspace_name" (string)
          The name of the workspace. If not specified, defaults to "DefaultWorkspace".

        ● "link_options" (table)
          A list of linker options to apply globally.
          Defaults to an empty table if not specified.

        ● "extra_configurations" (table)
          A list of tuples where each tuple contains:
            1. The name of the new configuration (string).
            2. The base configuration ("debug" or "release") from which
               settings are derived.
          All configurations (included the extra) will have preprocessor 
          macros defined based on their names, such as CONFIGURATION_DEBUG 
          for a "debug" configuration.

        ● "disabled_warnings" (table)
          A list of compiler warnings to disable.
          Defaults to an empty table if not specified.

        ● "toolset" (string)
          The toolset to use: "msc" or "clang"
          Defaults to "msc"

        ● "defines" (table)
          Global scope defines

        ● "start_config" (string)
          Specifies the initial project configuration. Defaults to "Debug" if omitted.

    Example Usage:

        local configurations = {
            workspace_name = "ExampleWorkspace",
            link_options = { "/NODEFAULTLIB:library" },
            extra_configurations = {
                { "beta", "release" },
                { "testing", "debug" }
            },
            defines = { "ELPP_THREAD_SAFE", "OTHER=1" }
            disabled_warnings = { "4100", "4201" }
        }
        setup(configurations)
    =====================================================================================
]]

local os = require("os")

qcs_target_folder = nil

function setup(_options)

    -- extract parameters

    local param_workspace_name    = _options.workspace_name or "DefaultWorkspace"
    local param_link_options      = _options.link_options or {}
    local param_extra_configs     = _options.extra_configurations or {}
    local param_disabled_warnings = _options.disabled_warnings or {}
    local param_toolset           = _options.toolset or "msc"
    local param_defines           = _options.defines or {}
    local param_start_config      = _options.start_config or "debug"
    local param_cppdialect        = _options.cppver or "C++20"
    local param_warnings          = _options.warnings or "Everything"
    local param_start_project     = _options.start_project or nil

    -- sensible warning deactivation

    if param_toolset == "msc" then
        table.insert(param_disabled_warnings, "4577") -- 'noexcept' used with no exception handling mode specified; termination on exception is not guaranteed. Specify /EHsc
        table.insert(param_disabled_warnings, "4820") -- 'CLASS': 'N' bytes padding added after data member 'MEMBER'
        table.insert(param_disabled_warnings, "5045") -- Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
        table.insert(param_disabled_warnings, "4710") -- 'FUNCTION': function not inlined
        table.insert(param_disabled_warnings, "4711") -- function 'FUNCTION' selected for automatic inline expansion
        table.insert(param_disabled_warnings, "4266") -- 'function' : no override available for virtual member function from base 'type'; function is hidden
        table.insert(param_disabled_warnings, "4514") -- 'function' : unreferenced inline function has been removed
        table.insert(param_disabled_warnings, "5264") -- 'variable-name': 'const' variable is not used
        table.insert(param_disabled_warnings, "5245") -- 'function': unreferenced function with internal linkage has been removed
    end

    -- start the workspace

    workspace(param_workspace_name)

    -- platforms and architecture

    platforms    { "x64" }
    architecture "x86_64"

    -- configurations

    local default_configurations = { "debug", "release" }
    if not _config_exists(default_configurations, param_extra_configs, param_start_config) then
        error("the specified start config \"" .. param_start_config .. "\" does not exist")
    end

    local final_configurations = { param_start_config }

    for _, config in ipairs (default_configurations) do
        if config ~= param_start_config then
            table.insert(final_configurations, config)
        end
    end

    for _, config_pair in ipairs(param_extra_configs) do
        if config_pair[1] ~= param_start_config then
            table.insert(final_configurations, config_pair[1])
        end
    end

    configurations(final_configurations)

    filter "configurations:debug"   defines { "CONFIGURATION_DEBUG"             } symbols "On"
    filter "configurations:release" defines { "CONFIGURATION_RELEASE", "NDEBUG" } optimize "On"

    for _, config in ipairs(param_extra_configs) do
        filter { "configurations:" .. config[1] }
        defines { "CONFIGURATION_" .. string.upper(config[1]), "CONFIGURATION_" .. string.upper(config[2]) }
        if config[2] == "debug" then
            symbols "On"
        elseif config[2] == "release" then
            optimize "On"
        end
    end

    filter {}

    -- language & compiler settings

    language          "C++"
    cppdialect        (param_cppdialect)
    warnings          (param_warnings)
    exceptionhandling "off"
    flags             { "FatalCompileWarnings", "FatalLinkWarnings", "MultiProcessorCompile" }
    toolset           (param_toolset)
    editandcontinue   "Off"
    justmycode        "Off"
    characterset      "Unicode"
    buildoptions      "/utf-8"
    if _options.staticruntime then
        staticruntime "on"
    end

    -- folders

    qcs_target_folder = path.join(_MAIN_SCRIPT_DIR, ".out/%{cfg.platform}/%{cfg.buildcfg}")
    location    ".build"
    targetdir   (".out/%{cfg.platform}/%{cfg.buildcfg}")
    objdir      (".tmp/%{prj.name}")

    -- includes and libs paths

    local includedirs_in_folders = _scan_includedirs()
    local libdirs_in_folders = _scan_libdirs(param_extra_configs)

    if #includedirs_in_folders > 0 then
        includedirs(includedirs_in_folders)
    end

    for key, paths in pairs(libdirs_in_folders) do
        table.insert(paths, ".out/%{cfg.platform}/%{cfg.buildcfg}")
    end

    -- dynamic filters for libdirs

    for config_name, paths in pairs(libdirs_in_folders) do
        filter("configurations:" .. config_name)
        libdirs(paths)
    end
    filter {}

    -- debug

    debugdir (_MAIN_SCRIPT_DIR)

    -- Apply global defines to the workspace

    for _, define in ipairs(param_defines) do
        defines { define }
    end

    -- link options

    if #param_link_options > 0 then
        linkoptions(param_link_options)
    end

    -- Disable specified warnings if any

    if #param_disabled_warnings > 0 then
        disablewarnings { param_disabled_warnings }
    end

    -- include all the projects and select the start project as well

    if _OPTIONS["debug"] then
        print ("QCS: Scanning projects...")
    end

    local projects = os.matchfiles("**.proj.lua")

    if _OPTIONS["debug"] then
        for _, project_file in ipairs(projects) do
            print("QCS:     Found project: " .. project_file)
        end
    end

    _select_and_set_start_project(projects, param_start_project)
    for _, project_file in ipairs(projects) do
        if _OPTIONS["debug"] then
            print ("QCS:     Including " .. project_file)
        end
        dofile(project_file)
    end

    -- generate temporary folders

    create_temporary_folder_links({ ".build", ".tmp", ".out" })

    -- debug

    if _OPTIONS["debug"] then
        print ("")
    end

end

--[[
    ============================================================
    given a list of project file paths, it sets the startproject
    ============================================================
--]]

function _select_and_set_start_project(_projects, _start_project)

    -- If a start project is explicitly specified, set it and return early

    if _start_project then
        if _OPTIONS["debug"] then
            print("QCS: Setting start project to " .. _start_project)
        end
        startproject(_start_project)
        return
    end

    -- If not explicit we choose the first one on the root of the workspace
    -- that is not misc.proj.lua (special type of project)

    local filtered_projects = {}

    -- Exclude 'misc.proj.lua' and sort by hierarchy level

    for _, file in ipairs(_projects) do
        if not file:find("misc.proj.lua") then
            table.insert(filtered_projects, file)
        end
    end

    table.sort(filtered_projects, function(a, b)
        local countA = select(2, a:gsub("/", "/"))
        local countB = select(2, b:gsub("/", "/"))
        return countA < countB
    end)

    -- Select the first project

    if #filtered_projects > 0 then
        local start_project_name = _extract_project_name(filtered_projects[1])
        if start_project_name then
            if _OPTIONS["debug"] then
                print("QCS: Setting start project to " .. start_project_name)
            end
            startproject(start_project_name)
        end
    end
end

--[[
    =====================================================
    given a project file, extract the name of the project
    =====================================================
--]]

function _extract_project_name(file)
    local result = nil
    for line in io.lines(file) do
        result = line:match('project%s*"([^"]+)"')
        if result then
            break
        end
    end
    return result
end

--[[
    =====================================================================================
    given a folder and a filename it finds recursively all the folders where it was found
    =====================================================================================
--]]

function _find_directories_with_file(start_folder, filename)
    local search_pattern = start_folder .. "/**/" .. filename
    local root_file_path = start_folder .. "/" .. filename
    local unique_paths = {}

    -- Filename in the root

    if os.isfile(root_file_path) then
        unique_paths[start_folder] = true
    end

    -- Subfolders

    local files_found = os.matchfiles(search_pattern)
    for _, file in ipairs(files_found) do
        unique_paths[path.getdirectory(file)] = true
    end

    -- Convert to list

    local result = {}
    for p, _ in pairs(unique_paths) do
        table.insert(result, p)
    end
    return result
end

--[[
    ============================
    find the include directories
    ============================
--]]

function _scan_includedirs()

    local result = _find_directories_with_file(os.getcwd(), ".includedirs")
    if _OPTIONS["debug"] then
        print ("QCS: Scanning .includedirs at " .. os.getcwd() .. "...")
        for _, directory_path in ipairs(result) do
            print("QCS:     Adding \"" .. directory_path .. "\"")
        end
    end
    return result
end

--[[
    ============================
    find the library directories
    ============================
--]]

function _scan_libdirs(_extra_configurations)

    if _OPTIONS["debug"] then
        print ("QCS: Scanning libdirs...")
    end

    -- mapping between the extra configuration and one of the
    -- default ones and initialize the result table

    local result  = { debug = {}, release = {} }
    local aliases = { debug = {}, release = {} }
    for _, config in ipairs(_extra_configurations) do
        result[config[1]] = {}
        if config[2] == "debug" or config[2] == "release" then
            table.insert(aliases[config[2]], config[1])
        end
    end

    -- some local functions

    local function remove_comments(_line)
        return _line:gsub("%-%-.*", "")
    end

    local function extract_configurations_from_braces(_line)
        local configurations = {}
        local brace_content = _line:match("{(.*)}")
        if brace_content then
            for config in brace_content:gmatch("([^,]+)") do
                config = config:match("^%s*(.-)%s*$")
                table.insert(configurations, config)
            end
        end
        return configurations
    end

    -- iterate over all the .libdirs files

    local libdirs = _find_directories_with_file(os.getcwd(), ".libdirs")
    for _, directory_path in ipairs(libdirs) do

        -- Read line by line

        local found_match = false;
        local file_path = directory_path .. "/.libdirs"
        for line in io.lines(file_path) do
            local clean_line = remove_comments(line)
            local configurations = extract_configurations_from_braces(clean_line)

            if #configurations > 0 then
                for _, config in ipairs(configurations) do
                    if config ~= "debug" and config ~= "release" then
                        error("Unknown configuration '" .. config .. "' found in " .. file_path .. "it must be \"debug\" or \"release\"")
                        break
                    end

                    found_match = true
                    table.insert(result[config], directory_path)

                    if _OPTIONS["debug"] then
                        print ("QCS:     Adding \"" .. directory_path .. "\" to: \"" .. config .. "\" configuration")
                    end

                    for _, alias in ipairs(aliases[config]) do
                        table.insert(result[alias], directory_path)
                        if _OPTIONS["debug"] then
                            print ("QCS:     Adding \"" .. directory_path .. "\" to: \"" .. alias .. "\" configuration")
                        end
                    end
                end
            end
        end

        -- Add to all the configurations if there was no content in .libdirs file

        if not found_match then
            if _OPTIONS["debug"] then
                print ("QCS:     Adding \"" .. directory_path .. "\" to: \"debug\" configuration")
                print ("QCS:     Adding \"" .. directory_path .. "\" to: \"release\" configuration")
            end
            table.insert(result["debug"], directory_path)
            table.insert(result["release"], directory_path)
            for _, config in ipairs(_extra_configurations) do
                table.insert(result[config[1]], directory_path)
                if _OPTIONS["debug"] then
                    print ("QCS:     Adding \"" .. directory_path .. "\" to: \"" .. config[1] .. "\" configuration")
                end
            end
        end

    end

    return result
end

--[[
    =============================================
    Generate temporary folders in the temp folder
    =============================================
--]]

function create_temporary_folder_links(_folders)
    for _, _relative_path in ipairs(_folders) do
        local temp_folder_path = os.tmpname() .. _relative_path
        os.execute("mkdir \"" .. temp_folder_path .. "\"")
        os.execute("mklink /D \"" .. _relative_path .. "\" \"" .. temp_folder_path .. "\"")
    end
end

--[[
    ======================
    Find an item in a list
    ======================
--]]

function _config_exists(_default_configs, _extra_configs, _target_config)

    for _, config in ipairs(_default_configs) do
        if config == _target_config then
            return true
        end
    end

    for _, config_pair in ipairs(_extra_configs) do
        if config_pair[1] == _target_config then
            return true
        end
    end

    return false
end

-- our options

newoption {
    trigger = "debug",
    description = "Enable verbose output mode"
}

