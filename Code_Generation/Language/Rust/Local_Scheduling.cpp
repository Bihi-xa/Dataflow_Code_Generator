#include "Scheduling.hpp"
#include "Dataflow_Analysis/Scheduling_Lib/Scheduling_Lib.hpp"
#include "Config/debug.h"
#include "Config/config.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include <sstream>
#include <tuple>
#include "String_Helper.h"
#include "ABI/abi.hpp"

static std::string transform_string(const std::string &input)
{
	std::vector<std::string> parts;
	std::stringstream ss(input);
	std::string item;

	while (std::getline(ss, item, ','))
	{
		parts.push_back(item);
	}

	std::string result;
	for (size_t i = 0; i < parts.size(); ++i)
	{
		// Remove leading/trailing spaces
		parts[i] = parts[i].substr(parts[i].find_first_not_of(" "), parts[i].find_last_not_of(" ") + 1);
		if (i > 0)
			result += ", ";
		result += "Some(" + parts[i] + ")";
	}

	return result;
}
static std::string default_local(
	std::map<std::string, std::vector<Scheduling::Channel_Schedule_Data>> &actions,
	std::vector<IR::FSM_Entry> &fsm,
	std::vector<IR::Priority_Entry> &priorities,
	Actor_Classification input_classification,
	Actor_Classification output_classification,
	std::string prefix,
	// maps method names to their scheduling condition: guards and size and free space in the FIFOs
	std::map<std::string, std::string> &action_guard,
	std::map<std::string, std::string> &action_schedulingCondition_map,
	std::map<std::string, std::vector<std::string>> &channel_read_map,
	std::map<std::string, std::string> &action_freeSpaceCondition_map,
	std::map<std::string, std::string> &droping_channel_sender,
	std::map<std::string, std::string> &cond_receiver_map,
	std::map<std::string, std::set<std::string>> &action_param_read,
	std::map<std::string, std::string> &state_channel_access,
	std::map<std::string, std::string> &action_post_exec_code_map,
	std::string loop_exp,
	bool round_robin,
	std::string schedule_function_name,
	std::string schedule_function_parameter)
{
	Config *c = c->getInstance();
	std::string output{};
	std::string local_prefix;
	std::string buffered_prefix = prefix;
	bool static_rate = (input_classification != Actor_Classification::dynamic_rate);

	output.append(prefix + "async fn schedule(&mut self) {\n");

	if (!round_robin)
	{
		output.append(prefix + "\tloop {\n");
	}
	if (!fsm.empty())
	{
		std::string state_enum_compare;
		std::string state_enum_assign;

		state_enum_compare = "FSM::";
		state_enum_assign = "self.current_state = FSM::";
		std::set<std::string> states = Scheduling::get_all_states(fsm);

		output.append(prefix + "\tmatch self.current_state {\n");

		for (auto it = states.begin(); it != states.end(); ++it)
		{
			// string containing termination conditions
			std::string cond_receiver_map_str = "";

			output.append(prefix + "\t\t" + state_enum_compare + *it + " => {\n");

			// find actions that could be scheduled in this state
			std::vector<std::string> schedulable_actions = find_schedulable_actions(*it, fsm, actions);
			// sort the list of schedulable actions with the comparsion function defined above if a priority block is defined
			if (!priorities.empty())
			{
				std::sort(schedulable_actions.begin(), schedulable_actions.end(), Scheduling::comparison_object{priorities});
			}
			if (static_rate)
			{
				std::string channel_prefetch = state_channel_access[*it];
				replace_all_substrings(channel_prefetch, "\t", prefix + "\t\t\t");
				output.append(prefix + "\t\tif (" + action_schedulingCondition_map[schedulable_actions.front()] + ") {\n");
				output.append(channel_prefetch);
				local_prefix = prefix + "\t\t\t";
			}
			else
			{
				local_prefix = prefix + "\t\t\t";
			}
			std::vector<std::string> schedulable_actions_tmp = find_schedulable_actions("", fsm, actions);

			// create condition test and scheduling for each schedulable action
			for (auto action_it = schedulable_actions.begin();
				 action_it != schedulable_actions.end(); ++action_it)
			{

				std::string action_condition = action_guard[*action_it];
				if (!static_rate)
				{
					std::string tmp = action_schedulingCondition_map[*action_it];

					bool cond_non_true = action_condition != "true" && action_condition != "(true)";
					bool sched_non_true = tmp != "true" && tmp != "(true)";

					if (cond_non_true && sched_non_true)
					{
						tmp.append(" && ");
						tmp.append(action_condition);
						action_condition = tmp;
					}
					else if (sched_non_true)
					{
						action_condition = tmp;
					}
				}

				if (action_it == schedulable_actions.begin())
				{
					output.append(local_prefix + "if (" + action_condition + ") {\n");
				}
				else
				{
					output.append(local_prefix + "} else if (" + action_condition + ") {\n");
				}

				if (!action_freeSpaceCondition_map[*action_it].empty())
				{
					output.append(local_prefix + "\t" + action_freeSpaceCondition_map[*action_it] + "\n");
				}

				std::string pass_param;
				int match_counter = 0;

				if (action_param_read.find(*action_it) != action_param_read.end())
				{
					const auto &params = action_param_read[*action_it];

					for (const auto &p : params)
					{

						if (channel_read_map.find(*action_it) != channel_read_map.end() && channel_read_map[*action_it].size() >= match_counter + 1)
						{
							output.append(local_prefix + "match (" + channel_read_map[*action_it][match_counter] + ") {\n");
							local_prefix.append("\t");
							output.append(local_prefix + "(Some(" + p + ")) => {\n");
							local_prefix.append("\t");
						}
						else
						{
							output.append(local_prefix + "match (" + channel_read_map[*action_it][match_counter - 1] + ") {\n");
							local_prefix.append("\t");
							output.append(local_prefix + "(Some(" + p + ")) => {\n");
							local_prefix.append("\t");
						}
						pass_param += p;
						if (match_counter < params.size() - 1)
						{
							pass_param += ", ";
						}
						++match_counter;
						// we have reading channel
					}
				}
				if (action_freeSpaceCondition_map[*action_it].empty())
				{
					output.append(local_prefix + "\tself." + *action_it + "(" + pass_param + ").await; \n");
					output.append(local_prefix + "\t" + state_enum_assign + Scheduling::find_next_state(*it, *action_it, fsm) + ";\n");
					if (!action_post_exec_code_map[*action_it].empty())
					{
						std::string t = action_post_exec_code_map[*action_it];
						replace_all_substrings(t, "\t", local_prefix + "\t");
						output.append(t);
					}
				}
				else
				{
					output.append(local_prefix + "\t\tself." + *action_it + "(" + pass_param + ").await; \n");
					output.append(local_prefix + "\t\t" + state_enum_assign + Scheduling::find_next_state(*it, *action_it, fsm) + ";\n");
					if (!action_post_exec_code_map[*action_it].empty())
					{
						std::string t = action_post_exec_code_map[*action_it];
						replace_all_substrings(t, "\t", local_prefix + "\t\t");
						output.append(t);
					}
				}
				if (channel_read_map[*action_it] != std::vector<std::string>())
				{
					while (match_counter > 0)
					{
						output.append(local_prefix + "} // close Some\n");
						output.append(local_prefix + "_ => {\n");
						if (round_robin)
						{
							output.append(local_prefix + "\treturn;\n");
						}
						else
						{
							output.append(local_prefix + "\tbreak;\n");
						}
						output.append(local_prefix + "}\n");
						// output.append(prefix + "\t\t\t\t}\n");
						output.append(prefix + "\t\t\t} // close match\n");
						match_counter--;
					}
				}

				if (!action_freeSpaceCondition_map[*action_it].empty())
				{
					size_t tmp_count = 0;
					for (char c : action_freeSpaceCondition_map[*action_it])
					{
						if (c == '\n')
						{
							tmp_count++;
						}
					}

					for (size_t i = 0; i < tmp_count + 1; i++)
					{
						output.append(local_prefix + "\t\t}\n");
						// output.append(local_prefix + "\t\t}\n");
						output.append(local_prefix + "\t\telse {\n");
						if (!round_robin)
						{
							output.append(local_prefix + "\t\t\tbreak;\n");
						}
						else
						{
							output.append(local_prefix + "\t\t\treturn;\n");
						}
						output.append(local_prefix + "\t\t}\n");
					}
				}

				cond_receiver_map_str.append(cond_receiver_map[*action_it]);
				if (action_it == std::prev(schedulable_actions.end()))
				{
					if (cond_receiver_map[*action_it] != "")
					{
						output.append(local_prefix + "} else if " + cond_receiver_map_str + " {\n");
						output.append(local_prefix + "\t" + droping_channel_sender[*action_it] + "\n");
						output.append(local_prefix + "\tself.done_flag = true;\n");
						if (round_robin)
						{
							output.append(local_prefix + "\treturn;\n");
						}
						else
						{
							output.append(local_prefix + "\tbreak;\n");
						}
					}
					else if (cond_receiver_map_str != "")
					{
						output.append(local_prefix + "} else if " + cond_receiver_map_str + " {\n");
						output.append(local_prefix + "\t" + droping_channel_sender[*action_it] + "\n");
						output.append(local_prefix + "\tself.done_flag = true;\n");
						if (round_robin)
						{
							output.append(local_prefix + "\treturn;\n");
						}
						else
						{
							output.append(local_prefix + "\tbreak;\n");
						}
					}
					else
					{
						output.append("// cond_receiver_map is empty \n");
					}
				}
			}

			output.append(local_prefix + "} else { // before last else\n");
			if (round_robin)
			{
				output.append(local_prefix + "\treturn;\n");
			}
			else
			{
				output.append(local_prefix + "\tbreak;\n");
			}
			output.append(local_prefix + "}\n");
			if (static_rate)
			{

				output.append(prefix + "\t\t\t}\n");
			}
			output.append(prefix + "\t\t" + "} // close current state\n"); // close current state
		}
		output.append(prefix + "\t} // close state match\n"); // close state checking match
	}
	else
	{
		std::vector<std::string> schedulable_actions = find_schedulable_actions("", fsm, actions);
		if (!priorities.empty())
		{
			std::sort(schedulable_actions.begin(), schedulable_actions.end(), Scheduling::comparison_object{priorities});
		}
		local_prefix = prefix;
		if (static_rate)
		{

			std::string channel_prefetch = state_channel_access[""];
			replace_all_substrings(channel_prefetch, "\t", prefix + "\t\t");
			output.append(prefix + "\tif (" + action_schedulingCondition_map[schedulable_actions.front()] + ") {// first action condition \n");
			output.append(channel_prefetch);
			local_prefix = prefix + "\t\t";
		}
		else
		{
			local_prefix = prefix + "\t";
		}
		for (auto action_it = schedulable_actions.begin(); action_it != schedulable_actions.end(); ++action_it)
		{
			if (*action_it == "initialize")
			{
				continue;
			}
			std::string action_condition = action_guard[*action_it];
			if (!static_rate)
			{
				std::string tmp = action_schedulingCondition_map[*action_it];

				bool cond_non_true = action_condition != "true" && action_condition != "(true)";
				bool sched_non_true = tmp != "true" && tmp != "(true)";

				if (cond_non_true && sched_non_true)
				{
					tmp.append(" && ");
					tmp.append(action_condition);
					action_condition = tmp;
				}
				else if (sched_non_true)
				{
					action_condition = tmp;
				}
			}

			if (action_it == schedulable_actions.begin())
			{
				output.append(local_prefix + "if (" + action_condition + ") {\n");
			}
			else
			{
				output.append(local_prefix + "else if (" + action_condition + ") {\n");
			}

			if (!action_freeSpaceCondition_map[*action_it].empty())
			{
				output.append(local_prefix + "\t" + action_freeSpaceCondition_map[*action_it] + "\n");
			}

			std::string pass_param;
			int match_counter = 0;
			if (action_param_read.find(*action_it) != action_param_read.end())
			{
				const auto &params = action_param_read[*action_it];
				// size_t i = 0;
				for (const auto &p : params)
				{
					// we have reading channel
					if (channel_read_map.find(*action_it) != channel_read_map.end() && channel_read_map[*action_it].size() >= match_counter + 1)
					{
						output.append(local_prefix + "match (" + channel_read_map[*action_it][match_counter] + ") {\n");
						local_prefix.append("\t");
						output.append(local_prefix + "(Some(" + p + ")) => {\n");
						local_prefix.append("\t");
					}
					else
					{
						output.append(local_prefix + "match (" + channel_read_map[*action_it][match_counter - 1] + ") {\n");
						local_prefix.append("\t");
						output.append(local_prefix + "(Some(" + p + ")) => {\n");
						local_prefix.append("\t");
					}
					pass_param += p;
					if (match_counter < params.size() - 1)
					{
						pass_param += ", ";
					}
					++match_counter;
				}
			}

			if (action_freeSpaceCondition_map[*action_it].empty())
			{

				output.append(local_prefix + "\tself." + *action_it + "(" + pass_param + ").await; \n");

				if (!action_post_exec_code_map[*action_it].empty())
				{
					std::string t = action_post_exec_code_map[*action_it];
					replace_all_substrings(t, "\t", local_prefix + "\t");
					output.append(t);
				}
			}
			else
			{

				output.append(local_prefix + "\tself." + *action_it + "(" + pass_param + ").await; \n");

				if (!action_post_exec_code_map[*action_it].empty())
				{
					std::string t = action_post_exec_code_map[*action_it];
					replace_all_substrings(t, "\t", local_prefix + "\t\t");
					output.append(t);
				}
			}
			if (channel_read_map[*action_it] != std::vector<std::string>())
			{
				while (match_counter > 0)
				{
					output.append(local_prefix + "} // close Some\n");
					output.append(local_prefix + "_ => {\n");
					if (round_robin)
					{
						output.append(local_prefix + "\treturn;\n");
					}
					else
					{
						output.append(local_prefix + "\tbreak;\n");
					}
					output.append(local_prefix + "}\n");
					output.append(prefix + "\t\t\t} // close match\n");
					match_counter--;
				}
				if (!action_freeSpaceCondition_map[*action_it].empty())
				{
					size_t tmp_count = 0;
					for (char c : action_freeSpaceCondition_map[*action_it])
					{
						if (c == '\n')
						{
							tmp_count++;
						}
					}

					for (size_t i = 0; i < tmp_count + 1; i++)
					{
						output.append(local_prefix + "\t\t}\n");
						output.append(local_prefix + "\t\telse {\n");
						if (!round_robin)
						{
							output.append(local_prefix + "\t\t\tbreak;\n");
						}
						else
						{
							output.append(local_prefix + "\t\t\treturn;\n");
						}
						output.append(local_prefix + "\t\t}\n");
					}
				}
			}
			else
			{
				if (!action_freeSpaceCondition_map[*action_it].empty())
				{
					size_t tmp_count = 0;
					for (char c : action_freeSpaceCondition_map[*action_it])
					{
						if (c == '\n')
						{
							tmp_count++;
						}
					}

					for (size_t i = 0; i < tmp_count + 1; i++)
					{
						output.append(local_prefix + "\t\t}\n");
						output.append(local_prefix + "\t\telse {\n");
						if (!round_robin)
						{
							output.append(local_prefix + "\t\t\tbreak;\n");
						}
						else
						{
							output.append(local_prefix + "\t\t\treturn;\n");
						}
						output.append(local_prefix + "\t\t}\n");
					}
				}
				output.append(local_prefix + "} else { \n");
				output.append(local_prefix + "\t" + droping_channel_sender[*action_it] + "\n");
				output.append(local_prefix + "\tself.done_flag = true;\n");
				if (round_robin)
				{
					output.append(local_prefix + "\treturn;\n");
				}
				else
				{
					output.append(local_prefix + "\tbreak;\n");
				}
			}

			if (action_it == schedulable_actions.begin())
			{
				output.append(prefix + "\t\t\t} // close first if \n");
			}
			if (action_it == std::prev(schedulable_actions.end()))
			{

				if (cond_receiver_map[*action_it] != "")
				{
					output.append(local_prefix + "} else if " + cond_receiver_map[*action_it] + " {\n");
					output.append(local_prefix + "\t" + droping_channel_sender[*action_it] + "\n");
					output.append(local_prefix + "\tself.done_flag = true;\n");
					if (round_robin)
					{
						output.append(local_prefix + "\treturn;\n");
					}
					else
					{
						output.append(local_prefix + "\tbreak;\n");
					}

					// output.append(local_prefix + "}\n");
				}
			}
		}
		output.append(local_prefix + "} else { \n");

		if (round_robin)
		{
			output.append(local_prefix + "\treturn;\n");
		}
		else
		{
			output.append(local_prefix + "\tbreak;\n");
		}
		output.append(local_prefix + "}\n");
	}

	if (!round_robin)
	{
		output.append(prefix + "\t} // close loop\n"); // close loop
	}

	output.append(prefix + "} // close scheduler\n"); // close scheduler method

	return output;
}

