extern crate rand;
use rand::Rng;

pub fn get_rand() -> u32 {
    let mut t = rand::thread_rng();
    t.gen::<u32>()
}

#[test]
fn create_test() {
}
