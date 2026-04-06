#pragma once

#include <stddef.h>

struct ParameterDefinition {
  const char *key;
  const char *title;
  const char *description;
  const char *range;
  const char *defaultValue;
};

size_t getParameterDefinitionCount();
const ParameterDefinition *getParameterDefinitionAt(size_t index);
