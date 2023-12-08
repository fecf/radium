#include "service_locator.h"

std::map<std::pair<size_t, int>, std::shared_ptr<void>> ServiceLocator::map;
