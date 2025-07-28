#include "Generate_Rust.hpp"
#include "Config/config.h"
#include "Config/debug.h"
#include <fstream>
#include "Scheduling.hpp"
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <cctype>
#include "Converter_RVC_Rust.hpp"
#include "Action_Conversion_Rust.hpp"
#include "String_Helper.h"
#include <filesystem>
#include "ABI/abi.hpp"

static std::pair<std::string, std::string> class_variable_generation(
	IR::Actor_Instance* actor,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::map<std::string, std::string>& constructor_parameter_name_type_map,
	std::set<std::string>& unused_in_channels,
	std::set<std::string>& unused_out_channels,
	std::set<std::string>& used_in_channels,
	std::set<std::string>& used_out_channels,
	std::set<std::string>& actor_var_map,
	std::vector<IR::FSM_Entry>& fsm,
	Actor_Conversion_Data& data,
	std::string prefix)
{
	std::string ret;
	std::string const_ret;
	Config* c = c->getInstance();

	for (auto it = actor->get_actor()->get_var_buffers().begin();
		it != actor->get_actor()->get_var_buffers().end(); ++it)
	{
		it->reset_buffer();
		Token t = it->get_next_token();
		std::string symbol_name;
		bool const_def = false;
		std::string tmp = Converter_RVC_Rust::convert_expression_act_var(t, *it, data.get_symbol_map(), data.get_symbol_type_map(), symbol_name, prefix, false);

		actor_var_map.insert(symbol_name);

		if (tmp.find_first_of(";") != tmp.find_last_of(";"))
		{
			// There is more than one ; in the result. This is some comprehension. Add it to the constructor.
			if (tmp.find("static") != tmp.npos)
			{
				// It is not really const, but is static because we only tag it as not const
				// due to the initialization that has to be done by the init, otherwise it is const.
				// Hence, it is static, so no need to add it to actor type!
				const_ret.append(tmp.substr(0, tmp.find_first_of(";")));
				const_def = true;
			}
			else
			{
				ret += tmp.substr(0, tmp.find_first_of(";"));
			}
			tmp = tmp.substr(tmp.find_first_of(";") + 1);
			replace_all_substrings(tmp, "\t", "\t\t");
			data.add_constructor_code(tmp);
		}
		else
		{
			if (tmp.find("const") != tmp.npos)
			{
				const_ret.append(tmp);
				const_def = true;
			}
			else
			{ // this is a temporal solution
				ret.append(tmp);
				replace_all_substrings(tmp, "\t", "");
				// const_ret.append(tmp);
			}
		}
		data.add_class_variable(symbol_name);
	}

	// Parameters
	std::string parameters;
	for (auto it = actor->get_actor()->get_param_buffers().begin();
		it != actor->get_actor()->get_param_buffers().end(); ++it)
	{
		it->reset_buffer();
		Token t = it->get_next_token();
		parameters += Converter_RVC_Rust::convert_actor_parameters(t, *it, actor->get_conversion_data().get_symbol_map(),
			constructor_parameter_name_type_map, data.get_default_parameter_map(), actor_var_map, prefix);
	}
	ret.append(prefix + "// Actor Parameters\n");
	ret.append(parameters);

	ret.append(prefix + "actor_name: String,\n");
	ret.append(prefix + "done: bool,\n");
	if (!fsm.empty())
	{
		ret.append(prefix + "// FSM\n");
		ret.append(prefix + "current_state: FSM,\n");
	}

	if (!actor->get_actor()->get_in_buffers().empty())
	{
		ret.append(prefix + "// Input Channels\n");
		// Channels
		for (auto it = actor->get_actor()->get_in_buffers().begin();
			it != actor->get_actor()->get_in_buffers().end(); ++it)
		{
			if (unused_in_channels.find(it->buffer_name) != unused_in_channels.end())
			{
				// it is not used, skip
				continue;
			}
			used_in_channels.insert(it->buffer_name);
			Tokenizer tokenizer(it->type); // string like "int(size=32)"
			Token type_token = tokenizer.get_next_token();

			std::string rust_type = Converter_RVC_Rust::convert_type(type_token, tokenizer, data.get_symbol_map(), data.get_symbol_map());

			std::pair<std::string, std::string> decl;
			ABI_CHANNEL_DECL_REC(c, decl, it->buffer_name, "0", rust_type, false, prefix);
			std::string type = decl.second;
			constructor_parameter_name_type_map[it->buffer_name] = type;
			ret.append(decl.first);
		}
	}

	if (!actor->get_actor()->get_out_buffers().empty())
	{
		ret.append(prefix + "// Output Channels\n");
		for (auto it = actor->get_actor()->get_out_buffers().begin();
			it != actor->get_actor()->get_out_buffers().end(); ++it)
		{
			if (unused_out_channels.find(it->buffer_name) != unused_out_channels.end())
			{
				// it is not used, skip
				continue;
			}
			used_out_channels.insert(it->buffer_name);
			Tokenizer tokenizer(it->type); // string like "int(size=32)"
			Token type_token = tokenizer.get_next_token();

			std::string rust_type = Converter_RVC_Rust::convert_type(type_token, tokenizer, data.get_symbol_map(), data.get_symbol_map());

			std::pair<std::string, std::string> decl;
			ABI_CHANNEL_DECL_SEN(c, decl, it->buffer_name, "0", rust_type, false, prefix);
			std::string type = decl.second;
			constructor_parameter_name_type_map[it->buffer_name] = type;
			ret.append(decl.first);
		}
	}

	ret.append("\n");

	return std::make_pair(ret, const_ret);
}

