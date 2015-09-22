# AurumECS
An experimental C++ ECS library which makes heavy usage of C++11 variadic templates and template metaprogramming to provide a type safe, data-oriented and ownership-based component system, developed with usage in games as its focus.

Its functionality is split over 3 parts:
* Components - Small groups of related data, with no actual logic (other than copy or move operations if necessary)
* Processes - Classes which request iterators for Components and perform a function on them.
* Worlds - Contain the components and processes, while providing an ownership-based method of iterating over components

### Advantages
* Double buffered components - allows processes to both read and write to the same component, at the same time.
* Ownership-based iterator design - ensures that multiple processes can't write to the same component, at the same time, making the relationship between a process, the data it requires to function and the data it modifies explicit.
* Easy to define and use new components and processes.
* Data-oriented design allows for better cache usage by components.
* Focus on eliminating virtual calls within World (however, an IWorld interface exists, which contains mostly unsafe operations).

### Disadvantages
* Higher memory usage (in an ideal scenario, roughly twice of most ECS systems due to the double buffered component containers)
* Complexity when dealing with processes that interact with a large amount of components or Worlds with a large number of component containers.

### Comparisons

# Pending Work
* Finish multithreading support (at the moment, some parts of World, such as ID generation, are not threadsafe)
* Review and stabilize World API
* Split World.h
* Make this into a header only library
* Add a template system
* Add event hooks (OnEntityAdded and OnEntityRemoved for example)
* Support user defined logging

# Dependencies
* variadic-variant (https://github.com/kmicklas/variadic-variant) - A type safe, C++11 based variant library, used for the component actions structure.

# Documentation
Coming soon, see Examples for now.

# Examples
* basic.cpp - Demonstrates basic ECS functionality.
* basic_shared_authority.cpp - Demonstrates how two or more processes can modify the same component simultaneously.