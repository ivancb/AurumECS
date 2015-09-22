#pragma once

namespace au {
	using ComponentIdType = size_t;

#ifndef COMPONENT_INFO
#define COMPONENT_INFO(x, id_value) static ::au::ComponentIdType Id() { return (::au::ComponentIdType)id_value; } \
	static const bool HasCustomMigrationHandling = false; \
	static const char* IdName() { return #x; } \
	size_t OwnerIndex
#define COMPONENT_INFO_PARENT(x, id_value) static ::au::ComponentIdType Id() { return (::au::ComponentIdType)id_value; } \
	static const bool HasCustomMigrationHandling = true; \
	static const char* IdName() { return #x; } \
	size_t OwnerIndex
#endif
}