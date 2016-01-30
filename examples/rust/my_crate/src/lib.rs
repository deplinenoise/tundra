extern crate rand;
use rand::Rng;

pub fn get_rand() -> u32 {
    // test
    let mut t = rand::thread_rng();
    t.gen::<u32>()
}