static std::string constructor_generation(
	IR::Actor* actor,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::map<std::string, std::string>& constructor_parameter_name_type_map,
	std::set<std::string>& used_in_channels,
	std::set<std::string>& used_out_channels,
	std::string class_name,
	Actor_Conversion_Data& data)
{
	std::string ret;
	std::string body;

	// ret = "\t" + class_name + "(std::string _n";
	ret = "\tpub fn new(name: &str";

	for (auto it = constructor_parameter_name_type_map.begin();
		it != constructor_parameter_name_type_map.end(); ++it)
	{
		ret.append(", ");
		// problem: the keyword 'in' in rust cant be used as a name
		// temporar solution until better alternative is found
		std::string tmp_name = (it->first == "in") ? "input" : it->first;
		// Type + _ + variable
		// ret.append(it->second + " _" + it->first);
		ret.append(tmp_name + ": " + it->second);

		// we need to know if the channel is a sender or receiver

		std::string init_line;
		if (used_in_channels.find(it->first) != used_in_channels.end())
		{
			// its an input (Receiver)
			init_line = tmp_name;
		}
		else if (used_out_channels.find(it->first) != used_out_channels.end())
		{
			// its an output (Sender)
			init_line = "Some(" + tmp_name + ")";
		}
		else
		{
			// fallback/default
			init_line = tmp_name;
		}
		body.append("\t\t\t" + tmp_name + ": " + init_line + ",\n");
		data.add_parameter(it->first);
	}
	ret.append(") -> Self {\n");
	ret.append("\t\t" + class_name + "{\n");
	for (auto it = actor->get_var_buffers().begin();
		it != actor->get_var_buffers().end(); ++it)
	{
		it->reset_buffer();
		Token t = it->get_next_token();
		std::string symbol_name;
		bool const_def = false;
		// std::cout << "first token " << t.str << std::endl;
		std::string tmp = Converter_RVC_Rust::convert_expression_act_var(t, *it, data.get_symbol_map(), data.get_symbol_type_map(), symbol_name, "\t\t\t", true);
		// replace_all_substrings(tmp, "\t", "");
		ret.append(tmp);
	}
	ret.append("\t\t\tactor_name: name.to_string(),\n");
	ret.append("\t\t\tdone: false,\n");
	if (!actor->get_fsm().empty())
	{
		ret.append("\t\t\tcurrent_state: FSM::" + actor->get_initial_state() + ",\n");
	}
	ret.append(body);
	ret.append(data.get_constructor_code());
	ret.append("\t\t}\n");
	ret.append("\t}\n");

	return ret;
}

static std::string action_generation(
	IR::Actor* actor,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::set<std::string>& unused_actions,
	std::set<std::string>& unused_in_channels,
	std::set<std::string>& unused_out_channels,
	std::set<std::string>& actor_var_map,
	Actor_Conversion_Data& data,
	std::string prefix)
{
	std::string ret;

	for (auto it = actor->get_actions().begin();
		it != actor->get_actions().end(); ++it)
	{
		// The init action is converted later and added to the public part of the class
		if ((*it)->is_init())
		{
			continue;
		}
		if ((*it)->is_deleted())
		{
			continue;
		}
		if (unused_actions.find((*it)->get_name()) != unused_actions.end())
		{
			// action is not used, don't generate
			continue;
		}
		(*it)->get_action_buffer()->reset_buffer();
		/* Cannot use input_channel_parameters for the action because the scheduler cannot handle this
		 * right now properly. It will prefetch the tokens before checking whether sufficient output
		 * channel space is available. If this is not the case the tokens are lost.
		 * Hence, this might only work for SISO actors. Deactivate for now.
		 */

		 // ret += convert_action_rust(*it, (*it)->get_action_buffer(), data,
		 // 						   false, false,
		 // 						   unused_in_channels, unused_out_channels, prefix);
		ret += convert_action_rust(*it, (*it)->get_action_buffer(), data,
			true, false,
			unused_in_channels, unused_out_channels, actor_var_map, prefix);
	}

	return ret;
}

