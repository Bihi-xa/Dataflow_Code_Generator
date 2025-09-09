#include "Generate_Rust.hpp"
#include "Converter_RVC_Rust.hpp"
#include "Config/config.h"
#include "Config/debug.h"
#include "Scheduling.hpp"
#include <string>
#include <fstream>
#include <map>
#include <set>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include "ABI/abi.hpp"

/* Map each sender with its appropriat receiver */
static std::map<std::string, std::string> channel_sender_receiver_map;
/* Map each channel to the concrete channel implementation that is used for this channel. */
static std::map<std::string, std::string> channel_impl_map;
/* Map each channel to the type of the tokens it carries. */
static std::map<std::string, std::string> channel_type_map;
/* Map each channel to its size (number of tokens it can carry). */
static std::map<std::string, std::string> channel_size_map;
static std::map<std::string, Actor_Conversion_Data *> actor_data_map;
/* Map actor_instance_name_port_name to the name of the generated channel. */
static std::map<std::string, std::string> actorport_channel_map;
static std::map<std::string, IR::Actor_Instance *> actorname_instance_map; // Not for composit actors, they carry their parameters inside!
static std::vector<std::string> global_scheduling_routines;
static std::set<std::string> actor_original;

static std::string find_channel_type(
	std::string port_name,
	std::vector<IR::Buffer_Access> &ports,
	std::map<std::string, std::string> &global_map)
{
	for (auto it = ports.begin(); it != ports.end(); ++it)
	{
		if (it->buffer_name == port_name)
		{
			Tokenizer tokenizer(it->type);
			Token guard_token = tokenizer.get_next_token();
			std::string rust_type = Converter_RVC_Rust::convert_type(guard_token, tokenizer, global_map);
			return rust_type;
		}
	}
	// this cannot happen as this is detected early during network reading
	throw Code_Generation::Code_Generation_Exception{"Cannot find type for port." + port_name};
}

static std::string generate_actor_constructor_parameters(
	std::string name,
	Actor_Conversion_Data *data,
	bool static_alloc)
{
	std::string result;
	Config *c = c->getInstance();

	result.append("\"" + name + "\"");
	for (auto param_it = data->get_parameter_order().begin();
		 param_it != data->get_parameter_order().end(); ++param_it)
	{
		result.append(", ");
		if (actorport_channel_map.contains(name + "_" + *param_it))
		{
			if (static_alloc)
			{
				result.append("&");
			}
			if (c->get_target_language() == Target_Language::rust2)
			{
				result.append(actorport_channel_map[name + "_" + *param_it] + ".clone()");
			}
			else
			{
				result.append(name + "_" + *param_it);
			}
		}
		else if (actorname_instance_map.contains(name))
		{
			// only true for non-merged actor instances
			IR::Actor_Instance *instance = actorname_instance_map[name];
			if (instance->get_parameters().contains(*param_it))
			{
				result.append(actorname_instance_map[name]->get_parameters()[*param_it]);
			}
			else if (instance->get_conversion_data().get_default_parameter_map().contains(*param_it))
			{
				result.append(instance->get_conversion_data().get_default_parameter_map()[*param_it]);
			}
			else
			{
				if (instance->is_port(*param_it))
				{
					result.append("None");
				}
				else
				{
					// No Parameter value in the network, no default parameter = bug
					throw Code_Generation::Code_Generation_Exception{"No Parameter value given for " + name + " parameter: " + *param_it};
				}
			}
		}
		else
		{
			// something is wrong here, this cannot happen
			std::cout << "ERROR: Parameter insertion for actor constructor failed!" << std::endl;
			exit(6);
		}
	}

	return result;
}

