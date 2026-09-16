#pragma once

#include "sort.hpp"
#include "typing.hpp"
#include <cstddef>
#include <cstdint>
#include <flat_map>

namespace mold::internal {

std::flat_map<NodeId, std::size_t> ranks_of(const SortResult &sort);

};
