﻿#include "Action_Conversion_Rust.hpp"
#include "Converter_RVC_Rust.hpp"
#include "Conversion/Unsupported.hpp"
#include "Tokenizer/Tokenizer.hpp"
#include <tuple>
#include "Dataflow_Analysis/Scheduling_Lib/Scheduling_Data.hpp"
#include "ABI/abi.hpp"
#include "Config/config.h"
#include "common/include/String_Helper.h"

/*
	This function takes a token pointing at the start of the list comprehension, the corresponding Action_Buffer,
	the name of the FIFO the list comprehension is used for and the prefix for each line of code as arguments.
	This function is only used for list comprehensions that occur in FIFOs, all other list comprehensions are handled in the Converter_RVC_Cpp.
	The list comprehension is converted to a loop or multiple loops in pure C++ without SYCL. For list comprehension in relation with SYCL there is another function.
	The assumption is, list comprehension can only occur in output FIFOs, because in input FIFOs it wouldn't make any sense.
	The function returns the generated C++ as a string.
	After the function the Token reference should point at the closing ] of the list comprehension.
*/
static std::string convert_list_comprehension(
	Token& t,
	Token_Container& token_producer,
	std::string var_name,
	std::string type,
	std::string prefix,
	std::map<std::string, std::string>& global_map,
	std::map<std::string, std::string>& local_map)
{
	/*
	should be tested more
	*/
	Config* c = c->getInstance();
	std::string output;
	std::string var_iterator = Converter_RVC_Rust::find_unused_name(global_map, local_map);
	std::string tmp;

	t = token_producer.get_next_token(); // skip [
	output.append(prefix + "let mut " + var_name + ": Vec<" + type + "> = Vec::new();\n");

	std::string element_expr;
	while ((t.str != ":") && (t.str != "]"))
	{
		if (t.str == "if")
		{
			auto buf = Converter_RVC_Rust::convert_inline_if(t, token_producer);
			element_expr.append(buf.first);
		}
		else if (t.str == "[")
		{
			element_expr.append(Converter_RVC_Rust::convert_brackets(t, token_producer, false, global_map, local_map, prefix).first);
		}
		else
		{
			element_expr.append(t.str);
			t = token_producer.get_next_token();
		}
	}

	if (t.str == ":")
	{
		t = token_producer.get_next_token(); // skip :

		// loop structure
		std::string head, tail;

		while (t.str != "]")
		{
			if ((t.str == "for") || (t.str == "foreach"))
			{
				t = token_producer.get_next_token(); // variable or type

				std::string var;
				std::string start, end;

				if ((t.str == "uint") || (t.str == "int") || (t.str == "bool") || (t.str == "float") || (t.str == "half"))
				{
					std::string dummy_type = Converter_RVC_Rust::convert_type(t, token_producer, global_map);
					var = t.str;
					t = token_producer.get_next_token(); // in
					t = token_producer.get_next_token(); // start
					start = t.str;
					t = token_producer.get_next_token(); // ..
					t = token_producer.get_next_token(); // end
					end = t.str;
					t = token_producer.get_next_token();

					head.append(prefix + "for " + var + " in " + start + "..=" + end + " {\n");
					tail.append(prefix + "}\n");
				}
				else
				{
					// foreach x in [..]
					var = t.str;
					t = token_producer.get_next_token(); // in
					std::string collection = "{";
					t = token_producer.get_next_token(); // should be [
					while (t.str != "]")
					{
						collection.append(t.str);
						t = token_producer.get_next_token();
					}
					collection.append("}");
					t = token_producer.get_next_token(); // drop ]

					head.append(prefix + "for " + var + " in vec!" + collection + ".iter() {\n");
					tail.append(prefix + "}\n");
				}

				if (t.str == ",") // chained loops
					t = token_producer.get_next_token();
			}
		}
		std::string command = prefix + "\t" + var_name + ".push(" + element_expr + ");\n";

		output.append(head);
		output.append(command);
		output.append(tail);
	}
	else if (t.str == "]")
	{
		// simple value list like [1, 2, 3]
		element_expr.insert(0, "vec![");
		element_expr.append("]");
		output.append(prefix + "let " + var_name + ": Vec<" + type + "> = " + element_expr + ";\n");
	}
	return output;
}

