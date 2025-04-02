#![allow(missing_docs)]

use kernel::prelude::*;

module! {
    type: HelloWorld,
    name: "hello_world",
    author: "Douglas Su",
    description: "A simple hello world example",
    license: "GPL",
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