static std::string function_generation(
	IR::Actor* actor,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::set<std::string>& actor_var_map,
	Actor_Conversion_Data& data,
	std::string prefix)
{
	std::string ret;

	for (auto it = actor->get_method_buffers().begin();
		it != actor->get_method_buffers().end(); ++it)
	{
		std::map<std::string, std::string> local_type_map{ data.get_symbol_type_map() };
		it->reset_buffer();
		Token t = it->get_next_token();
		if (t.str == "function")
		{
			ret += Converter_RVC_Rust::convert_function(t, *it, data.get_symbol_map(), local_type_map, actor_var_map, prefix);
		}
		if (t.str == "procedure")
		{
			ret += Converter_RVC_Rust::convert_procedure(t, *it, data.get_symbol_map(), local_type_map, actor_var_map, prefix);
		}
	}

	return ret;
}

static std::string convert_natives(IR::Actor* actor)
{
	std::string ret;

	for (auto it = actor->get_native_buffers().begin();
		it != actor->get_native_buffers().end(); ++it)
	{
		it->reset_buffer();
		Token t = it->get_next_token();
		ret += Converter_RVC_Rust::convert_native_declaration(t, (*it), "*",
			actor->get_conversion_data(), actor->get_conversion_data().get_symbol_map());
	}

	return ret;
}

static std::string init_action_generation(
	IR::Actor* actor,
	Actor_Conversion_Data& data,
	std::set<std::string>& actor_var_map,
	std::string prefix)
{
	std::string ret;
	Config* c = c->getInstance();

	for (auto it = actor->get_actions().begin();
		it != actor->get_actions().end(); ++it)
	{
		if ((*it)->is_init())
		{
			(*it)->get_action_buffer()->reset_buffer();
			ret += convert_action_rust(*it, (*it)->get_action_buffer(), data,
				true, false, std::set<std::string>(), std::set<std::string>(), actor_var_map, prefix);
		}
	}

	if (ret.empty())
	{
		ret.append(prefix + "fn initialize(&mut self) {\n");
		ret.append(prefix + "\tprintln!(\"Initializing {}\", self.actor_name);\n");
		ret.append(prefix + "}\n");
	}

	return ret;
}

// TODO: Maybe some states are not required if actions are removed
static std::string generate_FSM(
	IR::Actor* actor,
	std::set<std::string>& unused_actions,
	std::string class_name,
	std::string prefix)
{
	Config* c = c->getInstance();

	if (actor->get_fsm().empty())
	{
		return std::string();
	}

	std::set<std::string> states;
	for (auto it = actor->get_fsm().begin();
		it != actor->get_fsm().end(); ++it)
	{
		states.insert(it->state);
	}

	std::string ret;
	ret.append("// FSM\n");
	// if (c->get_target_language() == Target_Language::c)
	// {
	// 	ret.append(prefix + "typedef enum " + class_name + "_fsm {\n");
	// }
	// else if (c->get_target_language() == Target_Language::cpp)
	// {
	// 	ret.append(prefix + "enum class FSM {\n");
	// }
	// enum should be outside struct scope
	ret.append("pub enum FSM {\n");

	for (auto it = states.begin(); it != states.end(); ++it)
	{
		ret.append("\t" + *it + ",\n");
	}

	ret.append("} \n");
	// if (c->get_target_language() == Target_Language::c)
	// {
	// 	ret.append(prefix + "} " + class_name + "_fsm_t;\n");
	// }
	// else if (c->get_target_language() == Target_Language::cpp)
	// {
	// 	ret.append(prefix + "};\n");
	// 	ret.append(prefix + "FSM state = FSM::" + actor->get_initial_state() + ";\n\n");
	// }
	return ret;
}

