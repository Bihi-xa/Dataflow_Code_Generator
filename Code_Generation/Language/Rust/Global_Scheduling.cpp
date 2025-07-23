#include "Scheduling.hpp"
#include "Config/config.h"
#include "Dataflow_Analysis/Scheduling_Lib/Scheduling_Lib.hpp"
#include "ABI/abi.hpp"

static std::string find_class_name(
	std::string actor_name,
	std::map<std::string, Actor_Conversion_Data*>& actor_data_map)
{
	return actor_data_map[actor_name]->get_class_name();
}

static std::string basic_non_preemptive(
	IR::Dataflow_Network* dpn,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::vector<std::string>& global_scheduling_routines,
	std::map<std::string, Actor_Conversion_Data*>& actor_data_map)
{
	std::string result;
	Config* c = c->getInstance();
	/*
	We are using Tokio’s multi-threaded runtime (Rust), meaning we dont need to manually manage:
	Actor sharing across threads
	Core-to-actor assignments
	Explicit locks, atomics, or thread joins
	*/

	std::vector<std::string> actors;
	if (c->get_topology_sort())
	{
		std::set<std::string> tmp;
		for (auto it = actor_data_map.begin(); it != actor_data_map.end(); ++it)
		{
			tmp.insert(it->first);
		}
		Scheduling::topology_sort(tmp, dpn, actors);
	}
	else
	{
		for (auto it = actor_data_map.begin(); it != actor_data_map.end(); ++it)
		{
			actors.push_back(it->first);
			// actors.push_back(it->second->get_class_name());
		}
	}
	if (c->get_sched_non_preemptive())
	{
		result.append("\t// Non-Preemtive Scheduling\n");
		result.append("\tlet handles: Vec<JoinHandle<()>> = vec![\n");

		for (auto it = actors.begin(); it != actors.end(); ++it)
		{
			result.append("\t\ttokio::spawn(async move {\n");
			result.append("\t\t\twhile !" + *it + ".is_done() { \n");
			result.append("\t\t\t\t" + *it + ".schedule().await;\n");
			result.append("\t\t\t}\n");
			result.append("\t\t}),\n");
		}
		result.append("\t];\n\n");

		result.append("\t// Wait for all actor tasks to finish\n");
		result.append("\tfor handle in handles {\n");
		result.append("\t\thandle.await.unwrap();\n");
		result.append("\t}\n");
		result.append("\tprintln!(\"all done!\");\n");
	}
	else if (c->get_sched_rr())
	{
		result.append("\t// Round-Robin Scheduling\n");
		// result.append("");
		result.append("\tloop {\n");
		result.append("\t\tlet ( ");
		for (auto it = actors.begin(); it != actors.end(); ++it)
		{
			if (it == actors.begin())
			{
				result.append(*it + "_done");
			}
			else
			{
				result.append(", " + *it + "_done");
			}
		}
		result.append(") = tokio::join!(\n");
		for (auto it = actors.begin(); it != actors.end(); ++it)
		{
			result.append("\t\t\tasync {\n");
			result.append("\t\t\t\tif !" + *it + ".is_done() {\n");
			result.append("\t\t\t\t\t" + *it + ".schedule().await;\n");
			result.append("\t\t\t\t}\n");
			result.append("\t\t\t\t" + *it + ".is_done()\n");
			result.append("\t\t\t},\n");
		}
		result.append("\t\t);\n");

		result.append("\n");

		result.append("\t\tif ");
		for (auto it = actors.begin(); it != actors.end(); ++it)
		{
			if (it == actors.begin())
			{
				result.append(*it + "_done");
			}
			else
			{
				result.append(" && " + *it + "_done");
			}
		}
		result.append(" {\n");
		result.append("\t\t\tprintln!(\"all done!\");\n");
		result.append("\t\t\tbreak;\n");
		result.append("\t\t}\n");

		result.append("\n");

		result.append("\t\ttokio::task::yield_now().await;\n");

		result.append("\t}\n");
	}

	for (unsigned i = 0; i < c->get_cores(); ++i)
	{
		global_scheduling_routines.push_back("global_scheduler");
	}

	return result;
}

std::string Scheduling::generate_global_scheduler_rust(
	IR::Dataflow_Network* dpn,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::vector<std::string>& global_scheduling_routines,
	std::map<std::string, Actor_Conversion_Data*>& actor_data_map)
{
	return basic_non_preemptive(dpn, opt_data1, opt_data2, map_data, global_scheduling_routines, actor_data_map);
}