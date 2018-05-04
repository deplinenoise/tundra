#ifndef DAGGENERATOR_HPP
#define DAGGENERATOR_HPP

#include "MemAllocHeap.hpp"

struct lua_State;

namespace t2
{

bool GenerateDag(const char* build_file, const char* dag_fn);

bool GenerateIdeIntegrationFiles(const char* build_file, int argc, const char** argv);

bool GenerateTemplateFiles(int argc, const char** argv);

}

#endif