static std::string generate_channels(
	IR::Dataflow_Network *dpn,
	Optimization::Optimization_Data_Phase1 *opt_data1,
	Optimization::Optimization_Data_Phase2 *opt_data2,
	Mapping::Mapping_Data *map_data)
{
	std::string result;
	Config *c = c->getInstance();
	for (auto it = dpn->get_edges().begin();
		 it != dpn->get_edges().end(); ++it)
	{
		IR::Actor_Instance *source = it->get_source();
		IR::Actor_Instance *sink = it->get_sink();
		Actor_Conversion_Data &d1 = source->get_conversion_data();
		Actor_Conversion_Data &d2 = sink->get_conversion_data();
		if ((source->get_composit_actor() != nullptr) && (source->get_composit_actor() == sink->get_composit_actor()))
		{
			// This is just an edge inside a cluster, no need to create a channel object for it.
			continue;
		}
		if (it->is_deleted())
		{
			continue;
		}
		std::string name;

		name = it->get_src_id() + "_" + it->get_src_port() + "_" + it->get_dst_id() + "_" + it->get_dst_port();

		// Rust: we need 2 names channel sender and receiver
		// channel receiver := dst + dst-port
		// channel sender := src + src-port
		std::string channel_receiver;
		std::string channel_sender;
		channel_receiver = it->get_dst_id() + "_" + it->get_dst_port();
		channel_sender = it->get_src_id() + "_" + it->get_src_port();
		if (channel_impl_map.contains(name))
		{
			// just a sanity check, this cannot happen I think
			std::cout << "ERROR: Determined channel name that is already in use: " << name << std::endl;
			exit(5);
		}
		// convert to rust type
		std::string typeSource = find_channel_type(it->get_src_port(), it->get_source()->get_actor()->get_out_buffers(), d1.get_symbol_map());
		std::string typeSink = find_channel_type(it->get_dst_port(), it->get_sink()->get_actor()->get_in_buffers(), d2.get_symbol_map());
		if (typeSource != typeSink)
		{
			throw Code_Generation::Code_Generation_Exception{
				"Types of " + it->get_source()->get_name() + "." + it->get_src_port() + " and " + it->get_sink()->get_name() + "." + it->get_dst_port() + " don't match."};
		}

		actorport_channel_map[it->get_source()->get_name() + "_" + it->get_src_port()] = name;
		actorport_channel_map[it->get_sink()->get_name() + "_" + it->get_dst_port()] = name;

		channel_type_map[name] = typeSource;
		std::string chan_sz;
		if (it->get_specified_size() == c->get_FIFO_size())
		{
			chan_sz = "CHANNEL_SIZE";
		}
		else
		{
			chan_sz = std::to_string(it->get_specified_size());
		}
		channel_size_map[name] = chan_sz;

		std::pair<std::string, std::string> decl;
		// abi not needed
		ABI_CHANNEL_DECL(c, decl, name, chan_sz, typeSource, c->get_static_alloc(), "");

		channel_sender_receiver_map[channel_sender] = channel_receiver;

		channel_impl_map[name] = decl.second;
		// result.append(decl.first);
	}

	return result;
}
/*
this function now declares actor modules
*/
static std::string generate_actor_instances(
	IR::Dataflow_Network *dpn,
	Optimization::Optimization_Data_Phase1 *opt_data1,
	Optimization::Optimization_Data_Phase2 *opt_data2,
	Mapping::Mapping_Data *map_data)
{
	std::string result;
	Config *c = c->getInstance();

	for (auto it = dpn->get_actor_instances().begin();
		 it != dpn->get_actor_instances().end(); ++it)
	{
		if ((*it)->get_composit_actor() != nullptr)
		{
			continue;
		}
		if ((*it)->is_deleted())
		{
			continue;
		}

		// check if module already declared or not
		if (actor_original.find((*it)->get_conversion_data().get_class_name()) == actor_original.end())
		{
			std::string t;
			std::string tmp_name = (*it)->get_conversion_data().get_class_name();

			// Convert each character to lowercase to match the .rs file name
			std::transform(tmp_name.begin(), tmp_name.end(), tmp_name.begin(), [](unsigned char c)
						   { return std::tolower(c); });

			// mod actor1;
			t.append("mod " + tmp_name);
			t.append(";\n");

			// use actor1::Actor1;
			t.append("use " + tmp_name + "::" + (*it)->get_conversion_data().get_class_name());
			actor_original.insert((*it)->get_conversion_data().get_class_name());
			result.append(t + ";\n");
		}

		// Must happen before the constructor parameters are generated!
		actor_data_map[(*it)->get_name()] = (*it)->get_conversion_data_ptr();
		actorname_instance_map[(*it)->get_name()] = (*it);
	}
	result.append("// composit actors\n");

	for (auto it = dpn->get_composit_actors().begin();
		 it != dpn->get_composit_actors().end(); ++it)
	{
		if (actor_original.find((*it)->get_conversion_data().get_class_name()) == actor_original.end())
		{
			std::string t;
			std::string tmp_name = (*it)->get_conversion_data().get_class_name();

			// Convert each character to lowercase to match the .rs file name
			std::transform(tmp_name.begin(), tmp_name.end(), tmp_name.begin(), [](unsigned char c)
						   { return std::tolower(c); });
			// mod actor;
			t.append("mod " + tmp_name);
			t.append(";\n");

			// use actor::Actor;
			t.append("use " + tmp_name + "::" + (*it)->get_conversion_data().get_class_name());
			actor_original.insert((*it)->get_conversion_data().get_class_name());

			result.append(t + ";\n");
		}
		actor_data_map[(*it)->get_name()] = (*it)->get_conversion_data_ptr();
	}

	return result;
}

