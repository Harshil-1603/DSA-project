#include "route_finder/allotment.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <limits>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "route_finder/state.hpp"

namespace route_finder
{
namespace
{

using AssignmentQueue = std::priority_queue<AssignmentPair, std::vector<AssignmentPair>, std::greater<AssignmentPair>>;

void process_priority_queue(
    AssignmentQueue &queue,
    std::set<std::string> &assigned_students,
    std::unordered_map<std::string, int> &centre_loads)
{
    while (!queue.empty())
    {
        const auto assignment = queue.top();
        queue.pop();

        if (assigned_students.count(assignment.student_id))
        {
            continue;
        }

        Centre *target_centre = nullptr;
        for (auto &centre : centres)
        {
            if (centre.centre_id == assignment.centre_id)
            {
                target_centre = &centre;
                break;
            }
        }

        if (!target_centre)
        {
            continue;
        }

        if (centre_loads[assignment.centre_id] >= target_centre->max_capacity)
        {
            continue;
        }

        final_assignments[assignment.student_id] = assignment.centre_id;
        assigned_students.insert(assignment.student_id);
        centre_loads[assignment.centre_id]++;
        target_centre->current_load = centre_loads[assignment.centre_id];
    }
}

void enqueue_student_options(
    const Student &student,
    AssignmentQueue &queue)
{
    const auto lookup_it = allotment_lookup_map.find(student.snapped_node_id);
    if (lookup_it == allotment_lookup_map.end())
    {
        return;
    }

    const auto &centre_distances = lookup_it->second;

    for (const auto &centre : centres)
    {
        if (!is_valid_assignment(student, centre))
        {
            continue;
        }

        const auto dist_it = centre_distances.find(centre.centre_id);
        if (dist_it == centre_distances.end())
        {
            continue;
        }

        const double distance = dist_it->second;
        if (distance == std::numeric_limits<double>::max())
        {
            continue;
        }

        queue.push({distance, student.student_id, centre.centre_id});
    }
}

} // namespace

bool is_valid_assignment(const Student &, const Centre &)
{
    // All centres accept all students in the current data model.
    return true;
}

void run_batch_greedy_allotment()
{
    std::cout << "Running tiered distance-first allotment..." << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();

    std::set<std::string> assigned_students;
    std::unordered_map<std::string, int> centre_loads;
    for (auto &centre : centres)
    {
        centre_loads[centre.centre_id] = 0;
        centre.current_load = 0;
    }

    final_assignments.clear();

    std::vector<Student> female_students;
    std::vector<Student> pwd_students;
    std::vector<Student> male_students;

    for (const auto &student : students)
    {
        if (student.category == "female")
        {
            female_students.push_back(student);
        }
        else if (student.category == "pwd")
        {
            pwd_students.push_back(student);
        }
        else
        {
            male_students.push_back(student);
        }
    }

    std::cout << "Student distribution (male=" << male_students.size()
              << ", pwd=" << pwd_students.size()
              << ", female=" << female_students.size() << ")." << std::endl;

    AssignmentQueue queue_male;
    for (const auto &student : male_students)
    {
        enqueue_student_options(student, queue_male);
    }
    process_priority_queue(queue_male, assigned_students, centre_loads);
    std::cout << "Assigned " << assigned_students.size() << " male students." << std::endl;

    AssignmentQueue queue_pwd;
    const size_t male_count = assigned_students.size();
    for (const auto &student : pwd_students)
    {
        enqueue_student_options(student, queue_pwd);
    }
    process_priority_queue(queue_pwd, assigned_students, centre_loads);
    std::cout << "Assigned " << assigned_students.size() - male_count << " PwD students." << std::endl;

    AssignmentQueue queue_female;
    const size_t male_pwd_count = assigned_students.size();
    for (const auto &student : female_students)
    {
        enqueue_student_options(student, queue_female);
    }
    process_priority_queue(queue_female, assigned_students, centre_loads);
    std::cout << "Assigned " << assigned_students.size() - male_pwd_count << " female students." << std::endl;

    auto end_time = std::chrono::high_resolution_clock::now();
    const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "Allotment complete: " << assigned_students.size() << " of "
              << students.size() << " students assigned in "
              << total_ms << " ms." << std::endl;
}

} // namespace route_finder


