require "tundra.syntax.rust-cargo"
require "tundra.syntax.glob"

-- Static library that is used by a Rust Program and Rust SharedLibrary
StaticLibrary {
	Name = "test_lib",
	Sources = { "native_lib/lib.c" },
}

-- This is for Tundra to know about when to (re) build a local crate (which has 
-- a dependency in the Cargo.toml we can just make sure Tundra knows about the source
-- files so cargo we be executed if something changes in the crate

local my_crate_sources = { Glob { Dir = "my_crate/src", Extensions = { ".rs" } } }

-- Rust program that uses a local crate (set in Cargo.toml) and a static library
-- that is built by Tundra and then linked to the final executable by Cargo

RustProgram {
	Name = "prog",
	CargoConfig = "prog/Cargo.toml",
	Sources = { my_crate_sources, 
				"prog/src/main.rs", "prog/build.rs" },
	Depends = { "test_lib" },
}

-- Rust SharedLibrary that uses a static library (test_lib)

RustSharedLibrary {
	Name = "shared_lib",
	CargoConfig = "shared_lib/Cargo.toml",
	Sources = { "shared_lib/src/lib.rs", "shared_lib/build.rs" },
	Depends = { "test_lib" },
}

Default "prog"
Default "shared_lib"