static std::string generate_main(
	IR::Dataflow_Network *dpn,
	Optimization::Optimization_Data_Phase1 *opt_data1,
	Optimization::Optimization_Data_Phase2 *opt_data2,
	Mapping::Mapping_Data *map_data)
{
	std::string result;
	Config *c = c->getInstance();

	if (c->get_orcc_compat())
	{
		result.append("use std::{env, ffi::CString};\n");

		result.append("extern \"C\" {fn parse_command_line_input(argc: i32, argv: *const *const i8);}\n");
	}

	result.append("#[allow(non_snake_case)]\n");
	if (c->get_target_language() == Target_Language::rust1)
	{
		result.append("#[tokio::main(flavor = \"multi_thread\")]\n");
		result.append("async fn main() {\n");
	}
	else
	{
		result.append("fn main() {\n");
	}

	if (c->get_orcc_compat())
	{
		// Collect command-line arguments in Rust
		result.append("\tlet args: Vec<String> = env::args().collect();");

		// Convert arguments into C-style strings (CString) and collect pointers
		result.append("\tlet argc = args.len() as i32;\n");

		// Create a Vec of raw pointers (i.e., *const i8) for each argument
		result.append("\tlet argv: Vec<*const i8> = args.iter().map(|arg| { CString::new(arg.clone()).unwrap().into_raw() as *const i8 }).collect();\n");

		// Pass the arguments to the C function (unsafe because we are working with raw pointers)
		result.append("\tunsafe {parse_command_line_input(argc, argv.as_ptr());}\n\n");
	}

	// initialize channels
	result.append("\t// Initialize channels\n");
	for (auto it = channel_sender_receiver_map.begin(); it != channel_sender_receiver_map.end(); ++it)
	{
		std::string tmp;
		std::string tmp_name = it->first + "_" + it->second;
		ABI_CHANNEL_INIT(c, tmp, it->first, it->second, channel_impl_map[tmp_name], channel_type_map[tmp_name], channel_size_map[tmp_name], "\t");
		result.append(tmp + "\n");
	}

	result.append("\n\n");

	// initialize actor instances and call their init function
	result.append("\t// Initialize actors\n");
	for (auto it = actor_data_map.begin(); it != actor_data_map.end(); ++it)
	{

		result.append("\tlet mut " + it->first + " = ");
		result.append(it->second->get_class_name() + "::new(");
		result.append(generate_actor_constructor_parameters(it->first, it->second, false));
		result.append(");\n");
		result.append("\t" + it->first + ".initialize();\n");
	}

	result.append("\n\n");

	if (c->get_target_language() == Target_Language::rust2)
	{
		result.append("\tlet mut actors: Vec<&mut dyn Actor> = vec![\n");
		for (auto it = actor_data_map.begin(); it != actor_data_map.end(); ++it)
		{
			result.append("\t\t&mut " + it->first + ",\n");
		}
		result.append("\t];\n");
	}

	// Scheduling
	if (c->get_target_language() == Target_Language::rust1)
	{
		result.append(Scheduling::generate_global_scheduler_rust(dpn, opt_data1, opt_data2, map_data,
																 global_scheduling_routines, actor_data_map));
		result.append("\n\n");
	}
	else
	{
		result.append("\tglobal_scheduler(&mut actors);\n");
	}

	/*
	we cant deal with this part directly now as we dont use a function from outside of main
	but generate it inside depending on the scheduling starategy
	*/
	// if (c->get_omp_tasking())
	// {
	// 	std::cout << "Main Inside. 4" << std::endl;
	// 	result.append("#pragma omp parallel default(shared)\n");
	// 	result.append("#pragma omp single\n");
	// 	result.append("\t{\n");
	// 	for (auto it = global_scheduling_routines.begin();
	// 		 it != global_scheduling_routines.end(); ++it)
	// 	{
	// 		result.append("#pragma omp task\n");
	// 		result.append("\t" + *it + "();\n");
	// 	}
	// 	result.append("\t}\n");
	// }
	// else if (c->get_cores() > 1)
	// {

	// 	std::vector<std::string> identifiers;
	// 	for (unsigned i = 0; i < c->get_cores(); ++i)
	// 	{

	// 		std::string tmp;
	// 		std::string identifier;
	// 		ABI_THREAD_CREATE(c, tmp, global_scheduling_routines[i], "\t", identifier);
	// 		result.append(tmp);
	// 		identifiers.push_back(identifier);
	// 			}

	// 	for (auto i = identifiers.begin(); i != identifiers.end(); ++i)
	// 	{
	// 		std::string tmp;
	// 		ABI_THREAD_START(c, tmp, *i, "\t");
	// 		result.append(tmp);
	// 	}

	// 	for (auto i = identifiers.begin(); i != identifiers.end(); ++i)
	// 	{
	// 		std::string tmp;
	// 		ABI_THREAD_JOIN(c, tmp, *i, "\t");
	// 		result.append(tmp);
	// 	}

	// }
	// else
	// {
	// 	result.append("\t" + global_scheduling_routines[0] + "();\n");
	// }

	result.append("}");
	return result;
}