static void convert_import(
	IR::Actor_Instance* inst)
{
	Actor_Conversion_Data& d = inst->get_conversion_data();
	std::set<std::string> seen_vars;
	std::set<std::string> converted_actors; // actors with already converted imports
	std::set<std::string> empty_map;

	for (auto it = inst->get_actor()->get_imported_symbols().begin();
		it != inst->get_actor()->get_imported_symbols().end(); ++it)
	{
		// checks if imports already converted for this actor
		if (converted_actors.contains(inst->get_actor()->get_conversion_data().get_class_name()))
		{
			break;
		}

		converted_actors.insert(inst->get_actor()->get_conversion_data().get_class_name());

		bool found_symbol{ false };
		std::string code, declarations;
		// code.append("\t// Import parameters\n");
		if (it->first == "*")
		{
			// looking for nothing specific...everything is fine
			found_symbol = true;
		}

		for (auto v = it->second->get_var_buffers().begin(); v != it->second->get_var_buffers().end(); ++v)
		{
			v->reset_buffer();
			Token t = v->get_next_token();
			std::string var_code;

			if ((t.str == "uint") || (t.str == "int") || (t.str == "String") || (t.str == "bool") || (t.str == "half") || (t.str == "float"))
			{
				code += Converter_RVC_Rust::convert_expression(t, *v, d.get_symbol_map(), d.get_symbol_type_map(), empty_map, it->first, "");
			}
			else if (t.str == "List")
			{
				code += Converter_RVC_Rust::convert_list(t, *v, d.get_symbol_map(), d.get_symbol_map(), d.get_symbol_type_map(), empty_map, it->first, "\t");
			}
			else if (t.str == "")
			{
				// std::cout << "DEBUG 1" << std::endl;
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			else
			{
				throw Wrong_Token_Exception{ "Unexpected token during processing of Unit file." };
			}
		}
		for (auto m = it->second->get_method_buffers().begin(); m != it->second->get_method_buffers().end(); ++m)
		{
			m->reset_buffer();
			Token t = m->get_next_token();
			if (t.str == "function")
			{
				std::string tmp{ Converter_RVC_Rust::convert_function(t, *m, d.get_symbol_map(), d.get_symbol_type_map(), empty_map, "\t", it->first) };
				code += tmp;
				// find declaration and insert it at the beginning of the source string, to avoid linker errors
				// std::string dekl = tmp.substr(0, tmp.find("{")) + ";\n";
				// declarations.insert(0, dekl);
			}
			else if (t.str == "procedure")
			{
				std::string tmp{ Converter_RVC_Rust::convert_procedure(t, *m, d.get_symbol_map(), d.get_symbol_type_map(), empty_map, "\t", it->first) };
				code += tmp;
				// find declaration and insert it at the beginning of the source string, to avoid linker errors
				// std::string dekl = tmp.substr(0, tmp.find("{")) + ";\n";
				// declarations.insert(0, dekl);
			}
			else if (t.str == "")
			{
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			else
			{
				throw Wrong_Token_Exception{ "Unexpected token during processing of Unit file." };
			}
		}
		for (auto n = it->second->get_native_buffers().begin(); n != it->second->get_native_buffers().end(); ++n)
		{
			n->reset_buffer();
			Token t = n->get_next_token();
			declarations += Converter_RVC_Rust::convert_native_declaration(t, *n, it->first, d);
		}

		if (d.get_symbol_map().contains(it->first))
		{
			found_symbol = true;
		}
		if (!found_symbol)
		{
			throw Code_Generation::Code_Generation_Exception{ "Didn't find symbol " + it->first + " in import." };
		}
		d.add_var_code(code);
		d.add_declarations(declarations);
	}
}

std::pair<Code_Generation_Rust::Header, Code_Generation_Rust::Source>
Code_Generation_Rust::generate_actor_code(
	IR::Actor_Instance* instance,
	std::string class_name,
	std::set<std::string>& unused_actions,
	std::set<std::string>& unused_in_channels,
	std::set<std::string>& unused_out_channels,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::string channel_include,
	unsigned scheduling_loop_bound)
{
	IR::Actor* actor = instance->get_actor();
	std::string header_name, source_name;

#ifdef DEBUG_ACTOR_GENERATION
	std::cout << "Generation of actor " << actor->get_class_name() << " with name " << class_name << std::endl;
#endif
	std::set<std::string> used_in_channels;
	std::set<std::string> used_out_channels;
	std::set<std::string> actor_var_map;
	std::string header_code, source_code;
	Config* c = c->getInstance();
	std::map<std::string, std::string> constructor_parameter_name_type_map;

	Actor_Conversion_Data& d = instance->get_conversion_data();

	convert_import(instance);

	std::map<std::string, std::string> guard_map;
	std::string tmp_name;
	tmp_name = class_name;
	std::transform(tmp_name.begin(), tmp_name.end(), tmp_name.begin(), [](unsigned char c)
		{ return std::tolower(c); });
	header_name = tmp_name + ".rs";

	// we change the class_name to match with calls in main
	// class_name = instance->get_name();
	header_code.append("use crate::actor::Actor;\n");
	header_code.append("use crate::channel::{ChannelReceiver, ChannelSender};\n");

	if (c->get_orcc_compat())
	{
		// header_code.append("#include \"options.h\"\n");
		header_code.append("mod \"options\";\n");
	}

	header_code.append(generate_FSM(actor, unused_actions, class_name, "\t"));

	// header_code.append("\n");
	header_code.append(d.get_declarations_code());
	header_code.append(convert_natives(actor));
	// header_code.append("\n");

	// Struct
	header_code.append(d.get_var_code());
	header_code.append("\n");
	auto tmp = class_variable_generation(instance, opt_data1, opt_data2, map_data,
		constructor_parameter_name_type_map, unused_in_channels, unused_out_channels, used_in_channels, used_out_channels, actor_var_map, actor->get_fsm(), d, "\t");
	header_code.append(tmp.second);

	header_code.append("\n");
	header_code.append("pub struct " + class_name + " {\n");

	header_code.append(tmp.first);
	header_code.append("}\n");

	header_code.append("\n");
	// impl actor function and actions
	header_code.append("impl " + class_name + " {\n");
	header_code.append(constructor_generation(actor, opt_data1, opt_data2, map_data,
		constructor_parameter_name_type_map, used_in_channels, used_out_channels, class_name, d));
	header_code.append("\n");

	header_code.append(function_generation(actor, opt_data1, opt_data2, map_data, actor_var_map, d, "\t"));
	header_code.append(action_generation(actor, opt_data1, opt_data2, map_data, unused_actions,
		unused_in_channels, unused_out_channels, actor_var_map, d, "\t"));
	header_code.append("}\n");

	// impl Scheduling
	header_code.append("impl Actor for " + class_name + " {\n");
	std::map<std::string, std::string> empty_map;
	for (auto it = instance->get_actor()->get_actions().begin();
		it != instance->get_actor()->get_actions().end(); ++it)
	{
		Tokenizer tokenizer((*it)->get_guard()); // string like "i<INPUT_SIZE"
		Token guard_token = tokenizer.get_next_token();

		// std::string tmp_guard = Converter_RVC_Rust::convert_expression(guard_token, tokenizer, d.get_symbol_map(), d.get_symbol_type_map(), actor_var_map, "");
		std::string tmp_guard = Converter_RVC_Rust::convert_guard(guard_token, tokenizer, empty_map, actor_var_map);
		// std::string tmp_guard = (*it)->get_guard();
		guard_map[(*it)->get_function_name()] = tmp_guard;
		// guard_map[(*it)->get_function_name()] = (*it)->get_guard();
	}
	header_code.append(init_action_generation(actor, d, actor_var_map, "\t"));
	header_code.append("\n");

	header_code.append(Scheduling::generate_local_scheduler_rust(
		d, guard_map,
		actor->get_fsm(), actor->get_priorities(),
		actor->get_input_classification(),
		actor->get_output_classification(),
		"\t",
		"schedule",
		"&mut self",
		scheduling_loop_bound,
		actor_var_map));

	header_code.append("\n");
	header_code.append("\tfn is_done(&self) -> bool {\n");
	header_code.append("\t\tself.done\n");
	header_code.append("\t}\n");
	header_code.append("}");

	std::filesystem::path path_header{ c->get_target_dir() };
	path_header /= "src";
	path_header /= header_name;
	std::ofstream output_file_header{ path_header };
	if (output_file_header.fail())
	{
		throw Code_Generation::Code_Generation_Exception{ "Cannot open the file " + path_header.string() };
	}
	output_file_header << header_code;
	output_file_header.close();

	if (!source_name.empty() && !source_code.empty())
	{
		std::filesystem::path path_source{ c->get_target_dir() };
		path_source /= source_name;
		std::ofstream output_file_source{ path_source };
		if (output_file_source.fail())
		{
			throw Code_Generation::Code_Generation_Exception{ "Cannot open the file " + path_header.string() };
		}
		output_file_source << source_code;
		output_file_source.close();
	}

	return std::make_pair(header_name, source_name);
}