static std::string default_local_2(
	std::map<std::string, std::vector<Scheduling::Channel_Schedule_Data>> &actions,
	std::vector<IR::FSM_Entry> &fsm,
	std::vector<IR::Priority_Entry> &priorities,
	Actor_Classification input_classification,
	Actor_Classification output_classification,
	std::string prefix,
	// maps method names to their scheduling condition: guards and size and free space in the FIFOs
	std::map<std::string, std::string> &action_guard,
	std::map<std::string, std::string> &action_schedulingCondition_map,
	std::map<std::string, std::vector<std::string>> param_read_decl_map,
	std::map<std::string, std::string> &action_freeSpaceCondition_map,
	std::map<std::string, std::string> &droping_channel_sender,
	std::map<std::string, std::string> &cond_receiver_map,
	std::map<std::string, std::set<std::string>> &action_param_read,
	std::map<std::string, std::string> &state_channel_access,
	std::map<std::string, std::string> &action_post_exec_code_map,
	std::string loop_exp,
	bool round_robin,
	std::string schedule_function_name,
	std::string schedule_function_parameter)
{
	Config *c = c->getInstance();
	std::string output{};
	std::string local_prefix;
	std::string buffered_prefix = prefix;
	bool static_rate = (input_classification != Actor_Classification::dynamic_rate);

	output.append(prefix + "fn schedule(&mut self) {\n");

	if (!round_robin)
	{
		output.append(prefix + "\tloop {\n");
	}
	if (!fsm.empty())
	{
		std::string state_enum_compare;
		std::string state_enum_assign;

		state_enum_compare = "FSM::";
		state_enum_assign = "self.current_state = FSM::";
		std::set<std::string> states = Scheduling::get_all_states(fsm);

		output.append(prefix + "\tmatch self.current_state {\n");

		for (auto it = states.begin(); it != states.end(); ++it)
		{
			output.append(prefix + "\t\t" + state_enum_compare + *it + " => {\n");

			// find actions that could be scheduled in this state
			std::vector<std::string> schedulable_actions = find_schedulable_actions(*it, fsm, actions);
			// sort the list of schedulable actions with the comparsion function defined above if a priority block is defined
			if (!priorities.empty())
			{
				std::sort(schedulable_actions.begin(), schedulable_actions.end(), Scheduling::comparison_object{priorities});
			}
			if (static_rate)
			{
				std::string channel_prefetch = state_channel_access[*it];
				replace_all_substrings(channel_prefetch, "\t", prefix + "\t\t\t");
				output.append(prefix + "\t\t\tif (" + action_schedulingCondition_map[schedulable_actions.front()] + ") {\n");
				output.append(channel_prefetch);
				local_prefix = prefix + "\t\t\t\t";
			}
			else
			{
				local_prefix = prefix + "\t\t\t\t";
			}
			// create condition test and scheduling for each schedulable action
			for (auto action_it = schedulable_actions.begin();
				 action_it != schedulable_actions.end(); ++action_it)
			{
				std::string action_condition = action_guard[*action_it];
				if (!static_rate)
				{
					std::string tmp = action_schedulingCondition_map[*action_it];

					bool cond_non_true = action_condition != "true" && action_condition != "(true)";
					bool sched_non_true = tmp != "true" && tmp != "(true)";

					if (cond_non_true && sched_non_true)
					{
						tmp.append(" && ");
						tmp.append(action_condition);
						action_condition = tmp;
					}
					else if (sched_non_true)
					{
						action_condition = tmp;
					}
				}
				if (action_it == schedulable_actions.begin())
				{
					output.append(local_prefix + "if (" + action_condition + ") {\n");
				}
				else
				{
					output.append(local_prefix + "else if (" + action_condition + ") {\n");
				}
				if (action_freeSpaceCondition_map[*action_it].empty())
				{
					output.append(local_prefix + "\tself." + *action_it + "(" +
								  get_action_in_parameters(*action_it, actions) + "); \n");

					output.append(local_prefix + "\t" + state_enum_assign + Scheduling::find_next_state(*it, *action_it, fsm) + ";\n");
					if (!action_post_exec_code_map[*action_it].empty())
					{
						std::string t = action_post_exec_code_map[*action_it];
						replace_all_substrings(t, "\t", local_prefix + "\t");
						output.append(t);
					}
				}
				else
				{
					output.append(local_prefix + "\tif (" + action_freeSpaceCondition_map[*action_it] + ") {\n");
					output.append(local_prefix + "\t\tself." + *action_it + "(" +
								  get_action_in_parameters(*action_it, actions) + "); \n");

					output.append(local_prefix + "\t\t" + state_enum_assign + Scheduling::find_next_state(*it, *action_it, fsm) + ";\n");
					if (!action_post_exec_code_map[*action_it].empty())
					{
						std::string t = action_post_exec_code_map[*action_it];
						replace_all_substrings(t, "\t", local_prefix + "\t\t");
						output.append(t);
					}
					output.append(local_prefix + "\t}\n");
					if (cond_receiver_map[*action_it] != "")
					{
						output.append(local_prefix + "\telse if " + cond_receiver_map[*action_it] + " {\n");
						output.append(local_prefix + "\t\t" + droping_channel_sender[*action_it] + "\n");
						output.append(local_prefix + "\t\tself.done_flag = true;\n");
						if (!round_robin)
						{
							output.append(local_prefix + "\t\tbreak;\n");
						}
						else
						{
							output.append(local_prefix + "\t\treturn;\n");
						}
						output.append(local_prefix + "\t}\n");
					}

					output.append(local_prefix + "\telse {\n");
					if (!round_robin)
					{
						output.append(local_prefix + "\t\tbreak;\n");
					}
					else
					{
						output.append(local_prefix + "\t\treturn;\n");
					}

					output.append(local_prefix + "\t}\n");
				}
				output.append(local_prefix + "}\n");
			}
			output.append(local_prefix + "else {\n");
			if (round_robin)
			{
				output.append(local_prefix + "\treturn;\n");
			}
			else
			{
				output.append(local_prefix + "\tbreak;\n");
			}
			output.append(local_prefix + "}\n");
			if (static_rate)
			{
				output.append(prefix + "\t\t\t} else { \n");
				if (!round_robin)
				{
					output.append(prefix + "\t\t\t\tbreak;\n");
				}
				else
				{
					output.append(prefix + "\t\t\t\treturn;\n");
				}
				output.append(prefix + "\t\t\t}\n");
			}
			output.append(prefix + "\t\t} // close state \n"); // close state
		}
		output.append(prefix + "\t} // close match\n"); // close match
	}
	else
	{
		std::vector<std::string> schedulable_actions = find_schedulable_actions("", fsm, actions);
		if (!priorities.empty())
		{
			std::sort(schedulable_actions.begin(), schedulable_actions.end(), Scheduling::comparison_object{priorities});
		}
		if (static_rate)
		{
			// We only need to check size once
			output.append(prefix + "\tif (" + action_schedulingCondition_map[schedulable_actions.front()] + ") {\n");
			std::string channel_prefetch = state_channel_access[""];
			replace_all_substrings(channel_prefetch, "\t", prefix + "\t\t");
			output.append(channel_prefetch);
			local_prefix = prefix + "\t\t";
		}
		else
		{
			local_prefix = prefix + "\t";
		}
		for (auto it = schedulable_actions.begin(); it != schedulable_actions.end(); ++it)
		{
			if (*it == "initialize")
			{
				continue;
			}
			std::string action_condition = action_guard[*it];
			if (!static_rate)
			{
				std::string tmp = action_schedulingCondition_map[*it];

				bool cond_non_true = (action_condition != "true") && (action_condition != "(true)");
				bool sched_non_true = (tmp != "true") && (tmp != "(true)");

				if (cond_non_true && sched_non_true)
				{
					tmp.append(" && ");
					tmp.append(action_condition);
					action_condition = tmp;
				}
				else if (sched_non_true)
				{
					action_condition = tmp;
				}
			}
			if (it == schedulable_actions.begin())
			{
				output.append(local_prefix + "if (" + action_condition + ") {\n");
			}
			else
			{
				output.append(local_prefix + "else if (" + action_condition + ") {\n");
			}

			if (action_freeSpaceCondition_map[*it].empty())
			{
				for (const auto &val : param_read_decl_map[*it])
				{
					output.append(local_prefix + "\t" + val);
				}
				output.append(local_prefix + "\tself." + *it + "(" + get_action_in_parameters(*it, actions) + "); \n");

				if (!action_post_exec_code_map[*it].empty())
				{
					std::string t = action_post_exec_code_map[*it];
					replace_all_substrings(t, "\t", local_prefix + "\t");
					output.append(t);
				}
			}
			else
			{
				output.append(local_prefix + "\tif (" + action_freeSpaceCondition_map[*it] + ") {\n");

				for (const auto &val : param_read_decl_map[*it])
				{
					output.append(local_prefix + "\t\t" + val);
				}

				output.append(local_prefix + "\t\tself." + *it + "(" + get_action_in_parameters(*it, actions) + "); \n");

				if (!action_post_exec_code_map[*it].empty())
				{
					std::string t = action_post_exec_code_map[*it];
					replace_all_substrings(t, "\t", local_prefix + "\t\t");
					output.append(t);
				}
				output.append(local_prefix + "\t}\n");
				output.append(local_prefix + "\telse {\n");
				if (!round_robin)
				{
					output.append(local_prefix + "\t\tbreak;\n");
				}
				else
				{
					output.append(local_prefix + "\t\treturn;\n");
				}

				output.append(local_prefix + "\t}\n");
			}

			output.append(local_prefix + "}\n");

			if (action_schedulingCondition_map[schedulable_actions.front()] == "true")
			{
				if (cond_receiver_map[*it] == "")
				{
					output.append(local_prefix + "else {\n");
				}
				else
				{
					output.append(local_prefix + "else if " + cond_receiver_map[*it] + "{\n");
				}

				output.append(local_prefix + "\t" + droping_channel_sender[*it] + "\n");
				output.append(local_prefix + "\tself.done_flag = true;\n");
				if (!round_robin)
				{
					output.append(local_prefix + "\tbreak;\n");
				}
				else
				{
					output.append(local_prefix + "\treturn;\n");
				}
				output.append(local_prefix + "}\n");
				if (cond_receiver_map[*it] != "")
				{
					output.append(prefix + "\telse {\n");
					if (!round_robin)
					{
						output.append(prefix + "\t\tbreak;\n");
					}
					else
					{
						output.append(prefix + "\t\treturn;\n");
					}

					output.append(prefix + "\t}\n");
				}
			}
			if (static_rate)
			{
				if (action_schedulingCondition_map[schedulable_actions.front()] != "true")
				{
					output.append(prefix + "\t}\n");
					if (cond_receiver_map[*it] == "")
					{
						output.append(prefix + "\telse {\n");
					}
					else
					{
						output.append(prefix + "\telse if " + cond_receiver_map[*it] + " {\n");
					}

					output.append(prefix + "\t\t" + droping_channel_sender[*it] + "\n");
					output.append(prefix + "\t\tself.done_flag = true;\n");
					if (!round_robin)
					{
						output.append(local_prefix + "\t\tbreak;\n");
					}
					else
					{
						output.append(local_prefix + "\t\treturn;\n");
					}

					output.append(prefix + "\t}\n");
					if (cond_receiver_map[*it] != "")
					{
						output.append(prefix + "\telse {\n");
						if (!round_robin)
						{
							output.append(prefix + "\t\tbreak;\n");
						}
						else
						{
							output.append(prefix + "\t\treturn;\n");
						}

						output.append(prefix + "\t}\n");
					}
				}
				else
				{
					output.append(prefix + "\t}\n");
				}
			}
		}
	}
	if (!round_robin)
	{
		output.append(prefix + "\t}\n"); // close main loop
	}
	output.append(prefix + "}\n"); // close scheduler method

	return output;
}