/*
	This function has to be used if something like: int a[2] = b<3?[5,b]:[0,0] is used in the part of the action where the ports/FIFOs are accessed.
	C++ doesn't allow list assignments without initialization. Thus, this must be converted to a for loop surrounded by condition checking (if...else...).
	The result is returned as a string. This function is meant to be used if SYCL is NOT used. Otherwise the elements won't be stored in the FIFOs and undefined variables will be used!
*/
static std::string convert_inline_if_with_list_assignment(
	Token& t,
	Token_Container& token_producer,
	std::map<std::string, std::string>& global_map,
	std::map<std::string, std::string>& local_map,
	std::string prefix,
	std::string var_name,
	std::string type)
{
	/*
	unchanged as rust and c++ use same (if...else...) */
	std::string condition;
	std::string expression1;
	std::string expression2;
	std::string output;
	// Condition
	while (t.str != "?")
	{
		condition.append(t.str + " ");
		t = token_producer.get_next_token();
	}
	t = token_producer.get_next_token(); // skip "?"
	// if body
	int count{ 1 };
	bool nested{ false };
	while (count != 0)
	{
		if (t.str == "?")
		{
			++count;
			nested = true;
		}
		else if (t.str == ":")
		{
			--count;
		}
		expression1.append(t.str + " ");
		t = token_producer.get_next_token();
	}
	expression1.erase(expression1.size() - 2, 2); // remove last :
	if (nested)
	{
		Tokenizer tok{ expression1 };
		Token tok_token = tok.get_next_token();
		expression1 = convert_inline_if_with_list_assignment(tok_token, tok, global_map, local_map, prefix + "\t", var_name, type);
	}
	else
	{
		// auf listenzuweiseung pr�fen, wenn ja mit convert_list_comprehension in C++ Code umwandeln
		if (expression1[0] == '[')
		{
			Tokenizer tok{ expression1 };
			Token tok_token = tok.get_next_token();
			expression1 = convert_list_comprehension(tok_token, tok, var_name, type, prefix + "\t", global_map, local_map);
		}
		else
		{
			expression1 = prefix + "\t" + var_name + " = " + expression1 + ";\n";
		}
	}

	count = 1;
	nested = false;
	while (count != 0)
	{
		if (t.str == "?")
		{
			++count;
			nested = true;
		}
		else if (t.str == ":")
		{
			--count;
		}
		else if (t.str == "")
		{
			break;
		}
		expression2.append(t.str + " ");
		t = token_producer.get_next_token();
	}
	if (nested)
	{
		Tokenizer tok{ expression2 };
		Token t = tok.get_next_token();
		expression2 = convert_inline_if_with_list_assignment(t, tok, global_map, local_map, prefix + "\t", var_name, type);
	}
	else
	{
		// auf listenzuweiseung pr�fen, wenn ja mit convert_list_comprehension in C++ Code umwandeln
		if (expression2[0] == '[')
		{
			Tokenizer tok{ expression2 };
			Token t = tok.get_next_token();
			expression2 = convert_list_comprehension(t, tok, var_name, type, prefix + "\t", global_map, local_map);
		}
		else
		{
			expression2 = prefix + "\t" + var_name + " = " + expression2 + ";\n";
		}
	}

	// Build expression
	output.append(prefix + "if(" + condition + ") {\n");
	output.append(expression1);
	output.append(prefix + "}\n");
	output.append(prefix + "else {\n");
	output.append(expression2);
	output.append(prefix + "}\n");
	return output;
}

/*
	This function takes a token that points to part of the action where operations are performed (after the fifo access part),
	the corresponding Action_Buffer, the local map for this action and the prefix for each line of code as inputs.
	The function converts these statements to C++ and returns this code as a string.
	All symbol declarations/definitions are inserted into the local map.
*/
static std::string convert_action_body(
	Token& t,
	Action_Buffer& token_producer,
	Actor_Conversion_Data& actor_data,
	std::map<std::string, std::string> local_symbol_map,
	std::map<std::string, std::string> local_type_map,
	std::set<std::string> actor_var_map,
	std::string prefix = "")
{
	std::string output;
	while ((t.str != "end") && (t.str != "endaction"))
	{
		if ((t.str == "var") || (t.str == "do"))
		{
			t = token_producer.get_next_token();
		}
		else if ((t.str == "for") || (t.str == "foreach"))
		{
			output.append(Converter_RVC_Rust::convert_for(t, token_producer,
				actor_data.get_symbol_map(), local_symbol_map, actor_data.get_symbol_type_map(), actor_var_map, false, prefix));
		}
		else if (t.str == "while")
		{
			output.append(Converter_RVC_Rust::convert_while(t, token_producer,
				actor_data.get_symbol_map(), local_symbol_map, actor_data.get_symbol_type_map(), actor_var_map, false, prefix));
		}
		else if (t.str == "if")
		{
			output.append(Converter_RVC_Rust::convert_if(t, token_producer,
				actor_data.get_symbol_map(), local_symbol_map, actor_data.get_symbol_type_map(), actor_var_map, false, prefix));
		}
		else
		{
			output.append(Converter_RVC_Rust::convert_expression(t, token_producer,
				actor_data.get_symbol_map(), local_symbol_map, actor_data.get_symbol_type_map(), actor_var_map, "*", false, prefix));
		}
	}
	return output;
}

