## Introduction
AurumECS is an experimental header-only C++ library which makes heavy usage of variadic templates and template metaprogramming to provide a type safe, data-oriented and ownership-based entity component system. Developed with the aim of being used in games.

## Features
* Double buffered components which allow processes to read and write to the same component at the same time.
* Ownership-based iterator design which makes the relationship between a process and the data it requires to function and modifies explicit. This also prevents multiple processes from writing to the same component at the same time (unless the user provides assurances that it is safe, see [shared authority] for an example).
* Data-oriented - components are just POD with some helper functions.
* Support for multithreaded world updates.
* Builtin timing for each step performed during world ticks.
* Header-only - simply add the include directory in your project's include paths and it's ready to use.

Note that due to its design it also has a higher memory usage than other ECS systems and is slightly more complex when dealing with large amount of components.

## Requirements
* A C++11 compiler (tested with Visual Studio 2015)
* variadic-variant (https://github.com/kmicklas/variadic-variant) - A type safe, C++11 based variant library, used for the component actions structure.

## Documentation
Coming soon, see [Examples] for now.

## Examples
A Visual Studio 2015 solution with examples is available under [Examples].
* [basic] - An extremely simple example of how the project can be used.
* [shared authority] - Demonstrates how two or more processes can modify the same component simultaneously.
* [multithreaded] - Shows how dispatchers can be used to select wether to parallelize process execution or not.

## Pending Work
* Review and stabilize World API
* Add unit tests
* Add support for specifying that components don't need to be destroyed
* Add an entity templating system
* Add event hooks (OnEntityAdded and OnEntityRemoved for example)
* Improvement performance of component iteration
* Improve multithreading support and indicate which parts are threadsafe
* Split World.h into more manageable chunks
* Add support for user logging

[basic]: ./examples/basic.cpp
[shared authority]: ./examples/basic_shared_authority.cpp
[multithreaded]: ./examples/mt_experimental.cpp
[Examples]: ./examples