/* Replace channel variables used in the guards by either fetched local variables
 * or prefetch function calls.
 */
static std::string guard_var_replacement(
	std::string guard,
	std::map<std::string, std::string> &replacement_map)
{
	remove_whitespaces(guard);

	for (auto it = replacement_map.begin(); it != replacement_map.end(); ++it)
	{
		replace_variables(guard, it->first, it->second);
	}

	return guard;
}

/* Update the guard conditions of the actions and generate code to fetch tokens from channels.
 * In the static cases (also cyclo-static etc.) code to fetch the tokens and store it in local
 * variables is generated, the guard conditions are manipulated to use these local variables.
 * Otherwise the guard conditions are manipulated to use the prefetch functionality of the channel
 * to compute the guard condition.
 * The guards are adjusted in action_guard and the code to fetch the tokens is returned as
 * map, mapping the state to the fetch code. If no FSM is defined the code has the empty string as key.
 */
static std::map<std::string, std::string> get_scheduler_channel_access(
	std::map<std::string, std::vector<Scheduling::Channel_Schedule_Data>> &actions,
	std::map<std::string, std::string> &action_guard,
	Actor_Classification input_classification,
	std::map<std::string, std::vector<std::string>> &actions_per_state,
	Actor_Conversion_Data &conversion_data)
{
	// Map the channel read to local variable for each state if required (not in the dynamic case)
	std::map<std::string, std::string> output;
	Config *c = c->getInstance();

	/* Only use this path for now, the other path has a bug, as it should fetch the
	 * the tokens only if sufficient output channel space is available.
	 * Otherwise there must be some local buffering for the next call or some revert operation
	 * on the channel to avoid losing the tokens.
	 */
	if (true || input_classification == Actor_Classification::dynamic_rate)
	{
		// It is dynamic, hence, we must prefetch
		for (auto action_it = actions.begin();
			 action_it != actions.end(); ++action_it)
		{
			std::map<std::string, std::string> replacement_map;

			for (auto sched_data_it = action_it->second.begin();
				 sched_data_it != action_it->second.end(); ++sched_data_it)
			{
				// One scheduling data entry per accessed channel
				unsigned index = 0;
				if (action_guard.contains(action_it->first) && (action_guard[action_it->first] != ""))
				{
					for (auto var_it = sched_data_it->var_names.begin();
						 var_it != sched_data_it->var_names.end(); ++var_it)
					{
						if (sched_data_it->unused_channel)
						{
							if (sched_data_it->repeat)
							{
								for (unsigned i = 0; i < sched_data_it->elements; ++i)
								{
									std::string r = *var_it + "[" + std::to_string(i) + "]";
									replacement_map[r] = "0";
								}
							}
							else
							{
								std::string r = *var_it;
								// Just use a dummy value if we cannot access this channel!
								replacement_map[r] = "0";
							}
						}
						else if (sched_data_it->repeat)
						{
							size_t repeat_val = sched_data_it->elements / sched_data_it->var_names.size();
							for (size_t i = 0; i < repeat_val; ++i)
							{
								std::string r = *var_it + "[" + std::to_string(i) + "]";
								std::string tmp;
								std::string offset = std::to_string(i * sched_data_it->var_names.size() + index);
								ABI_CHANNEL_PREFETCH(c, tmp, sched_data_it->channel_name, offset)
								std::string n = tmp;
								replacement_map[r] = n;
							}
						}
						else
						{
							std::string r = *var_it;
							std::string tmp;
							std::string offset = std::to_string(index);
							ABI_CHANNEL_PREFETCH(c, tmp, sched_data_it->channel_name, offset)
							std::string n = tmp;
							replacement_map[r] = n;
						}
						++index;
					}
				}
			}

			std::string replaced_guard = guard_var_replacement(action_guard[action_it->first], replacement_map);
#ifdef DEBUG_SCHEDULER_GENERATION
			if ((replaced_guard != action_guard[action_it->first]) && !replaced_guard.empty())
			{
				std::cout << "Action: " << action_it->first << " Replace guard " << action_guard[action_it->first] << " by " << replaced_guard << std::endl;
			}
#endif
			action_guard[action_it->first] = replaced_guard;
		}
	}
	else
	{
		// Must be some classification that demands that all actions consume the same number of tokens
		// Hence, we can load the channel data to a local variable, evaluate guards and the forward the
		// local variable to the action
		if (!actions_per_state.empty())
		{
			for (auto state_it = actions_per_state.begin(); state_it != actions_per_state.end(); ++state_it)
			{
				for (auto action_it = state_it->second.begin(); action_it != state_it->second.end(); ++action_it)
				{
					std::map<std::string, std::string> replacement_map;

					for (auto sched_data_it = actions[*action_it].begin();
						 sched_data_it != actions[*action_it].end(); ++sched_data_it)
					{
						// One scheduling data entry per accessed channel
						unsigned index = 0;
						if (action_guard.contains(*action_it) && (action_guard[*action_it] != ""))
						{
							for (auto var_it = sched_data_it->var_names.begin();
								 var_it != sched_data_it->var_names.end(); ++var_it)
							{
								if (sched_data_it->unused_channel)
								{
									if (sched_data_it->repeat)
									{
										for (unsigned i = 0; i < sched_data_it->elements; ++i)
										{
											std::string r = *var_it + "[" + std::to_string(i) + "]";
											replacement_map[r] = "0";
										}
									}
									else
									{
										std::string r = *var_it;
										// Just use a dummy value if we cannot access this channel!
										replacement_map[r] = "0";
									}
								}
								else if (sched_data_it->repeat)
								{
									size_t repeat_val = sched_data_it->elements / sched_data_it->var_names.size();
									for (size_t i = 0; i < repeat_val; ++i)
									{
										std::string r = *var_it + "[" + std::to_string(i) + "]";
										std::string n = sched_data_it->channel_name + "_param[" + std::to_string(i * sched_data_it->var_names.size() + index) + "]";
										replacement_map[r] = n;
									}
								}
								else
								{
									if (sched_data_it->elements == 1)
									{
										std::string r = *var_it;
										std::string n = sched_data_it->channel_name + "_param";
										replacement_map[r] = n;
									}
									else
									{
										std::string r = *var_it;
										std::string n = sched_data_it->channel_name + "_param[" + std::to_string(index) + "]";
										replacement_map[r] = n;
									}
								}
								++index;
							}
						}
					}
					std::string replaced_guard = guard_var_replacement(action_guard[*action_it], replacement_map);
#ifdef DEBUG_SCHEDULER_GENERATION
					if ((replaced_guard != action_guard[*action_it]) && !replaced_guard.empty())
					{
						std::cout << "Action: " << *action_it << " Replace guard " << action_guard[*action_it] << " by " << replaced_guard << std::endl;
					}
#endif
					action_guard[*action_it] = replaced_guard;
				}

				std::string local_def;
				for (auto it = actions[state_it->second.front()].begin(); it != actions[state_it->second.front()].end(); ++it)
				{
					std::string tmp;
					ABI_CHANNEL_READ(c, tmp, it->channel_name)
					if (it->unused_channel || !it->in)
					{
						continue;
					}
					if (it->elements == 1)
					{
						local_def.append("\t" + it->type + " " + it->channel_name + "_param = " + tmp + ";\n");
					}
					else
					{
						local_def.append("\t" + it->type + " " + it->channel_name + "_param[" + std::to_string(it->elements) + "];\n");
						local_def.append("\tfor (unsigned i = 0; i < " + std::to_string(it->elements) + "; ++i) {" + it->channel_name + "_param[i] = " + tmp + ";}\n");
					}
				}
#ifdef DEBUG_SCHEDULER_GENERATION
				std::cout << "Channel prefetch code for state " << state_it->first << ":" << local_def << std::endl;
#endif
				output[state_it->first] = local_def;
			}
		}
		else
		{
			// No FSM, hence, it is the static case
			for (auto action_it = actions.begin();
				 action_it != actions.end(); ++action_it)
			{
				std::map<std::string, std::string> replacement_map;

				for (auto sched_data_it = action_it->second.begin();
					 sched_data_it != action_it->second.end(); ++sched_data_it)
				{
					// One scheduling data entry per accessed channel
					unsigned index = 0;
					if (action_guard.contains(action_it->first) && (action_guard[action_it->first] != ""))
					{
						for (auto var_it = sched_data_it->var_names.begin();
							 var_it != sched_data_it->var_names.end(); ++var_it)
						{
							if (sched_data_it->unused_channel)
							{
								if (sched_data_it->repeat)
								{
									for (unsigned i = 0; i < sched_data_it->elements; ++i)
									{
										std::string r = *var_it + "[" + std::to_string(i) + "]";
										replacement_map[r] = "0";
									}
								}
								else
								{
									std::string r = *var_it;
									// Just use a dummy value if we cannot access this channel!
									replacement_map[r] = "0";
								}
							}
							else if (sched_data_it->repeat)
							{
								size_t repeat_val = sched_data_it->elements / sched_data_it->var_names.size();
								for (size_t i = 0; i < repeat_val; ++i)
								{
									std::string r = *var_it + "[" + std::to_string(i) + "]";
									std::string n = sched_data_it->channel_name + "_param[" + std::to_string(i * sched_data_it->var_names.size() + index) + "]";
									replacement_map[r] = n;
								}
							}
							else
							{
								if (sched_data_it->elements == 1)
								{
									std::string r = *var_it;
									std::string n = sched_data_it->channel_name + "_param";
									replacement_map[r] = n;
								}
								else
								{
									std::string r = *var_it;
									std::string n = sched_data_it->channel_name + "_param[" + std::to_string(index) + "]";
									replacement_map[r] = n;
								}
							}
							++index;
						}
					}
				}
				std::string replaced_guard = guard_var_replacement(action_guard[action_it->first], replacement_map);
#ifdef DEBUG_SCHEDULER_GENERATION
				if (!replaced_guard.empty() && (action_guard[action_it->first] != replaced_guard))
				{
					std::cout << "Action: " << action_it->first << " Replace guard " << action_guard[action_it->first] << " by " << replaced_guard << std::endl;
				}
#endif
				action_guard[action_it->first] = replaced_guard;
			}
			std::string local_def;
			for (auto it = actions.begin()->second.begin(); it != actions.begin()->second.end(); ++it)
			{
				if (it->unused_channel || !it->in)
				{
					continue;
				}
				if (it->elements == 1)
				{
					std::string tmp;
					ABI_CHANNEL_READ(c, tmp, it->channel_name)
					local_def.append("\t" + it->type + " " + it->channel_name + "_param = " + tmp + ";\n");
				}
				else
				{
					std::string tmp;
					ABI_CHANNEL_READ(c, tmp, it->channel_name)
					local_def.append("\t" + it->type + " " + it->channel_name + "_param[" + std::to_string(it->elements) + "];\n");
					local_def.append("\tfor (unsigned i = 0; i < " + std::to_string(it->elements) + "; ++i) {" + it->channel_name + "_param[i] = " + tmp + ";}\n");
				}
			}
#ifdef DEBUG_SCHEDULER_GENERATION
			std::cout << "Channel prefetch code:" << local_def << std::endl;
#endif
			output[""] = local_def;
		}
	}

	return output;
}

