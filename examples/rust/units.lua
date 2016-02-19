require "tundra.syntax.rust-cargo"
require "tundra.syntax.glob"

-- Static library that is used by a Rust Program and Rust SharedLibrary
StaticLibrary {
	Name = "test_lib",
	Sources = { "native_lib/lib.c" },
}

-- Rust create to build a static crate with Cargo

RustCrate {
	Name = "my_crate",
	CargoConfig = "my_crate/Cargo.toml",
	Sources = { "my_crate/src/lib.rs" },
}

-- Rust program that uses a local crate (set in Cargo.toml) and a static library
-- that is built by Tundra and then linked to the final executable by Cargo

RustProgram {
	Name = "prog",
	CargoConfig = "prog/Cargo.toml",
	Sources = { "prog/src/main.rs", "prog/build.rs" },
	Depends = { "test_lib", "my_crate" },
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