/*
	This function converts the access part to input fifos.
	The token points at the start of this part and it has to contain the first fifo name or ==> if there is not input fifo access,action buffer has to be the
	corresponding buffer to the token. Also the function takes the action information object for this action, the local map of this action and the prefix for each
	line of code as arguments.
	Every FIFO name with the number of consumed tokens is inserted into the action information object and every symbol declaration is inserted into the local map.
	The function returns the generated code in a string object.
*/
static std::tuple<std::string, std::string> convert_input_FIFO_access(
	Token& t,
	Action_Buffer& token_producer,
	Actor_Conversion_Data& actor_data,
	std::map<std::string, std::string>& local_map,
	std::map<std::string, std::string>& local_type_map,
	std::string prefix,
	std::string method_name,
	bool input_channel_parameters,
	std::set<std::string> unused_in_channels)
{
	Config* c = c->getInstance();
	// token contains start of the fifo part - first fifo name
	std::string output;
	std::string definitions;
	std::string parameters;
	while (t.str != "==>")
	{
		std::string name{ t.str };
		bool unused_channel = (unused_in_channels.find(name) != unused_in_channels.end());
		t = token_producer.get_next_token();							  // :
		t = token_producer.get_next_token();							  // [
		std::vector<std::pair<std::string, bool>> consumed_element_names; // store elements first, because there can be a repeat
		t = token_producer.get_next_token();							  // start of token name part
		while (t.str != "]")
		{
			std::string expr{};
			bool first{ true };
			bool already_known_variable{ false };
			while ((t.str != ",") && (t.str != "]"))
			{
				if (first)
				{
					// check if variable name is already known and therefore there is no need to define it again!
					first = false;
					if (actor_data.get_symbol_map().contains(t.str))
					{
						already_known_variable = true;
					}
				}
				if (t.str == "[")
				{
					while (t.str != "]")
					{
						if (actor_data.get_replacement_map().contains(t.str))
						{
							expr.append(actor_data.get_replacement(t.str));
						}
						else
						{
							expr.append(t.str);
						}
						t = token_producer.get_next_token();
					}
					expr.append(t.str);
					t = token_producer.get_next_token();
				}
				else
				{
					if (actor_data.get_replacement_map().contains(t.str))
					{
						expr.append(actor_data.get_replacement(t.str));
					}
					else
					{
						expr.append(t.str);
					}
					t = token_producer.get_next_token();
				}
			}
			if (t.str == ",")
			{
				t = token_producer.get_next_token();
			}
			consumed_element_names.push_back(std::make_pair(expr, already_known_variable));
		}
		t = token_producer.get_next_token(); // drop ]
		if (t.str == "repeat")
		{
			t = token_producer.get_next_token(); // repeat count
			std::string repeat_count_expression;
			while ((t.str != ",") && (t.str != "==>"))
			{
				repeat_count_expression.append(t.str + " ");
				t = token_producer.get_next_token();
			}
			int repeat_count = Converter_RVC_Rust::evaluate_constant_expression(repeat_count_expression,
				actor_data.get_symbol_map(), local_map);

			std::string repeat_iterator;
			repeat_iterator = Converter_RVC_Rust::find_unused_name(actor_data.get_symbol_map(), local_map);
			if (!unused_channel)
			{
				// output.append(prefix + "for (unsigned " + repeat_iterator + " = 0; " + repeat_iterator + " < " + std::to_string(repeat_count) + "; ++" + repeat_iterator + ") { \n");

				output.append(prefix + "let mut " + repeat_iterator + "= 0;\n");
				output.append(prefix + "while " + repeat_iterator + " < " + std::to_string(repeat_count) + " {\n");

				if (input_channel_parameters)
				{
					parameters.append("\n" + prefix + actor_data.get_channel_name_type_map()[name] + " *" + name + "_param");
				}
			}

			// create expressions to read the tokens from the fifo
			for (auto it = consumed_element_names.begin(); it != consumed_element_names.end(); ++it)
			{
				if (std::get<1>(*it))
				{
					// Variable already declared, no declaration needed
				}
				else
				{
					// definitions.append(prefix + actor_data.get_channel_name_type_map()[name] + " " + std::get<0>(*it) + "[" + std::to_string(repeat_count) + "];\n");
					definitions.append(prefix + "let mut " + std::get<0>(*it) + ": [" + actor_data.get_channel_name_type_map()[name] + ";" + std::to_string(repeat_count) + "];\n");
				}
				if (unused_channel)
				{
					continue;
				}

				if (input_channel_parameters)
				{
					output.append("\t" + prefix + std::get<0>(*it) + "[" + repeat_iterator + "] = " + name + "_param[" + repeat_iterator + "];\n");
				}
				else
				{
					std::string tmp;
					ABI_CHANNEL_READ(c, tmp, name)
						output.append("\t" + prefix + std::get<0>(*it) + "[" + repeat_iterator + "] = " + tmp + ";\n");
				}
			}

			Scheduling::Channel_Schedule_Data d;
			d.channel_name = name;
			d.elements = static_cast<unsigned>(consumed_element_names.size()) * repeat_count;
			d.in = true;
			d.parameter_generated = input_channel_parameters && !unused_channel;
			d.repeat = true;
			d.is_pointer = true;
			d.type = actor_data.get_channel_name_type_map()[name];
			d.unused_channel = unused_channel;
			for (auto it = consumed_element_names.begin(); it != consumed_element_names.end(); ++it)
			{
				d.var_names.push_back(std::get<0>(*it));
				local_type_map[std::get<0>(*it)] = d.type;
			}
			actor_data.add_scheduler_data(method_name, d);

			if (!unused_channel)
			{
				output.append(prefix + "\t " + repeat_iterator + " += 1;\n");
				output.append(prefix + "}\n");
			}
		}
		else
		{
			std::string channel_iterator;
			Tokenizer tokenizer(actor_data.get_channel_name_type_map()[name]); // string like "int(size=32)"
			Token type_token = tokenizer.get_next_token();

			std::string rust_type = Converter_RVC_Rust::convert_type(type_token, tokenizer, actor_data.get_symbol_map(), actor_data.get_symbol_map());
			if (input_channel_parameters && !unused_channel)
			{
				if (consumed_element_names.size() > 1)
				{
					// parameters.append("\n" + prefix + actor_data.get_channel_name_type_map()[name] + " *" + name + "_param");
					// channel_iterator = Converter_RVC_Rust::find_unused_name(actor_data.get_symbol_map(), local_map);
					// output.append(prefix + "unsigned " + channel_iterator + " = 0;\n");
					parameters.append("\n" + prefix + actor_data.get_channel_name_type_map()[name] + " *" + name + "_param");
					channel_iterator = Converter_RVC_Rust::find_unused_name(actor_data.get_symbol_map(), local_map);
					output.append(prefix + "let mut " + channel_iterator + " = 0;\n");
				}
				else
				{
					if (get<1>(consumed_element_names[0]))
					{
						// parameters.append("\n" + prefix + actor_data.get_channel_name_type_map()[name] + " " + name + "_param");
						parameters.append("\n" + prefix + "let mut " + name + "_param : " + actor_data.get_channel_name_type_map()[name] + ";");
						output.append(prefix + get<0>(consumed_element_names[0]) + " = " + name + "_param;\n");
					}
					else
					{
						parameters.append(" " + get<0>(consumed_element_names[0]) + ": " + rust_type);
					}
				}
			}

			// no repeat, every element in the fifo brackets consumes one element
			for (auto it = consumed_element_names.begin(); it != consumed_element_names.end(); ++it)
			{
				if (input_channel_parameters && !unused_channel)
				{
					if (consumed_element_names.size() == 1)
					{
						continue;
					}
					if (std::get<1>(*it))
					{
						output.append(prefix + std::get<0>(*it) + " = " + name + "_param[" + channel_iterator + "];\n");
					}
					else
					{
						// output.append(prefix + actor_data.get_channel_name_type_map()[name] + " " + std::get<0>(*it) + " = " + name + "_param[" + channel_iterator + "];\n");
						output.append(prefix + "let mut " + std::get<0>(*it) + ": " + actor_data.get_channel_name_type_map()[name] + " = " + name + "_param[" + channel_iterator + "];\n");
					}
				}
				else
				{
					if (std::get<1>(*it))
					{
						if (!unused_channel)
						{
							std::string tmp;
							ABI_CHANNEL_READ(c, tmp, name)
								output.append(prefix + std::get<0>(*it) + " = " + tmp + ";\n");
						}
					}
					else
					{
						output.append(prefix + actor_data.get_channel_name_type_map()[name] + " " + std::get<0>(*it));
						if (unused_channel)
						{
							output.append(";\n");
						}
						else
						{
							std::string tmp;
							ABI_CHANNEL_READ(c, tmp, name)
								output.append(" = " + tmp + "; \n");
						}
					}
				}
			}

			Scheduling::Channel_Schedule_Data d;
			d.channel_name = name;
			d.elements = static_cast<unsigned>(consumed_element_names.size());
			d.in = true;
			d.parameter_generated = input_channel_parameters && !unused_channel;
			d.repeat = false;
			d.is_pointer = consumed_element_names.size() > 1;
			d.type = actor_data.get_channel_name_type_map()[name];
			d.unused_channel = unused_channel;
			for (auto it = consumed_element_names.begin(); it != consumed_element_names.end(); ++it)
			{
				d.var_names.push_back(std::get<0>(*it));
				local_type_map[std::get<0>(*it)] = d.type;
			}
			actor_data.add_scheduler_data(method_name, d);
		}
		if (t.str == ",")
		{ // another fifo access, otherwise it has to be ==> and the loop terminates
			t = token_producer.get_next_token();
			if (input_channel_parameters && !unused_channel)
			{
				parameters.append(",");
			}
		}
	}
	return std::make_tuple(parameters, definitions + output);
}

