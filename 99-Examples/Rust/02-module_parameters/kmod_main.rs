#![allow(missing_docs)]

use kernel::prelude::*;

// https://github.com/Rust-for-Linux/linux/issues/1042
// The param is not supported in the mainline kernel yet

module! {
    type: HelloWorld,
    name: "hello_world",
    author: "Douglas Su",
    description: "A simple hello world example",
    license: "GPL",
    params: {
        howmany: i32 {
            default: 3,
            permissions: 0o644,
            description: "How many times string will be printed",
        },
        whom: str {
            default: "Mom",
            permissions: 0o644,
            description: "What string to be printed",
        },
    },
}

struct HelloWorld;

impl kernel::Module for HelloWorld {
    fn init(_module: &'static ThisModule) -> Result<Self> {
        pr_info!("Hello world from rust!\n");

        Ok(HelloWorld)
    }
}

impl Drop for HelloWorld {
    fn drop(&mut self) {
        pr_info!("Bye world from rust!\n");
    }
}
