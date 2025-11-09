#pragma once

#include <unordered_map>

#include "types.hpp"

namespace route_finder
{

bool is_valid_assignment(const Student &student, const Centre &centre);
void run_batch_greedy_allotment();

} // namespace route_finder