/*
	The functions takes a token pointing at the first fifo name of the output part and the corresponding action buffer, the action information object
	for this action, the local map of this action and the prefix for each line of code as input parameters.
	The fifo accesses are converted to pure C++ code and this code is returned as a string.
	Additionally, each fifo name and number of consumed/produces tokens in this access are inserted into the output fifo list of the action information list.
	All defined symbols are inserted into the local map.
*/
static std::tuple<std::string, std::string> convert_output_FIFO_access(
	Token& t,
	Action_Buffer& token_producer,
	Actor_Conversion_Data& actor_data,
	std::map<std::string, std::string>& local_map,
	std::string prefix,
	std::string method_name,
	bool output_channel_parameters,
	std::set<std::string> unused_out_channels,
	std::set<std::string> actor_var_map)
{
	Config* c = c->getInstance();
	// token contains start of output fifo part - first fifo name
	std::string output;
	std::string definitions;
	std::string parameters;
	while ((t.str != "do") && (t.str != "guard") && (t.str != "var") && (t.str != "end") && (t.str != "endaction"))
	{
		unsigned accessor_counter = 0;
		std::string name{ t.str };
		bool unused_channel = (unused_out_channels.find(name) != unused_out_channels.end());
		std::vector<std::tuple<std::string, bool>> output_fifo_expr; // bool = true indicates that define shall be added, e.g. for list comprehension
		t = token_producer.get_next_token();						 //:
		t = token_producer.get_next_token();						 //[
		t = token_producer.get_next_token();
		while (t.str != "]")
		{
			std::string expr;
			bool add_define{ false };
			std::string accessor_var = name + "_" + std::to_string(accessor_counter);
			if (t.str == "[")
			{
				// Found List comprehension
				output.append(convert_list_comprehension(t, token_producer, accessor_var,
					actor_data.get_channel_name_type_map()[name],
					prefix, actor_data.get_symbol_map(), local_map));
				t = token_producer.get_next_token(); // drop last ] of the list comprehension
				expr = accessor_var;
				add_define = true;
			}
			else
			{
				while ((t.str != ",") && (t.str != "]"))
				{
					if (t.str == "[")
					{
						while (t.str != "]")
						{
							expr.append(t.str);
							t = token_producer.get_next_token();
						}
						expr.append(t.str);
						t = token_producer.get_next_token();
					}
					else if (t.str == "(")
					{
						while (t.str != ")")
						{
							expr.append(t.str);
							t = token_producer.get_next_token();
						}
						expr.append(t.str);
						t = token_producer.get_next_token();
					}
					else if (t.str == "if")
					{
						auto tmp = Converter_RVC_Rust::convert_inline_if(t, token_producer);
						if (tmp.second)
						{
							Tokenizer tok{ tmp.first };
							Token tok_token = tok.get_next_token();
							output.append(convert_inline_if_with_list_assignment(tok_token, tok,
								actor_data.get_symbol_map(), local_map, prefix, accessor_var,
								actor_data.get_channel_name_type_map()[name]));
							add_define = true;
							expr.append(accessor_var);
						}
						else
						{
							expr.append(tmp.first);
						}
					}
					else
					{
						expr.append(t.str);
						t = token_producer.get_next_token();
					}
				}
				// output.append(prefix + accessor_var + " =" + expr + ";\n");
			}
			if (t.str == ",")
			{
				t = token_producer.get_next_token();
				++accessor_counter;
			}
			output_fifo_expr.push_back(std::make_tuple(expr, add_define));
		}

		t = token_producer.get_next_token();
		if (t.str == "repeat")
		{
			t = token_producer.get_next_token(); // contains repeat count
			std::string repeat_count_expression;
			while ((t.str != ",") && (t.str != "do") && (t.str != "var") && (t.str != "end") && (t.str != "endaction") && (t.str != "guard"))
			{
				repeat_count_expression.append(t.str + " ");
				t = token_producer.get_next_token();
			}

			int repeat_count = Converter_RVC_Rust::evaluate_constant_expression(repeat_count_expression,
				actor_data.get_symbol_map(), local_map);

			std::string repeat_iterator;
			std::string in_repeat_iterator;
			if (!unused_channel)
			{
				repeat_iterator = Converter_RVC_Rust::find_unused_name(actor_data.get_symbol_map(), local_map);
				// output.append(prefix + "for (unsigned " + repeat_iterator + " = 0; " + repeat_iterator + " < " + std::to_string(repeat_count) + "; ++" + repeat_iterator + ") {\n");
				output.append(prefix + "let mut " + repeat_iterator + "= 0;\n");
				output.append(prefix + "while " + repeat_iterator + " < " + std::to_string(repeat_count) + " {\n");

				in_repeat_iterator = Converter_RVC_Rust::find_unused_name(actor_data.get_symbol_map(), local_map);
				if (output_channel_parameters)
				{
					parameters.append("\n" + prefix + actor_data.get_channel_name_type_map()[name] + " *" + name + "_param");
					output.append(prefix + "\t" + in_repeat_iterator + " = " + repeat_iterator + " * " + std::to_string(output_fifo_expr.size()) + ";\n");
				}
			}
			for (auto it = output_fifo_expr.begin(); it != output_fifo_expr.end(); ++it)
			{
				if (std::get<1>(*it))
				{
					// in case of the unused channel it could also be an unconnected channel,
					// hence, we must generate a variable as it might be used before.
					// The compiler will optimize this anyhow.
					// definitions.append(prefix + actor_data.get_channel_name_type_map()[name] + " " + std::get<0>(*it) + "[" + std::to_string(repeat_count) + "];\n");
					definitions.append(prefix + std::get<0>(*it) + ": " + actor_data.get_channel_name_type_map()[name] + " = " + "[" + std::to_string(repeat_count) + "];\n");
				}
				if (unused_channel)
				{
					continue;
				}

				if (output_channel_parameters)
				{
					// output.append(prefix + "\t" + name + "_param[" + in_repeat_iterator + "++] = " + std::get<0>(*it) + "[" + repeat_iterator + "];\n");
					output.append(prefix + "\t" + name + "_param[" + in_repeat_iterator + "] = " + std::get<0>(*it) + "[" + repeat_iterator + "];\n");
					output.append(prefix + "\t" + in_repeat_iterator + " += 1;\n");
				}
				else
				{
					std::string tmp;
					std::string wv = std::get<0>(*it) + "[" + repeat_iterator + "]";
					ABI_CHANNEL_WRITE(c, tmp, wv, name, prefix)
						output.append("\t" + tmp + "\n");
				}
			}

			Scheduling::Channel_Schedule_Data d;
			d.channel_name = name;
			d.elements = static_cast<unsigned>(output_fifo_expr.size()) * repeat_count;
			d.in = false;
			d.parameter_generated = output_channel_parameters && !unused_channel;
			d.repeat = true;
			d.is_pointer = true;
			d.type = actor_data.get_channel_name_type_map()[name];
			d.unused_channel = unused_channel;
			// ignore var_names here, as the output is written to the channel anyhow and not required for guards
			actor_data.add_scheduler_data(method_name, d);

			if (!unused_channel)
			{

				output.append(prefix + "\t " + repeat_iterator + "+= 1;\n");
				output.append(prefix + "}\n");
			}
		}
		else
		{
			if (!unused_channel)
			{
				/* Ignore the add_define as it is only a scalar value that can written directly to the output */
				std::string channel_iterator;
				if (output_channel_parameters && !unused_channel)
				{
					if (output_fifo_expr.size() == 1)
					{
						// parameters.append("\n" + prefix + actor_data.get_channel_name_type_map()[name] + " &" + name + "_param");
						// e.g. let value_param = &original;  'value_param' is a reference to 'original'
						parameters.append("\n" + prefix + name + "_param : " + actor_data.get_channel_name_type_map()[name]);
					}
					else
					{
						// parameters.append("\n" + prefix + actor_data.get_channel_name_type_map()[name] + " *" + name + "_param");
						parameters.append("\n" + prefix + actor_data.get_channel_name_type_map()[name] + " *" + name + "_param");
						channel_iterator = Converter_RVC_Rust::find_unused_name(actor_data.get_symbol_map(), local_map);
						// output.append(prefix + "unsigned " + channel_iterator + " = 0;\n");
						output.append(prefix + "let mut " + channel_iterator + " = 0;\n");
					}
				}

				for (auto it = output_fifo_expr.begin(); it != output_fifo_expr.end(); ++it)
				{

					if (output_channel_parameters)
					{
						if (output_fifo_expr.size() == 1)
						{
							output.append(prefix + name + "_param = " + get<0>(*it) + ";\n");
						}
						else
						{
							// output.append(prefix + name + "_param[" + channel_iterator + "++] = " + get<0>(*it) + ";\n");
							output.append(prefix + name + "_param[" + channel_iterator + "] = " + get<0>(*it) + ";\n");
							output.append(prefix + channel_iterator + " += 1;\n");
						}
					}
					else
					{
						std::string tmp;
						// std::string wv = get<0>(*it);
						Tokenizer tokenizer(get<0>(*it)); // string like "INPUT_SIZE"
						Token token = tokenizer.get_next_token();

						// we use the function to set the keyword self
						std::string wv = Converter_RVC_Rust::convert_guard(token, tokenizer, actor_data.get_symbol_map(), actor_var_map);
						ABI_CHANNEL_WRITE(c, tmp, wv, name, prefix)
							output.append(tmp + "\n");
					}
				}
			}

			Scheduling::Channel_Schedule_Data d;
			d.channel_name = name;
			d.elements = static_cast<unsigned>(output_fifo_expr.size());
			d.in = false;
			d.repeat = false;
			d.parameter_generated = output_channel_parameters && !unused_channel;
			d.is_pointer = output_fifo_expr.size() > 1;
			d.type = actor_data.get_channel_name_type_map()[name];
			d.unused_channel = unused_channel;
			// ignore var_names here, as the output is written to the channel anyhow and not required for guards
			actor_data.add_scheduler_data(method_name, d);
		}
		if (t.str == ",")
		{ // another fifo access, otherwise it has to be var or do  and the loop terminates
			t = token_producer.get_next_token();
			if (output_channel_parameters && !unused_channel)
			{
				parameters.append(",");
			}
		}
	}
	return std::make_tuple(parameters, definitions + output);
}

