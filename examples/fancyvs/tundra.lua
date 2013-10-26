
Build {

  Units = function ()
    local lib = StaticLibrary {
      Name = "blahlib",
      Sources = {
        "blah.hpp",   -- including headers here makes them appear in generated solutions
        "blah.cpp",
      },

      -- Place in MSVC solution folder.
      IdeGenerationHints = {
        Msvc = {
          SolutionFolder = "Libraries",
        }
      },
    }

    local prog = Program {
      Name = "prog",
      Sources = { "main.cpp" },
      Depends = { lib },

      -- Place in MSVC solution folder.
      IdeGenerationHints = {
        Msvc = {
          SolutionFolder = "Other Stuff",
        }
      },
    }

    Default(prog)
  end,

  Env = {
    CXXOPTS = {
      "/W4",
      { "/O2"; Config = "*-vs2012-release" },
    },
    GENERATE_PDB = {
      { "0"; Config = "*-vs2012-release" },
      { "1"; Config = { "*-vs2012-debug", "*-vs2012-production" } },
    }
  },

  Configs = {
    Config {
      Name = 'win64-vs2012',
      Tools = { { "msvc-vs2012"; TargetArch = "x64" }, },
      DefaultOnHost = "windows",
    },
    Config {
      Name = 'win32-vs2012',
      Tools = { { "msvc-vs2012"; TargetArch = "x86" }, },
      SupportedHosts = { "windows" },
    },
  },

  IdeGenerationHints = {
    Msvc = {
      -- Remap config names to MSVC platform names (affects things like header scanning & debugging)
      PlatformMappings = {
        ['win64-vs2012'] = 'x64',
        ['win32-vs2012'] = 'Win32',
      },
      -- Remap variant names to MSVC friendly names
      VariantMappings = {
        ['release']    = 'Release',
        ['debug']      = 'Debug',
        ['production'] = 'Production',
      },
    },

    -- Override output directory for sln/vcxproj files.
    MsvcSolutionDir = 'vs2012',

    -- Override solutions to generate and what units to put where.
    MsvcSolutions = {
      ['Everything.sln'] = {},          -- receives all the units due to empty set
      ['ProgramOnly.sln'] = { Projects = { "prog" } },
      ['LibOnly.sln'] = { Projects = { "blahlib" } },
    },

    -- Cause all projects to have "Build" ticked on them inside the MSVC Configuration Manager.
    -- As a result of this, you can choose a project as the "Startup Project",
    -- and when hitting "Debug" or "Run", the IDE will build that project before running it.
    -- You will want to avoid pressing "Build Solution" with this option turned on, because MSVC
    -- will kick off all project builds simultaneously.
    BuildAllByDefault = true,
  }
}