std::string Scheduling::generate_local_scheduler_rust(
	Actor_Conversion_Data &conversion_data,
	std::map<std::string, std::string> &action_guard,
	std::vector<IR::FSM_Entry> &fsm,
	std::vector<IR::Priority_Entry> &priorities,
	Actor_Classification input_classification,
	Actor_Classification output_classification,
	std::string prefix,
	std::string schedule_function_name,
	std::string schedule_function_parameter,
	unsigned scheduling_loop_bound,
	std::set<std::string> &actor_var_map,
	std::map<std::string, std::set<std::string>> &action_param_read)
{
	Config *c = c->getInstance();
	std::map<std::string, std::vector<Channel_Schedule_Data>> actions = conversion_data.get_scheduling_data();

#ifdef DEBUG_SCHEDULER_GENERATION
	for (auto it = actions.begin(); it != actions.end(); ++it)
	{
		std::cout << "Action " << it->first << " scheduling data:" << std::endl;

		for (auto d = it->second.begin(); d != it->second.end(); ++d)
		{
			std::cout << "Channel name: " << d->channel_name << " Type: " << d->type
					  << " Num: " << d->elements << " Variables:";
			for (auto var_it = d->var_names.begin(); var_it != d->var_names.end(); ++var_it)
			{
				std::cout << " " << *var_it;
			}
			if (d->in)
			{
				std::cout << "; input." << std::endl;
			}
			else
			{
				std::cout << "; output." << std::endl;
			}
		}
	}
#endif

	std::map<std::string, std::string> action_schedulingCondition_map;
	std::map<std::string, std::vector<std::string>> channel_read_map;
	std::map<std::string, std::vector<std::string>> param_read_decl_map;
	// std::map<std::string, std::string> param_read_decl_map;
	std::map<std::string, std::string> action_freeSpaceCondition_map;
	std::map<std::string, std::string> cond_receiver_map;
	std::map<std::string, std::string> droping_channel_sender;
	std::map<std::string, std::string> action_post_exec_map;

	std::map<std::string, std::vector<std::string>> actions_per_state;
	std::string sched_loop;

	for (auto it : get_all_states(fsm))
	{
		std::vector<std::string> sched_actions = find_schedulable_actions(it, fsm, actions);
		actions_per_state[it] = sched_actions;
	}

	std::map<std::string, std::string> state_channel_access =
		get_scheduler_channel_access(actions, action_guard, input_classification, actions_per_state, conversion_data);

	// Set guard to (true) if no guard is specified as this avoids checking during code generation
	// whether a guard is used or not. The C-Compiler can optimize this.
	for (auto it = action_guard.begin(); it != action_guard.end(); ++it)
	{
		if (it->second.empty())
		{
			it->second = "(true)";
		}
		else
		{
			it->second = "(" + it->second + ")";
		}
	}

	// init with empty first so we don't need to care later and keep the guarantee that every action is in there
	for (auto it = actions.begin(); it != actions.end(); ++it)
	{
		action_schedulingCondition_map[it->first] = "";
		channel_read_map[it->first] = {};
		param_read_decl_map[it->first] = {};
		action_freeSpaceCondition_map[it->first] = "";
		droping_channel_sender[it->first] = "";
		cond_receiver_map[it->first] = "";
		action_post_exec_map[it->first] = "";
	}

	/* Determine input channel size checks for each action and output channel free space checks.
	 * They are stored in different maps as insufficient output channel space shall not influence
	 * the scheduling and is therefore treated differently.
	 */
	for (auto action_it = actions.begin(); action_it != actions.end(); ++action_it)
	{
		std::string param_str = get_action_in_parameters(action_it->first, actions);

		std::vector<std::string> param_map;
		std::stringstream ss(param_str);
		std::string param_item;

		while (std::getline(ss, param_item, ','))
		{
			// Trim leading/trailing spaces
			param_item.erase(0, param_item.find_first_not_of(" \t"));
			param_item.erase(param_item.find_last_not_of(" \t") + 1);
			param_map.push_back(param_item);
		}

		for (auto sched_data_it = action_it->second.begin();
			 sched_data_it != action_it->second.end();
			 ++sched_data_it)
		{
			if (sched_data_it->unused_channel)
			{
				continue;
			}
			if (sched_data_it->in)
			{
				if (!action_schedulingCondition_map[action_it->first].empty())
				{
					action_schedulingCondition_map[action_it->first].append(" && ");
				}

				if (!cond_receiver_map[action_it->first].empty())
				{
					cond_receiver_map[action_it->first].append(" && ");
				}

				std::string tmp;
				ABI_CHANNEL_SIZE(c, tmp, sched_data_it->channel_name)

				action_schedulingCondition_map[action_it->first].append("(" + tmp + " >= " + std::to_string(sched_data_it->elements) + ")");

				std::string tmp2;
				ABI_CHANNEL_READ(c, tmp2, sched_data_it->channel_name)
				channel_read_map[action_it->first].push_back(tmp2);

				std::string ch_tmp = (sched_data_it->channel_name == "in") ? "input" : sched_data_it->channel_name;
				cond_receiver_map[action_it->first].append("self." + ch_tmp + ".is_terminated() && self." + ch_tmp + ".is_empty()");

				// doesnt match well when we have 2 or more channel reads
				for (const auto &s : param_map)
				{
					param_read_decl_map[action_it->first].push_back("let " + s + " = " + tmp2 + ";\n");
				}

				// }
			}
			else
			{
				if (!action_freeSpaceCondition_map[action_it->first].empty())
				{
					// normaly we should just have one channel sender but incase
					if (c->get_target_language() == Target_Language::rust1)
					{
						action_freeSpaceCondition_map[action_it->first].append("\n");
					}
					else
					{
						action_freeSpaceCondition_map[action_it->first].append(" && ");
					}

					droping_channel_sender[action_it->first].append("\n");
				}

				std::string tmp;
				ABI_CHANNEL_FREE(c, tmp, sched_data_it->channel_name)

				// action_freeSpaceCondition_map[action_it->first].append("self." + sched_data_it->channel_name + " = None;");
				if (c->get_target_language() == Target_Language::rust1)
				{
					action_freeSpaceCondition_map[action_it->first].append(tmp + " >= " + std::to_string(sched_data_it->elements) + " {");
					droping_channel_sender[action_it->first].append("self." + sched_data_it->channel_name + " = None;");
				}
				else
				{
					action_freeSpaceCondition_map[action_it->first].append("(" + tmp + " >= " + std::to_string(sched_data_it->elements) + ")");
					droping_channel_sender[action_it->first].append("self." + sched_data_it->channel_name + ".terminate();");
				}

				// }
			}
		}
		if (action_schedulingCondition_map[action_it->first].empty())
		{
			action_schedulingCondition_map[action_it->first] = "true";
		}
	}

	if (c->get_sched_non_preemptive() || c->get_sched_rr())
	{
		if (c->get_target_language() == Target_Language::rust1)
		{
			return default_local(actions, fsm, priorities,
								 input_classification, output_classification, prefix,
								 action_guard,
								 action_schedulingCondition_map,
								 channel_read_map,
								 action_freeSpaceCondition_map,
								 droping_channel_sender,
								 cond_receiver_map,
								 action_param_read,
								 state_channel_access,
								 action_post_exec_map,
								 sched_loop,
								 c->get_sched_rr(),
								 schedule_function_name,
								 schedule_function_parameter);
		}
		else
		{
			return default_local_2(actions, fsm, priorities,
								   input_classification, output_classification, prefix,
								   action_guard,
								   action_schedulingCondition_map,
								   param_read_decl_map,
								   action_freeSpaceCondition_map,
								   droping_channel_sender,
								   cond_receiver_map,
								   action_param_read,
								   state_channel_access,
								   action_post_exec_map,
								   sched_loop,
								   c->get_sched_rr(),
								   schedule_function_name,
								   schedule_function_parameter);
		}
	}
	else
	{
		throw Scheduler_Generation_Exception{"No Scheduling Strategy defined."};
	}
}