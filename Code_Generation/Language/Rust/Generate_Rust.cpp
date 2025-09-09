#include "Generate_Rust.hpp"
#include "Config/config.h"
#include <filesystem>
#include "Misc.hpp"
#include "ABI/abi.hpp"

/* Generate a simple base struct all other actors inherit from.
 * This allows storing all actors in one list.
 * In case of Rust it is trait and struct
 */
static void generate_base_struct(void)
{
	std::string code1 =
		"pub trait Actor {\n"
		"\tfn initialize(&mut self);\n"
		"\tasync fn schedule(&mut self);\n"
		"\tfn is_done(&self) -> bool;\n"
		"}";

	std::string code2 =
		"pub trait Actor: Send + Sync {\n"
		"\tfn initialize(&mut self);\n"
		"\tfn schedule(&mut self);\n"
		"\tfn is_done(&self) -> bool;\n"
		"}";

	Config *c = c->getInstance();
	std::filesystem::path path{c->get_target_dir()};
	path /= "src";
	path /= "actor.rs";

	std::ofstream output_file{path};
	if (output_file.fail())
	{
		throw Code_Generation::Code_Generation_Exception{"Cannot open the file " + path.string()};
	}
	if (c->get_target_language() == Target_Language::rust1)
	{
		output_file << code1;
	}
	else
	{
		output_file << code2;
	}
	output_file.close();
}

static void generate_build(void)
{
	std::string code =
		"use std::fs;\n"
		"\n"
		"fn main() {\n"
		"\tlet mut build = cc::Build::new();\n"
		"\tlet mut found_c = false;\n"
		"\n"
		"\tfor entry in fs::read_dir(\"native\").unwrap() {\n"
		"\t\tlet path = entry.unwrap().path();\n"
		"\t\tif path.extension().unwrap_or_default() == \"c\" {\n"
		"\t\t\tbuild.file(path);\n"
		"\t\t\tfound_c = true;\n"
		"\t\t}\n"
		"\t}\n"
		"\n"
		"\tif found_c {\n"
		"\t\tbuild.compile(\"native_code\");\n"
		"\t}\n"
		"}";

	Config *c = c->getInstance();
	std::filesystem::path path{c->get_target_dir()};
	path /= "build.rs";

	std::ofstream output_file{path};
	if (output_file.fail())
	{
		throw Code_Generation::Code_Generation_Exception{"Cannot open the file " + path.string()};
	}
	output_file << code;
	output_file.close();
}

static void init_abi(IR::Dataflow_Network *dpn)
{
	Config *c = c->getInstance();
	ABI_INIT(c, dpn);
}

std::pair<Code_Generation_Rust::Header, Code_Generation_Rust::Source>
Code_Generation_Rust::start_code_generation(
	IR::Dataflow_Network *dpn,
	Optimization::Optimization_Data_Phase1 *opt_data1,
	Optimization::Optimization_Data_Phase2 *opt_data2,
	Mapping::Mapping_Data *map_data)
{
	init_abi(dpn);

	Config *c = c->getInstance();

	// create src directory
	std::filesystem::path target_dir = c->get_target_dir();
	std::filesystem::create_directories(target_dir / "src");

	// create native directory
	std::filesystem::create_directories(target_dir / "native");

	// generate build.rs to handle native file
	generate_build();

	generate_base_struct();

	if (c->get_orcc_compat())
	{
		return generate_ORCC_compatibility_layer_rust(c->get_target_dir());
	}
	return std::make_pair(std::string(), std::string());
}

std::pair<Code_Generation_Rust::Header, Code_Generation_Rust::Source>
Code_Generation_Rust::end_code_generation(
	IR::Dataflow_Network *dpn,
	Optimization::Optimization_Data_Phase1 *opt_data1,
	Optimization::Optimization_Data_Phase2 *opt_data2,
	Mapping::Mapping_Data *map_data,
	std::vector<Code_Generation_Rust::Header> &headers,
	std::vector<Code_Generation_Rust::Source> &sources)
{
	Config *c = c->getInstance();
	// Cmake generation is not needed, as we are using cargo project

	// Instead we create Cargo.toml
	std::string code{};
	code.append("[package]\n");
	code.append("name = \"generated_project\"\n");
	code.append("version = \"0.1.0\"\n");
	code.append("edition = \"2021\"\n");
	code.append("\n");
	code.append("[build-dependencies]\n");
	code.append("cc = \"1.0\"");
	code.append("\n");
	code.append("[dependencies]\n");
	if (c->get_target_language() == Target_Language::rust1)
	{
		code.append("tokio = { version = \"1\", features = [\"full\"] }\n");
		code.append("async-trait = \"0.1\"");
	}
	else
	{
		code.append("crossbeam = \"0.8\"\n");
		code.append("rayon = \"1.10\"");
	}

	std::filesystem::path path{c->get_target_dir()};
	std::string filename;

	filename = "Cargo.toml";

	path /= filename;

	std::ofstream output_file{path};
	if (output_file.fail())
	{
		throw Code_Generation::Code_Generation_Exception{"Cannot open the file " + path.string()};
	}
	output_file << code;
	output_file.close();

	return std::make_pair(std::string(), std::string());
}

std::pair<Code_Generation_Rust::Header, Code_Generation_Rust::Source>
Code_Generation_Rust::generate_channel_code(
	Optimization::Optimization_Data_Phase1 *opt_data1,
	Optimization::Optimization_Data_Phase2 *opt_data2,
	Mapping::Mapping_Data *map_data)
{
	Config *c = c->getInstance();

	ABI_CHANNEL_GEN(c, map_data->actor_sharing)
}