module(..., package.seeall)

function apply(env, options)
  env:set_many {
    ["RUST_SUFFIXES"] = { ".rs", },
    ["RUST_CARGO"] = "cargo",
    ["RUSTC"] = "rustc",
  }
end

