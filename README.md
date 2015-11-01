# Introduction
AurumECS is an experimental header-only C++ which makes heavy usage of C++11 variadic templates and template metaprogramming to provide a type safe, data-oriented and ownership-based entity component system. It was mainly developed to be used in games.

# Features
* Double buffered components which allow processes to read and write to the same component at the same time.
* Ownership-based iterator design which makes the relationship between a process and the data it requires to function and modifies explicit. This also prevents multiple processes from writing to the same component at the same time (unless the user provides assurances that it is safe, see [basic_shared_auth] for an example).
* Data-oriented - components are just POD with some helper functions.
* Support for multithreaded World processing and updates
* Builtin timing for each step during a World's update.

Note that due to its design it also has a higher memory usage than other ECS systems and is slightly more complex when dealing with large amount of components.

# Requirements
* C++11 compiler (tested with Visual Studio 2015)
* variadic-variant (https://github.com/kmicklas/variadic-variant) - A type safe, C++11 based variant library, used for the component actions structure.

# Documentation
Coming soon, see [Examples] for now.

# Examples
* basic.cpp - Demonstrates basic ECS functionality.
* basic_shared_authority.cpp - Demonstrates how two or more processes can modify the same component simultaneously.
* mt_experimental.cpp - Example of how to use a MultithreadedDispatcher to parallelize process execution.

# Pending Work
* Review and stabilize World API
* Add unit tests
* Improvement performance of component iteration
* Improve multithreading support and indicate which parts are threadsafe
* Split World.h into more manageable chunks
* Add a template system
* Add event hooks (OnEntityAdded and OnEntityRemoved for example)
* Support user logging
* Add support for specifying that components don't need to be destroyed

[basic_shared_auth]: ./examples/basic_shared_authority.cpp
[Examples]: ./examples