extern {
    fn print_hello();
}

// foo

#[no_mangle]
pub extern fn call_me() {
    unsafe { print_hello() }
}
