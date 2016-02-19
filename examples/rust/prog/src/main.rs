extern crate my_crate;

extern {
    fn print_hello();
}

fn main() {
    unsafe { 
        print_hello() 
    }
    println!("some value {}", my_crate::get_rand())
}

#[test]
fn passing_test() {
}