static std::string generate_orcc_main()
{
}

std::pair<Code_Generation_Rust::Header, Code_Generation_Rust::Source>
Code_Generation_Rust::generate_core(
	IR::Dataflow_Network *dpn,
	Optimization::Optimization_Data_Phase1 *opt_data1,
	Optimization::Optimization_Data_Phase2 *opt_data2,
	Mapping::Mapping_Data *map_data,
	std::vector<std::string> &includes)
{
#ifdef DEBUG_MAIN_GENERATION
	std::cout << "Main Generation." << std::endl;
#endif

	Config *c = c->getInstance();

	std::string code{};

	// if (c->get_list_scheduling())
	// {
	// 	std::cout << "List scheduling not implemented for Rust code generation!" << std::endl;
	// }

	code.append("\nconst CHANNEL_SIZE: usize =  " + std::to_string(c->get_FIFO_size()) + ";\n");

	std::string include_code;
	// in c/c++ we get t = Channel.hpp but in rust we want "mod Channel;" and aswell "use channel::new_channel;"

	include_code.append("mod channel;\n");
	include_code.append("mod actor;\n");
	include_code.append("use actor::Actor;\n");

	// include_code.append("use tokio::task::JoinHandle;\n");
	if (c->get_target_language() == Target_Language::rust1)
	{
		include_code.append("use channel::new_channel;\n");
	}
	else
	{
		include_code.append("use channel::Channel;\n");
		include_code.append("use rayon::prelude::*;\n");
	}

	code.append(include_code);
	code.append(generate_channels(dpn, opt_data1, opt_data2, map_data));
	code.append("\n\n");
	code.append(generate_actor_instances(dpn, opt_data1, opt_data2, map_data));
	code.append("\n\n");

	// the global scheduler will be generated inside of main in case of rust1 (Tokio)
	if (c->get_target_language() == Target_Language::rust2)
	{
		code.append(Scheduling::generate_global_scheduler_rust(dpn, opt_data1, opt_data2, map_data,
															   global_scheduling_routines, actor_data_map));
		code.append("\n\n");
	}

	code.append(generate_main(dpn, opt_data1, opt_data2, map_data));
	std::filesystem::path path{c->get_target_dir()};
	path /= "src";
	std::string filename;

	filename = "main.rs";

	path /= filename;

	std::ofstream output_file{path};
	if (output_file.fail())
	{
		throw Code_Generation::Code_Generation_Exception{"Cannot open the file " + path.string()};
	}
	output_file << code;
	output_file.close();

	return std::make_pair("", filename);
}