/*
	This function takes an action buffer (index at 0), a action information object and the prefix for each line of code as arguments.
	The tokens in the action buffer will be converted to pure C++ code.
	Additionally, the name, tag, the name of the generated method and the fifo names and the consumed/produced tokens are inserted into the action information object.
	The generated C++ code is return in a string.
	The scheduling conditions (guards, fifo sizes) are inserted into the scheduling condition map of the class.

	This function also appends the declaration of the generated function to the header_output class variable and the
	complete function with class qualifier to the source_output class variable.
*/
std::string convert_action_rust(
	IR::Action* action,
	Action_Buffer* token_producer,
	Actor_Conversion_Data& actor_data,
	bool input_channel_parameters,
	bool output_channel_parameters,
	std::set<std::string> unused_in_channels,
	std::set<std::string> unused_out_channels,
	std::set<std::string> actor_var_map,
	std::string prefix)
{
	std::map<std::string, std::string> local_symbol_map;
	std::map<std::string, std::string> local_type_map{ actor_data.get_symbol_type_map() };
	std::string output{ prefix };
	std::string end_of_output;
	std::string middle_of_output;
	Token t = token_producer->get_next_token();
	std::string method_name;
	Config* c = c->getInstance();
	bool is_init = false;

	if (t.str == "action")
	{ // action has no name
		method_name = action->get_name();
	}
	else if (t.str == "initialize")
	{
		is_init = true;
	}
	else
	{
		std::string action_tag;
		std::string action_name{ "" };

		action_tag = t.str;
		t = token_producer->get_next_token(); // : or .
		if (t.str == ".")
		{
			t = token_producer->get_next_token();
			while (t.str != ":")
			{
				if (t.str == ".")
				{ // dots cannot be used in c++
					action_name.append("_");
				}
				else
				{
					action_name.append(t.str);
				}
				t = token_producer->get_next_token();
			}
		}
		t = token_producer->get_next_token(); // action
		if (t.str == "initialize")
		{ // init action can have a name, dont know why
			is_init = true;
		}
		if (action_tag.empty())
		{
			method_name = action_name;
		}
		else if (action_name.empty())
		{
			method_name = action_tag;
		}
		else
		{
			method_name = action_tag + "_" + action_name;
		}
	}

	if (is_init)
	{
		method_name = "initialize";
		actor_data.set_init();
	}

	// Add it to the list to inform the scheduler about this action even it doesn't read/write channels
	actor_data.init_action_scheduler_data(method_name);
	action->set_function_name(method_name);

	actor_data.get_symbol_map()[method_name] = "";
	// bool insert_separator{false};
	bool insert_separator{ true };
	bool added_parameters{ false };
	// create function head
	if (is_init)
	{
		output.append("fn " + method_name + "(&mut self");
	}
	else
	{
		output.append("pub async fn " + method_name + "(&mut self");
	}

	if (!input_channel_parameters && !output_channel_parameters)
	{
		output.append(") { \n");
	}
	// convert fifo access
	t = token_producer->get_next_token(); // start of the fifo part - fifo name

	// handling action parameters
	while (t.str != "==>")
	{
		// input fifos
		/*
		input fifos shouldn't be read inside of the action but inside of the local_scheduler and passed as the action parameters
		*/
		std::tuple<std::string, std::string> tmp = convert_input_FIFO_access(t, *token_producer, actor_data, local_symbol_map, local_type_map,
			"", method_name, input_channel_parameters, unused_in_channels);

		middle_of_output.append(get<1>(tmp));

		if (input_channel_parameters && !get<0>(tmp).empty())
		{
			added_parameters = true;
			if (insert_separator)
			{
				output.append(",");
			}
			insert_separator = input_channel_parameters;
			output.append(get<0>(tmp));
		}
	}

	t = token_producer->get_next_token(); // get next token, that should contain first output fifo name, if any are accessed
	while ((t.str != "do") && (t.str != "guard") && (t.str != "var") && (t.str != "end") && (t.str != "endaction"))
	{
		// output fifos
		std::tuple<std::string, std::string> tmp = convert_output_FIFO_access(t, *token_producer, actor_data, local_symbol_map,
			prefix + "\t", method_name, output_channel_parameters, unused_out_channels, actor_var_map);

		if (output_channel_parameters)
		{
			if (!std::get<0>(tmp).empty())
			{
				added_parameters = true;
				if (insert_separator)
				{
					output.append(", ");
				}
			}
			output.append(get<0>(tmp));
		}
		end_of_output.append(get<1>(tmp));
	}

	if (input_channel_parameters || output_channel_parameters)
	{
		output.append(") {\n");
	}
	output.append(middle_of_output);

	// read guards
	if (t.str == "guard")
	{
		std::string guard;
		t = token_producer->get_next_token();
		while ((t.str != "var") && (t.str != "do") && (t.str != "end") && (t.str != "endaction"))
		{
			t = token_producer->get_next_token();
		}
	}

	// convert the body of the action
	if ((t.str != "end") && (t.str != "endaction"))
	{
		std::string tmp = convert_action_body(t, *token_producer, actor_data, local_symbol_map, local_type_map, actor_var_map, prefix + "\t");

		output.append(tmp);
	}

	return output + end_of_output + prefix + "}\n";
}