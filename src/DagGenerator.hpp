#ifndef DAGGENERATOR_HPP
#define DAGGENERATOR_HPP

#include "MemAllocHeap.hpp"

struct lua_State;

namespace t2
{

bool GenerateDag(const char* build_file, const char* dag_fn, const char* globals);

bool GenerateIdeIntegrationFiles(const char* build_file, int argc, const char** argv, const char* globals);

}

#endif
