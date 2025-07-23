#include "Converter_RVC_Rust.hpp"
#include <iostream>
#include <algorithm>
#include "Config/config.h"
#include <cstdlib>

namespace Converter_RVC_Rust
{
	std::string convert_inline_if_with_list_assignment(
		std::string string_to_convert,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::map<std::string, std::string>& symbol_type_map,
		std::string prefix,
		std::string outer_expression);

	std::string find_unused_name(
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map)
	{ // Left the same
		for (int i = 1;; ++i)
		{
			std::string output{ "_" + std::to_string(i) };
			if ((global_map.find(output) == global_map.end()) && (local_map.find(output) == local_map.end()))
			{
				local_map[output] = "";
				return output;
			}
		}
	}

	std::string convert_string(
		Token& t,
		Token_Container& token_producer)
	{ // Left the same
		std::string output{};
		if (t.str == "\"")
		{
			output.append("\"");
			t = token_producer.get_next_token();
			while (t.str != "\"")
			{
				if (t.str == "\\")
				{
					t = token_producer.get_next_token();
					output.append("\\" + t.str);
				}
				else if (t.str == "")
				{
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else
				{
					output.append(t.str);
				}
				t = token_producer.get_next_token();
				if (t.str != "\"")
				{
					output.append(" ");
				}
			}
			output.append("\"");
			// must be string termination
			t = token_producer.get_next_token();
		}
		return output;
	}

	namespace
	{
		std::string convert_list_comprehension(
			std::string string_to_convert,
			std::string list_name,
			std::map<std::string, std::string>& global_map,
			std::map<std::string, std::string>& local_map,
			std::map<std::string, std::string>& symbol_type_map,
			std::string prefix = "",
			std::string outer_expression = "",
			bool nested = false);
		// std::string convert_function_call_brakets(
		// 	Token &t,
		// 	Token_Container &token_producer,
		// 	bool println = false)
		// {
		// 	// needs to be tested
		// 	/*
		// 	in rust we println as follow:
		// 	println!("Value of x is: {x}");
		// 	*/
		// 	Config *c = c->getInstance();
		// 	std::string output{};

		// 	if (println)
		// 	{
		// 		output.append("(");
		// 	}

		// 	t = token_producer.get_next_token();
		// 	bool something_to_print{false};
		// 	bool to_string_added{false};
		// 	while (t.str != ")")
		// 	{
		// 		if (t.str == "(")
		// 		{
		// 			output.append(convert_function_call_brakets(t, token_producer));
		// 		}
		// 		else if (t.str == "\"")
		// 		{
		// 			std::string tmp{convert_string(t, token_producer)};
		// 			output.append(tmp);
		// 			something_to_print = true;
		// 		}
		// 		else if (t.str == "+" && println)
		// 		{
		// 			if (to_string_added)
		// 			{
		// 				output.append("}");
		// 				to_string_added = false;
		// 			}
		// 			// output.append(" + ");
		// 			t = token_producer.get_next_token();
		// 		}
		// 		else if (t.str == "")
		// 		{
		// 			throw Wrong_Token_Exception{"Unexpected End of File."};
		// 		}
		// 		else
		// 		{
		// 			if (println)
		// 			{
		// 				// it is not a string, hence, we should add std::to_string for printing, just to be safe
		// 				// in rust we pass it inside of braket such as {x}
		// 				if (!to_string_added)
		// 				{
		// 					output.append("{");
		// 					to_string_added = true;
		// 				}
		// 				output.append(t.str);
		// 			}
		// 			else
		// 			{
		// 				output.append(t.str);
		// 			}
		// 			something_to_print = true;
		// 			t = token_producer.get_next_token();
		// 		}
		// 	}
		// 	if (!println || to_string_added)
		// 	{
		// 		output.append("}");
		// 	}
		// 	t = token_producer.get_next_token();
		// 	if (println && !something_to_print)
		// 	{
		// 		// if the brackets are empty there is nothing to print,
		// 		// so a empty string will be returned to avoid << << without anything in between
		// 		return "";
		// 	}
		// 	return output;
		// }
		std::string convert_function_call_brakets(
			Token& t,
			Token_Container& token_producer,
			std::set<std::string>& actor_var_map,
			bool println = false)
		{
			std::string format_string{};
			bool inside_brace = false;
			std::string tmp_var;

			t = token_producer.get_next_token(); // skip '('

			while (t.str != ")")
			{
				if (t.str == "\"")
				{
					std::string tmp{ convert_string(t, token_producer) };				// e.g., "Value: "
					tmp.erase(std::remove(tmp.begin(), tmp.end(), '"'), tmp.end()); // remove " from the string
					format_string.append(tmp);
				}
				else if (t.str == "+")
				{
					if (inside_brace)
					{
						format_string += "} "; // end last {x}
						inside_brace = false;
					}
					t = token_producer.get_next_token();
					continue;
				}
				else if (t.str == "(")
				{
					format_string += convert_function_call_brakets(t, token_producer, actor_var_map, println);
				}
				else
				{
					if (!inside_brace)
					{
						format_string += " {";
						inside_brace = true;
					}
					if (println)
					{
						if (actor_var_map.count(t.str) > 0)
						{
							tmp_var.append(", self." + t.str);
						}
						else
						{
							tmp_var.append(", " + t.str);
						}
					}
					else
					{
						format_string += t.str;
					}
				}

				t = token_producer.get_next_token();
			}

			if (inside_brace)
			{
				format_string += "} ";
			}

			t = token_producer.get_next_token(); // move past ')'

			if (println)
			{
				return "(\"" + format_string + "\"" + tmp_var;
			}
			else
			{
				return format_string;
			}
		}
	}

	namespace
	{
		std::string get_full_list(
			Token& t,
			Token_Container& token_prod)
		{
			std::string output{ "[" };
			t = token_prod.get_next_token();
			while ((t.str != "]") && (t.str != "}"))
			{
				if ((t.str == "[") || (t.str == "{"))
				{
					output.append(get_full_list(t, token_prod));
				}
				else if (t.str == "")
				{
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else
				{
					output.append(t.str);
					t = token_prod.get_next_token();
				}
			}
			output.append("]");
			t = token_prod.get_next_token();
			return output;
		}

		// only applicable for the size evaluation!!!
		// Don't use in other methods, because ++ and -- might be wrong there
		// Left the same
		int get_next_literal(
			Token& t,
			Token_Container& token_producer,
			std::map<std::string, std::string>& global_map,
			std::map<std::string, std::string>& local_map)
		{
			int return_value{ 0 };
			if (t.str == "++")
			{
				t = token_producer.get_next_token();
				if (local_map.find(t.str) != local_map.end())
				{
					return_value = std::stoi(local_map[t.str]);
				}
				else if (global_map.find(t.str) != global_map.end())
				{
					return_value = std::stoi(global_map[t.str]);
				}
				else
				{
					return_value = std::stoi(t.str);
				}
				return_value++;
				t = token_producer.get_next_token();
			}
			else if (t.str == "--")
			{
				t = token_producer.get_next_token();
				if (local_map.find(t.str) != local_map.end())
				{
					return_value = std::stoi(local_map[t.str]);
				}
				else if (global_map.find(t.str) != global_map.end())
				{
					return_value = std::stoi(global_map[t.str]);
				}
				else
				{
					return_value = std::stoi(t.str);
				}
				return_value--;
				t = token_producer.get_next_token();
			}
			else if (local_map.find(t.str) != local_map.end())
			{
				return_value = std::stoi(local_map[t.str]);
				t = token_producer.get_next_token();
				if (t.str == "++")
				{
					return_value++;
					t = token_producer.get_next_token();
				}
				else if (t.str == "--")
				{
					return_value--;
					t = token_producer.get_next_token();
				}
			}
			else if (global_map.find(t.str) != global_map.end())
			{
				return_value = std::stoi(global_map[t.str]);
				t = token_producer.get_next_token();
				if (t.str == "++")
				{
					return_value++;
					t = token_producer.get_next_token();
				}
				else if (t.str == "--")
				{
					return_value--;
					t = token_producer.get_next_token();
				}
			}
			else
			{
				return_value = std::stoi(t.str);
				t = token_producer.get_next_token();
				if (t.str == "++")
				{
					return_value++;
					t = token_producer.get_next_token();
				}
				else if (t.str == "--")
				{
					return_value--;
					t = token_producer.get_next_token();
				}
			}
			return return_value;
		}

		int evaluate_size(
			Token& t,
			Token_Container& token_producer,
			std::map<std::string, std::string>& global_map,
			std::map<std::string, std::string>& local_map,
			bool rekursive = false);
		// Left the same
		int evaluate_plus_minus_followup(
			Token& t,
			Token_Container& token_producer,
			std::map<std::string, std::string>& global_map,
			std::map<std::string, std::string>& local_map)
		{
			int return_value{ 0 };
			if ((t.str == "+") || (t.str == "-"))
			{
				t = token_producer.get_next_token();
				while ((t.str != "+") && (t.str != "-") && (t.str != ")"))
				{
					if (t.str == "*")
					{
						t = token_producer.get_next_token();
						if (t.str == "(")
						{
							return_value =
								return_value * evaluate_size(t, token_producer, global_map, local_map, true);
						}
						else
						{
							return_value =
								return_value * get_next_literal(t, token_producer, global_map, local_map);
						}
					}
					else if (t.str == "/")
					{
						t = token_producer.get_next_token();
						if (t.str == "(")
						{
							return_value =
								return_value / evaluate_size(t, token_producer, global_map, local_map, true);
						}
						else
						{
							return_value =
								return_value / get_next_literal(t, token_producer, global_map, local_map);
						}
					}
					else if (t.str == "")
					{
						throw Wrong_Token_Exception{ "Unexpected End of File." };
					}
					else
					{ // must a ( or a variable
						if (t.str == "(")
						{
							return_value =
								evaluate_size(t, token_producer, global_map, local_map, true);
						}
						else
						{
							return_value =
								get_next_literal(t, token_producer, global_map, local_map);
						}
					}
				} // end while
			} // end if
			return return_value;
		}
	}
	int evaluate_constant_expression(
		std::string expression,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map)
	{
		Tokenizer token_producer{ "(" + expression + ")" }; // place brackets around the expression to reuse evaluate_size
		Token t = token_producer.get_next_token();
		return evaluate_size(t, token_producer, global_map, local_map, true);
	}

	namespace
	{
		// for C/C++ it takes (size = 64) as an argument
		// the goal is to change it so we get ride of the size =
		// Left the same
		int evaluate_size(
			Token& t,
			Token_Container& token_producer,
			std::map<std::string, std::string>& global_map,
			std::map<std::string, std::string>& local_map,
			bool rekursive)
		{
			Token iads = t;
			if (!rekursive)
			{
				if (t.str != "(")
				{
					return 32;
				}
				t = token_producer.get_next_token(); // size
				t = token_producer.get_next_token(); //=
			}
			t = token_producer.get_next_token();
			int return_value{ 0 };
			while (t.str != ")")
			{
				if (t.str == "+")
				{
					return_value +=
						evaluate_plus_minus_followup(t, token_producer, global_map, local_map);
				}
				else if (t.str == "-")
				{
					return_value -=
						evaluate_plus_minus_followup(t, token_producer, global_map, local_map);
				}
				else if (t.str == "*")
				{
					t = token_producer.get_next_token();
					if (t.str == "(")
					{
						return_value =
							return_value * evaluate_size(t, token_producer, global_map, local_map, true);
					}
					else
					{
						return_value =
							return_value * get_next_literal(t, token_producer, global_map, local_map);
					}
				}
				else if (t.str == "/")
				{
					t = token_producer.get_next_token();
					if (t.str == "(")
					{
						return_value =
							return_value / evaluate_size(t, token_producer, global_map, local_map, true);
					}
					else
					{
						return_value =
							return_value / get_next_literal(t, token_producer, global_map, local_map);
					}
				}
				else if (t.str == "")
				{
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else
				{ // must be a ( or a variable
					if (t.str == "(")
					{
						return_value = evaluate_size(t, token_producer, global_map, local_map, true);
					}
					else
					{
						return_value = get_next_literal(t, token_producer, global_map, local_map);
					}
				}
			}
			t = token_producer.get_next_token();
			return return_value;
		}

		/* try to determine a type as narrow as possible, otherwise long might be a good choice
		need to be tested
		*/
		std::string get_type_of_ranged_for(std::string range)
		{
			size_t pos = 1; /* Skip initial { */

			bool negative = false;
			int res = 0;
			char* e;

			while (size_t next = range.find_first_of(",", pos) != range.npos)
			{
				std::string val = range.substr(pos, next - pos);

				long int x = strtol(val.c_str(), &e, 10);
				pos = next + 1;
				if (x < 0)
				{
					negative = true;
				}
				if (x > SCHAR_MIN && x < SCHAR_MAX)
				{ // 1
					if (res < 1)
					{
						res = 1;
					}
				}
				else if (x > SHRT_MIN && x < SHRT_MAX)
				{ // 2
					if (res < 2)
					{
						res = 2;
					}
				}
				else if (x > INT_MIN && x < INT_MAX)
				{ // 3
					if (res < 3)
					{
						res = 3;
					}
				}
				else if (x > LONG_MIN && x < LONG_MAX)
				{ // 4
					if (res < 4)
					{
						res = 4;
					}
				}
				else
				{
					return "i64"; //"long long int";
				}
			}

			// cover also the last part
			std::string val = range.substr(pos, range.size());

			long int x = strtol(val.c_str(), &e, 10);
			if (x < 0)
			{
				negative = true;
			}
			if (x > SCHAR_MIN && x < SCHAR_MAX)
			{ // 1
				if (res < 1)
				{
					res = 1;
				}
			}
			else if (x > SHRT_MIN && x < SHRT_MAX)
			{ // 2
				if (res < 2)
				{
					res = 2;
				}
			}
			else if (x > INT_MIN && x < INT_MAX)
			{ // 3
				if (res < 3)
				{
					res = 3;
				}
			}
			else if (x > LONG_MIN && x < LONG_MAX)
			{ // 4
				if (res < 4)
				{
					res = 4;
				}
			}
			else
			{
				return "i64"; //"long long int";
			}

			if (x == 1)
			{
				if (negative)
				{
					return "i8"; //"signed char";
				}
				else
				{
					return "u8"; //"unsigned char";
				}
			}
			else if (x == 2)
			{
				if (negative)
				{
					return "i16"; //"signed short";
				}
				else
				{
					return "u16"; //"unsigned short";
				}
			}
			else if (x == 3)
			{
				if (negative)
				{
					return "i32"; //"int";
				}
				else
				{
					return "u32"; //"unsigned";
				}
			}
			else if (x == 4)
			{
				if (negative)
				{
					return "i64"; //"signed long";
				}
				else
				{
					return "u64"; //"unsigned long";
				}
			}
			else
			{
				return "i64"; //"long long int";
			}
		}

		/*
		need to be tested
		*/
		std::pair<std::string, std::string> convert_for_head(
			Token& t,
			Token_Container& token_prod,
			std::map<std::string, std::string>& local_map,
			std::map<std::string, std::string>& symbol_type_map,
			std::string prefix = "")
		{
			Config* c = c->getInstance();
			std::string head{};
			std::string tail{};
			std::string adjusted_prefix{ prefix };
			while ((t.str != "do") && (t.str != "}") && (t.str != ":"))
			{ // } due to list comprehension
				if ((t.str == "for") || (t.str == "foreach"))
				{
					t = token_prod.get_next_token();
					if ((t.str == "uint") || (t.str == "int") || (t.str == "String") || (t.str == "bool") || (t.str == "half") || (t.str == "float"))
					{
						// convert type is discarded, Rust automatically infers the type of the loop variable
						t = token_prod.get_next_token(); // get variable name
						std::string var_name = t.str;
						local_map[var_name] = "";
						// symbol_type_map[var_name] = type;

						t = token_prod.get_next_token(); // in
						t = token_prod.get_next_token(); // start value

						std::string range_start{};
						while (t.str != "..")
						{
							if (t.str == "")
								throw Wrong_Token_Exception{ "Unexpected End of File." };
							range_start.append(t.str);
							t = token_prod.get_next_token();
						}

						t = token_prod.get_next_token(); // skip ".."
						std::string range_end{};
						while (t.str != "do" && t.str != "," && t.str != ":" && t.str != "}")
						{
							if (t.str == "")
								throw Wrong_Token_Exception{ "Unexpected End of File." };
							range_end.append(t.str);
							t = token_prod.get_next_token();
						}

						// Rust inclusive range: 0..=10
						head.append(adjusted_prefix + "for ");
						head.append(var_name + " in ");
						head.append(range_start + "..=");
						head.append(range_end + " {\n");
					}
					else
					{
						// Case: "foreach x in {1,2,3}"
						std::string var_name = t.str;
						local_map[var_name] = "";

						t = token_prod.get_next_token(); // in
						t = token_prod.get_next_token();

						std::string list_expr;
						if ((t.str == "[") || (t.str == "{"))
						{
							list_expr = get_full_list(t, token_prod); // returns something like "{1,2,3}"
						}

						// Replace {} with [] for Rust
						std::replace(list_expr.begin(), list_expr.end(), '{', '[');
						std::replace(list_expr.begin(), list_expr.end(), '}', ']');

						// Rust range-based loop
						head.append(adjusted_prefix + "for ");
						head.append(var_name + " in ");
						head.append(list_expr + ".iter() {\n"); // may need to use .iter_mut() if we need to modify the elements directly inside the loop
					}
					tail.insert(0, adjusted_prefix + "}\n");
					if (t.str == ",")
					{
						// a komma indicates a further loop head, thus the next token has to be inspected
						t = token_prod.get_next_token();
						adjusted_prefix.append("\t");
					}
				}
			}
			return std::make_pair(head, tail);
		}
	}

	namespace
	{
		/**
		Converts the list declaration
		*/
		std::string convert_sub_list(
			Token& t,
			Token_Container& token_producer,
			std::map<std::string, std::string>& global_map,
			std::map<std::string, std::string>& local_map,
			std::set<std::string>& actor_var_map,
			std::string symbol,
			std::string& type)
		{
			/*
			in c++ taken the example output char arr[3][2]; or for inner conversion char[3][2]
			equivalent in rust would be let arr: [[i8; 2]; 3];
			argument example for this function List(type: List(type: int(8) size = 2) size = 3)
			*/
			std::string output{};
			if (t.str == "List")
			{
				t = token_producer.get_next_token(); // (
				t = token_producer.get_next_token(); // type
				t = token_producer.get_next_token(); // :
				t = token_producer.get_next_token(); // type argument

				std::string inner_type;

				if (t.str == "List")
				{
					inner_type = convert_sub_list(t, token_producer, global_map, local_map, actor_var_map, symbol, type);
				}
				else
				{
					inner_type = convert_type(t, token_producer, global_map, local_map);
					type.append(inner_type + " ");
				}

				// we expect: size = <value>
				t = token_producer.get_next_token(); // size
				t = token_producer.get_next_token(); // =
				t = token_producer.get_next_token(); // start of size value

				std::string size_expr;

				while (t.str != ")")
				{
					if (t.str == "(")
					{
						size_expr.append(convert_function_call_brakets(t, token_producer, actor_var_map));
					}
					else if ((global_map.find(t.str) != global_map.end()) && (global_map[t.str] != "") && (global_map[t.str] != "function"))
					{
						size_expr.append(global_map[t.str]);
						t = token_producer.get_next_token();
					}
					else if ((local_map.find(t.str) != local_map.end()) && (local_map[t.str] != "") && (local_map[t.str] != "function"))
					{
						size_expr.append(local_map[t.str]);
						t = token_producer.get_next_token();
					}
					else if (t.str == "")
					{
						throw Wrong_Token_Exception{ "Unexpected End of File." };
					}
					else
					{
						size_expr.append(t.str);
						t = token_producer.get_next_token();
					}
				}
				t = token_producer.get_next_token(); // consume closing ')'

				// constructing Rust type
				output = "[" + inner_type + "; " + size_expr + "]";
			}
			else
			{
				output.append("idk");
			}
			return output;
		}

	}

	std::string convert_inline_if_with_list_assignment(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::map<std::string, std::string>& symbol_type_map,
		std::string prefix,
		std::string outer_expression)
	{
		/*
			argument example for this function: cond ? expr1 : expr2
			c/c++ and rust if statement are similar the key differences are touched by this function
			it statys the same
		*/
		std::string condition;
		std::string expression1;
		std::string expression2;
		std::string output;

		while (t.str != "?")
		{
			if (t.str == "")
			{
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			condition.append(t.str + " ");
			t = token_producer.get_next_token();
		}
		t = token_producer.get_next_token(); // skip "?"
		int count{ 1 };
		bool nested{ false };
		while (count != 0)
		{
			if (t.str == "?")
			{
				++count;
				nested = true;
				expression1.append(t.str + " ");
				t = token_producer.get_next_token();
			}
			else if (t.str == ":")
			{
				--count;
				expression1.append(t.str + " ");
				t = token_producer.get_next_token();
			}
			else if (t.str == "[")
			{
				expression1.append(convert_brackets(t, token_producer, true, global_map, local_map, prefix).first);
			}
			else if (t.str == "")
			{
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			else
			{
				expression1.append(t.str + " ");
				t = token_producer.get_next_token();
			}
		}
		expression1.erase(expression1.size() - 2, 2); // remove last :
		if (nested)
		{
			expression1 =
				convert_inline_if_with_list_assignment(expression1, global_map, local_map, symbol_type_map,
					prefix + "\t", outer_expression);
		}
		else
		{
			// auf listenzuweiseung pr�fen, wenn ja mit convert_list_comprehension in C++ Code umwandeln
			if ((expression1[0] == '[') || (expression1[0] == '{'))
			{
				expression1 =
					convert_list_comprehension(expression1, outer_expression, global_map,
						local_map, symbol_type_map, prefix + "\t");
			}
			else
			{
				expression1 = prefix + "\t" + outer_expression + " = " + expression1 + ";\n";
			}
		}
		//-----------------
		count = 1;
		nested = false;
		while (count != 0)
		{
			if (t.str == "?")
			{
				++count;
				nested = true;
				expression2.append(t.str + " ");
				t = token_producer.get_next_token();
			}
			else if (t.str == ":")
			{
				--count;
				expression2.append(t.str + " ");
				t = token_producer.get_next_token();
			}
			else if (t.str == "")
			{
				break;
			}
			else if (t.str == "[")
			{
				expression2.append(convert_brackets(t, token_producer, true, global_map, local_map, prefix).first);
			}
			else if (t.str == "")
			{
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			else
			{
				expression2.append(t.str + " ");
				t = token_producer.get_next_token();
			}
		}
		if (nested)
		{
			expression2 =
				convert_inline_if_with_list_assignment(expression2, global_map, local_map, symbol_type_map,
					prefix + "\t", outer_expression);
		}
		else
		{
			// auf listenzuweiseung pr�fen, wenn ja mit convert_list_comprehension in C++ Code umwandeln
			if ((expression2[0] == '[') || (expression2[0] == '{'))
			{
				expression2 =
					convert_list_comprehension(expression2, outer_expression, global_map,
						local_map, symbol_type_map, prefix + "\t");
			}
			else
			{
				expression2 = prefix + "\t" + outer_expression + " = " + expression2 + ";\n";
			}
		}

		// Build expression
		output.append(prefix + "if (" + condition + ") {\n");
		output.append(expression1);
		output.append(prefix + "}\n");
		output.append(prefix + "else {\n");
		output.append(expression2);
		output.append(prefix + "}\n");
		return output;
	}

	std::string convert_inline_if_with_list_assignment(
		std::string string_to_convert,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::map<std::string, std::string>& symbol_type_map,
		std::string prefix,
		std::string outer_expression)
	{
		Tokenizer tok{ string_to_convert };
		Token t = tok.get_next_token();
		return convert_inline_if_with_list_assignment(t, tok, global_map, local_map, symbol_type_map, prefix, outer_expression);
	}
	/*
	left the same
	*/
	std::pair<std::string, bool> convert_inline_if(
		Token& t,
		Token_Container& token_producer)
	{
		/*
		Convert a simple if ... then ... else ... endif block into a ternary expression (? :)
		Indicate via convert_to_if whether it�s safe to keep as a ternary,
		or if it must be expanded to an if-else block
		if this function is deemed to be used other then inside function then it must be changed
		as Rust does not have a traditional ternary conditional expression like C++
		*/
		std::string output{};
		std::string previous_token_string;
		bool convert_to_if{ false };
		bool condition_done{ false };
		if (t.str == "if")
		{
			t = token_producer.get_next_token();
			while ((t.str != "end") && (t.str != "endif"))
			{
				if (t.str == "then")
				{
					condition_done = true;
					output.append("?");
					t = token_producer.get_next_token();
					previous_token_string = "?";
				}
				else if (t.str == "else")
				{
					output.append(":");
					t = token_producer.get_next_token();
					previous_token_string = ":";
				}
				else if (t.str == "if")
				{
					auto tmp = convert_inline_if(t, token_producer);
					output.append(tmp.first);
					if (tmp.second)
					{
						convert_to_if = true;
					}
				}
				else if (t.str == "")
				{
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else
				{
					if ((t.str == "[") || (t.str == "{"))
					{
						// ? oder : davor bedeuten, dass es kein array sein kann und
						// somit ist das eine listenzuweisung, was in C++ nicht funktioniert!
						if ((previous_token_string == "?") || (previous_token_string == ":"))
						{
							convert_to_if = true;
						}
					}

					if (t.str == "=")
					{
						output.append(" == ");
					}
					else
					{
						output.append(" " + t.str);
					}
					previous_token_string = t.str;
					t = token_producer.get_next_token();
				}
			}
			t = token_producer.get_next_token();
		}
		return std::make_pair(output, convert_to_if);
	}
	/*
	Rust does not have the ? : ternary operator
	Instead, it uses standard if ... else expressions (which do return values)
	this function goal is the replace conver_inLine_if when needed
	*/
	std::pair<std::string, bool> convert_inline_if_rust(
		Token& t,
		Token_Container& token_producer)
	{
		std::string output{};
		std::string previous_token_string;
		bool convert_to_if{ false };
		bool condition_done{ false };
		std::string condition{};
		std::string then_expr{};
		std::string else_expr{};

		if (t.str == "if")
		{
			t = token_producer.get_next_token();

			// Collect the condition
			while (t.str != "then")
			{
				if (t.str == "")
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				// Treat "=" as "=="
				if (t.str == "=")
					condition.append(" == ");
				else
					condition.append(t.str + " ");
				t = token_producer.get_next_token();
			}

			t = token_producer.get_next_token(); // skip "then"

			// Collect the then-expression
			int nested_count = 0;
			while (true)
			{
				if (t.str == "else" && nested_count == 0)
					break;
				if (t.str == "if")
					++nested_count;
				if (t.str == "endif" && nested_count > 0)
				{
					--nested_count;
				}

				if (t.str == "")
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				then_expr += t.str + " ";
				t = token_producer.get_next_token();
			}

			t = token_producer.get_next_token(); // skip "else"

			// Collect the else-expression
			nested_count = 0;
			while (t.str != "endif" || nested_count > 0)
			{
				if (t.str == "if")
					++nested_count;
				else if (t.str == "endif")
					--nested_count;

				if (t.str == "")
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				else_expr += t.str + " ";
				t = token_producer.get_next_token();
			}

			t = token_producer.get_next_token(); // skip final "endif"

			// Build Rust expression
			output = "if " + condition + "{ " + then_expr + "} else { " + else_expr + "}";

			// If either then/else expression contains list/array or nested if, return `true` so it can be turned into block form later if needed
			if (then_expr.find('{') != std::string::npos || else_expr.find('{') != std::string::npos)
			{
				convert_to_if = true;
			}
		}

		return std::make_pair(output, convert_to_if);
	}

	namespace
	{
		/*
		need to be tested
		uses convert_inline_if_rust indead of convert_inline_if
		*/
		std::string read_brace(
			Token& t,
			Token_Container& token_producer,
			bool convert_if = true)
		{
			/*
			processes a block enclosed in curly braces { ... }
			and returns a string representation of that block,
			optionally converting any inline if expressions it encounters.
			*/
			std::string output;
			if (t.str == "{")
			{
				output.append(t.str);
				t = token_producer.get_next_token();
				while (t.str != "}")
				{
					if (t.str == "{")
					{
						output.append(read_brace(t, token_producer));
					}
					else if (convert_if && t.str == "if")
					{
						output.append(convert_inline_if_rust(t, token_producer).first);
					}
					else if (t.str == "")
					{
						throw Wrong_Token_Exception{ "Unexpected End of File." };
					}
					else
					{
						output.append(t.str + " ");
						t = token_producer.get_next_token();
					}
				}
				output.append(t.str);
				t = token_producer.get_next_token();
			}
			return output;
		}

		/*
		need to be tested
		*/
		std::string convert_list_comprehension(
			Token& t,
			Token_Container& token_producer,
			std::string list_name,
			std::map<std::string, std::string>& global_map,
			std::map<std::string, std::string>& local_map,
			std::map<std::string, std::string>& symbol_type_map,
			std::string prefix = "",
			std::string outer_expression = "",
			bool nested = false)
		{
			/*
			takes areguemnt such as : { a * 2 : for int a in [1, 2, 3] }
			for c++ returns :
			int idx{0};
			for (int a = 0; a <= 3; ++a) {
				result[idx] = a * 2;
				++idx;
			}
			in rust it should be :
			let mut result = Vec::new();
			for x in [1, 2, 3].iter() {
				result.push(x * 2);
			}
			*/
			Config* c = c->getInstance();
			while (list_name.find(" ") != std::string::npos)
			{
				list_name.erase(list_name.find(" "), 1);
			}
			t = token_producer.get_next_token(); // skip {

			std::string index_name = find_unused_name(global_map, local_map);
			std::string output = prefix + "let mut " + list_name + " = Vec::new();\n";
			std::string command;
			bool had_inner_list_comprehension = false;

			while (t.str != "}")
			{
				if (t.str == "{")
				{
					std::string buffer = read_brace(t, token_producer, false);
					Tokenizer tok(buffer);
					Token token_tok = tok.get_next_token();
					std::string expr = nested ? outer_expression + "[" + index_name + "]" : list_name;
					command.append(convert_list_comprehension(token_tok, tok, list_name,
						global_map, local_map, symbol_type_map,
						prefix + "\t",
						expr, true));
					had_inner_list_comprehension = true;
				}
				else
				{
					if (!had_inner_list_comprehension)
					{
						command.append(prefix + "\t" + list_name + ".push(");
					}
					std::string tmp_command;
					while ((t.str != ":") && (t.str != "}"))
					{
						if (t.str == "{")
						{
							std::string tmp = read_brace(t, token_producer);
							tmp_command.append(tmp);
						}
						else if (t.str == "if")
						{
							auto buf = convert_inline_if_rust(t, token_producer);
							if (buf.second)
							{
								std::string tmp_str = command;
								while (tmp_str.find("\t") != std::string::npos)
								{
									tmp_str.erase(tmp_str.find("\t"), 1);
								}
								command = convert_inline_if_with_list_assignment(buf.first, global_map,
									local_map, symbol_type_map,
									prefix + "\t",
									tmp_str);
								tmp_command = "";
							}
							else
							{
								tmp_command.append(buf.first);
							}
						}
						else if (t.str == "")
						{
							throw Wrong_Token_Exception{ "Unexpected End of File." };
						}
						else
						{
							tmp_command.append(t.str);
							t = token_producer.get_next_token();
						}
					}
					if (t.str == ":")
					{
						t = token_producer.get_next_token();
						auto head_tail = convert_for_head(t, token_producer, local_map, symbol_type_map, prefix);
						output.append(head_tail.first);
						output.append(prefix + command + tmp_command + ");\n");
						output.append(head_tail.second);
					}
					else if (t.str == "}")
					{
						output.append(prefix + command + tmp_command + ");\n");
					}
				}
				if (t.str == ",")
				{
					t = token_producer.get_next_token();
					had_inner_list_comprehension = false;
				}
			}
			t = token_producer.get_next_token();
			return output;
		}

		std::string convert_list_comprehension(
			std::string string_to_convert,
			std::string list_name,
			std::map<std::string, std::string>& global_map,
			std::map<std::string, std::string>& local_map,
			std::map<std::string, std::string>& symbol_type_map,
			std::string prefix,
			std::string outer_expression,
			bool nested)
		{
			std::replace(string_to_convert.begin(), string_to_convert.end(), '[', '{');
			std::replace(string_to_convert.begin(), string_to_convert.end(), ']', '}');
			Tokenizer token_producer{ string_to_convert };
			Token t = token_producer.get_next_token(); // this must be the start of the list; { - can be dropped
			return prefix + "{\n" + convert_list_comprehension(t, token_producer, list_name, global_map, local_map, symbol_type_map, prefix + "\t") + prefix + "}\n";
		}

	}
	// std::pair<std::string, bool> convert_brackets(
	// 	Token &t,
	// 	Token_Container &token_producer,
	// 	bool is_list, // is_list => rvalue
	// 	std::map<std::string, std::string> &global_map,
	// 	std::map<std::string, std::string> &local_map,
	// 	std::string prefix)
	// {
	// 	return convert_brackets(t, token_producer, is_list, " ", global_map, local_map, prefix);
	// }

	std::pair<std::string, bool> convert_brackets(
		Token& t,
		Token_Container& token_producer,
		bool is_list, // is_list => rvalue
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::string prefix)
	{
		/*
		Use [ and ] for Rust array/vector literals (Rust uses square brackets, no {} for collections)

		*/
		bool list_comprehension{ false };
		std::string output{};
		std::string previous_token_string;
		if ((t.str == "[") || (t.str == "{"))
		{
			// In Rust, collection literals use square brackets
			output.append("[");
			previous_token_string = t.str;
			t = token_producer.get_next_token();
			for (;;)
			{
				if ((t.str == "[") || (t.str == "{"))
				{
					if ((previous_token_string == "[") || (previous_token_string == ","))
					{
						std::pair<std::string, bool> ret_val =
							convert_brackets(t, token_producer, is_list, global_map, local_map, prefix);
						output.append(ret_val.first);
						if (ret_val.second)
						{
							list_comprehension = true;
						}
					}
					else
					{
						std::pair<std::string, bool> ret_val =
							convert_brackets(t, token_producer, false, global_map, local_map, prefix);
						output.append(ret_val.first);
						if (ret_val.second)
						{
							list_comprehension = true;
						}
					}
					previous_token_string = "]";
				}
				else if ((t.str == "]") || (t.str == "}"))
				{
					output.append("]");
					previous_token_string = t.str;
					t = token_producer.get_next_token();
					break;
				}
				else if (t.str == ":")
				{
					// FOUND LIST COMPREHENSION!!!!!!!!! (still flagged, but no direct Rust syntax here)
					previous_token_string = t.str;
					t = token_producer.get_next_token();
					output.append(":");
					list_comprehension = true;
				}
				else if (t.str == "\"")
				{
					output.append(convert_string(t, token_producer));
					previous_token_string = "\"";
				}
				else if (t.str == "")
				{
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else
				{
					// std::cout << "Token: " << t.str << std::endl;
					// std::cout << "global_map: " << global_map[t.str] << std::endl;
					// std::cout << "local_map: " << local_map[t.str] << std::endl;
					if ((global_map.find(t.str) != global_map.end()) && (global_map[t.str] != "") && (global_map[t.str] != "function"))
					{
						output.append(global_map[t.str]);
					}
					else if ((local_map.find(t.str) != local_map.end()) && (local_map[t.str] != "") && (local_map[t.str] != "function"))
					{
						output.append(local_map[t.str]);
					}
					else
					{
						output.append(t.str);
					}
					previous_token_string = t.str;
					t = token_producer.get_next_token();
				}
			}
		}
		return std::make_pair(output, list_comprehension);
	}

	/*
	need to be tested
	*/
	std::string convert_function(
		Token& t, Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& symbol_type_map,
		std::set<std::string>& actor_var_map,
		std::string prefix,
		std::string symbol)
	{
		std::map<std::string, std::string> local_map{};
		Config* c = c->getInstance();
		if (t.str == "function")
		{
			std::string output{ prefix };
			t = token_producer.get_next_token(); // name
			std::string symbol_name{ t.str };
			global_map[t.str] = "function";
			t = token_producer.get_next_token(); //(
			std::string params;
			params.append("(");
			t = token_producer.get_next_token();

			bool first_param = true;
			while (t.str != ")")
			{
				if (!first_param)
				{
					params.append(", ");
				}
				if ((t.str == "uint") || (t.str == "int") || (t.str == "String") || (t.str == "bool") || (t.str == "half") || (t.str == "float"))
				{
					// params.append(convert_type(t, token_producer, global_map, local_map) + " ");
					std::string rust_type = convert_type(t, token_producer, global_map, local_map);
					params.append(t.str); // must be the parameter name
					params.append(": " + rust_type);
				}
				else if (t.str == "")
				{
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else
				{
					params.append(t.str);
				}
				t = token_producer.get_next_token();
				first_param = false;
			}
			params.append(")");
			t = token_producer.get_next_token();
			if (t.str != "-->")
			{
				std::cerr << "Error parsing function";
			}
			t = token_producer.get_next_token(); // must be the return type
			std::string rust_ret_type = convert_type(t, token_producer, global_map, local_map);
			output.append("fn " + symbol_name + params + " -> " + rust_ret_type + " {\n");

			// HEAD END
			t = token_producer.get_next_token();
			while ((t.str != "end") && (t.str != "endfunction"))
			{
				if ((t.str == "var") || (t.str == "begin") || (t.str == "do") || (t.str == ":") || (t.str == ","))
				{
					t = token_producer.get_next_token();
				}
				else if (t.str == "if")
				{
					output.append(convert_if(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, true, prefix + "\t", false));
				}
				else if ((t.str == "for") || (t.str == "foreach"))
				{
					output.append(convert_for(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, true, prefix + "\t"));
				}
				else if (t.str == "while")
				{
					output.append(convert_while(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, true, prefix + "\t"));
				}
				else if ((t.str == "list") || (t.str == "List"))
				{
					output.append(convert_list(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, "*", prefix + "\t"));
				}
				else if (t.str == "")
				{
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else
				{
					output.append(convert_expression(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, "*", true, prefix + "\t"));
				}
			}
			output.append(prefix + "}\n");
			t = token_producer.get_next_token();
			if ((symbol_name != symbol) && (symbol != "*"))
			{
				return ""; // jump out of this procedure if the symbol is not required by the caller
			}
			return output;
		}
		else
		{
			throw Wrong_Token_Exception{ "Expected a function declaration but found:" + t.str };
		}
	}

	/*
	need to be tested
	*/
	std::string convert_procedure(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& symbol_type_map,
		std::set<std::string>& actor_var_map,
		std::string prefix,
		std::string symbol)
	{
		std::map<std::string, std::string> local_map{};
		Config* c = c->getInstance();
		if (t.str == "procedure")
		{
			std::string output{ prefix };
			output.append("fn ");

			t = token_producer.get_next_token(); // name
			output.append(t.str);

			std::string symbol_name{ t.str };
			global_map[t.str] = "function";

			t = token_producer.get_next_token(); //(
			output.append("(");
			t = token_producer.get_next_token();
			bool first_param = true;
			while (t.str != ")")
			{
				if (!first_param)
				{
					output.append(", ");
				}
				first_param = false;
				if ((t.str == "uint") || (t.str == "int") || (t.str == "String") || (t.str == "bool") || (t.str == "half") || (t.str == "float"))
				{
					std::string rust_type = convert_type(t, token_producer, global_map, local_map);
					t = token_producer.get_next_token(); // parameter name
					output.append(t.str + ": " + rust_type);
					local_map[t.str] = rust_type;
				}
				else if (t.str == "")
				{
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else
				{
					output.append(t.str);
				}
				t = token_producer.get_next_token();
			}
			output.append(") {\n");

			t = token_producer.get_next_token();
			while ((t.str != "end") && (t.str != "endprocedure"))
			{
				if ((t.str == "var") || (t.str == "begin") || (t.str == "do") || (t.str == ":"))
				{
					t = token_producer.get_next_token();
				}
				else if (t.str == "if")
				{
					output.append(convert_if(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, false, prefix + "\t", false));
				}
				else if ((t.str == "for") || (t.str == "foreach"))
				{
					output.append(convert_for(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, false, prefix + "\t"));
				}
				else if (t.str == "while")
				{
					output.append(convert_while(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, false, prefix + "\t"));
				}
				else if ((t.str == "list") || (t.str == "List"))
				{
					output.append(convert_list(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, "*", prefix + "\t"));
				}
				else if (t.str == "")
				{
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else
				{
					output.append(convert_expression(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, "*", false, prefix + "\t"));
				}
			}
			output.append(prefix + "}\n");
			t = token_producer.get_next_token();
			if ((symbol_name != symbol) && (symbol != "*"))
			{
				return ""; // jump out of this procedure if the symbol is not required by the caller
			}
			return output;
		}
		else
		{
			throw Wrong_Token_Exception{ "Expected a procedure declaration but found:" + t.str };
		}
	}

	/*
	need to be tested
	*/
	std::string convert_if(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::map<std::string, std::string>& symbol_type_map,
		std::set<std::string>& actor_var_map,
		bool return_statement,
		std::string prefix,
		bool nested)
	{
		std::string output{};
		if (t.str == "if")
		{
			if (nested)
			{
				output.append("if(");
			}
			else
			{
				output.append(prefix + "if(");
			}
			t = token_producer.get_next_token(); // skip if

			while (t.str != "then")
			{
				if (t.str == "=")
				{
					output.append(" == ");
				}
				else if (t.str == "")
				{
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else
				{
					if (actor_var_map.count(t.str) > 0)
					{
						output.append("self.");
					}
					if (t.str == "]")
					{
						output.append("as usize ");
					}
					output.append(t.str + " ");
				}
				t = token_producer.get_next_token();
			}
			output.append(") {\n");
			t = token_producer.get_next_token(); // skip then

			while ((t.str != "end") && (t.str != "endif"))
			{
				if (t.str == "else")
				{
					t = token_producer.get_next_token();
					if (t.str == "if")
					{
						output.append(prefix + "} else ");
						output.append(convert_if(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, return_statement, prefix, true));
					}
					else
					{
						output.append(prefix + "} else {\n");
					}
				}
				else if (t.str == "if")
				{
					output.append(convert_if(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, return_statement, prefix + "\t", false));
				}
				else if ((t.str == "for") || (t.str == "foreach"))
				{
					output.append(convert_for(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, return_statement, prefix + "\t"));
				}
				else if (t.str == "while")
				{
					output.append(convert_while(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, return_statement, prefix + "\t"));
				}
				else if (t.str == "")
				{
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else
				{
					/*
					expressions usually end with semicolons except for the last expression in a block
					we just append converted expressions with semicolon and newline
					*/
					// output.append(convert_expression(t, token_producer, global_map, local_map, symbol_type_map, "*", return_statement, prefix + "\t"));
					output.append(prefix + "\t" + convert_expression(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, "*", return_statement, ""));
				}
			}
			if (!nested)
			{
				output.append(prefix + "}\n");
			}
			t = token_producer.get_next_token();
		}
		return output;
	}

	/*
	need to be tested
	*/
	std::string convert_for(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::map<std::string, std::string>& symbol_type_map,
		std::set<std::string>& actor_var_map,
		bool return_statement,
		std::string prefix)
	{
		std::string output{};
		if ((t.str == "for") || (t.str == "foreach"))
		{
			std::pair<std::string, std::string> head_tail = convert_for_head(t, token_producer, local_map, symbol_type_map); // t should contain do after this function

			// append loop header (e.g., "for x in y {")
			output.append(prefix + head_tail.first);
			output.append(" {\n");

			t = token_producer.get_next_token();
			while ((t.str != "end") && (t.str != "endfor") && (t.str != "endforeach"))
			{
				if (t.str == "if")
				{
					output.append(convert_if(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, return_statement, prefix + "\t", false));
				}
				else if ((t.str == "for") || (t.str == "foreach"))
				{
					output.append(convert_for(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, return_statement, prefix + "\t"));
				}
				else if (t.str == "while")
				{
					output.append(convert_while(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, return_statement, prefix + "\t"));
				}
				else if (t.str == "")
				{
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else
				{
					output.append(convert_expression(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, "*", return_statement, prefix + "\t"));
				}
			}
			// output.append(prefix + head_tail.second);
			//  Append closing brace for the loop block
			output.append(prefix + "}\n");

			t = token_producer.get_next_token();
		}
		return output;
	}

	/*
	need to be tested
	not changed as rust while semantic is the same as c
	*/
	std::string convert_while(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::map<std::string, std::string>& symbol_type_map,
		std::set<std::string>& actor_var_map,
		bool return_statement,
		std::string prefix)
	{
		std::string output{};
		if (t.str == "while")
		{
			t = token_producer.get_next_token();
			output.append(prefix + "while(");
			while ((t.str != "do") && (t.str != ":"))
			{
				if (t.str == "=")
				{
					output.append(" == ");
				}
				else if (t.str == "")
				{
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else
				{
					if (actor_var_map.count(t.str) > 0)
					{
						output.append("self.");
					}
					output.append(t.str);
				}
				t = token_producer.get_next_token();
			}
			output.append("){\n");
			t = token_producer.get_next_token();
			while ((t.str != "end") && (t.str != "endwhile"))
			{
				if (t.str == "if")
				{
					output.append(convert_if(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, return_statement, prefix + "\t", false));
				}
				else if ((t.str == "for") || (t.str == "foreach"))
				{
					output.append(convert_for(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, return_statement, prefix + "\t"));
				}
				else if (t.str == "while")
				{
					output.append(convert_while(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, return_statement, prefix + "\t"));
				}
				else if (t.str == "")
				{
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else
				{
					output.append(convert_expression(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, "*", return_statement, prefix + "\t"));
				}
			}
			output.append(prefix + "}\n");
			t = token_producer.get_next_token();
		}
		return output;
	}
	namespace
	{
		bool is_const(std::string str, std::map<std::string, std::string>& global_map, std::map<std::string, std::string>& local_map)
		{
			// all math operators are const
			if ((str == "/") || (str == "-") || (str == "+") || (str == "(") || (str == ")") || (str == "*"))
			{
				return true;
			}
			// all digits are const
			bool digit{ true };
			for (auto it = str.begin(); it != str.end(); ++it)
			{
				if ((*it == '0') || (*it == '1') || (*it == '2') || (*it == '3') || (*it == '4') || (*it == '5') || (*it == '6') || (*it == '7') || (*it == '8') || (*it == '9'))
				{
				}
				else
				{
					digit = false;
				}
			}
			if (digit)
			{
				return true;
			}
			if ((global_map.count(str) > 0) && (global_map[str] != "") && (global_map[str] != "function"))
			{
				return true;
			}
			if ((local_map.count(str) > 0) && (local_map[str] != "") && (local_map[str] != "function"))
			{
				return true;
			}
			return false;
		}

	}

	std::string convert_expression(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& symbol_type_map,
		std::set<std::string>& actor_var_map,
		std::string prefix)
	{
		std::string dummy;
		return convert_expression(t, token_producer, global_map, global_map, symbol_type_map, actor_var_map, dummy, "*", false, prefix);
	}

	std::string convert_expression(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& symbol_type_map,
		std::set<std::string>& actor_var_map,
		std::string& symbol_name,
		std::string prefix)
	{
		return convert_expression(t, token_producer, global_map, global_map, symbol_type_map, actor_var_map, symbol_name, "*", false, prefix);
	}

	std::string convert_expression(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::map<std::string, std::string>& symbol_type_map,
		std::set<std::string>& actor_var_map,
		std::string symbol,
		bool return_statement,
		std::string prefix)
	{
		std::string dummy;
		return convert_expression(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, dummy, symbol, return_statement, prefix);
	}
	/*
	need to be more tested
	*/
	std::string convert_expression(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::map<std::string, std::string>& symbol_type_map,
		std::set<std::string>& actor_var_map,
		std::string& symbol_name,
		std::string symbol,
		bool return_statement,
		std::string prefix)
	{
		/*
		some parts (mainly list comprehension) were not changed as to see their behavior first!
		*/
		std::string output{};
		bool type_specified{ false };
		std::string type;
		bool println{ false };
		bool print{ false };

		if ((t.str == "uint") || (t.str == "int") || (t.str == "String") || (t.str == "bool") || (t.str == "half") || (t.str == "float"))
		{
			type = convert_type(t, token_producer, global_map, local_map);
			// output.append(type + " ");
			type_specified = true;
		}
		else if (t.str == "List")
		{
			return convert_list(t, token_producer, global_map, local_map, symbol_type_map, actor_var_map, "*", prefix); // parsing for specific symbol isn't startet here, if specific list has to be found the function is started directly via the entry function of this flow
		}
		else if (return_statement)
		{
			output.append("return ");
		}
		symbol_name = t.str;
		Config* c = c->getInstance();
		if (type_specified)
		{
			local_map[symbol_name] = ""; // insert to check for name collisions
			symbol_type_map[symbol_name] = type;
		}
		if (t.str == "println")
		{
			output.append("println!");
			println = true;
		}
		else if (t.str == "print")
		{
			output.append("println!");
			print = true;
		}
		else
		{
			if (actor_var_map.count(t.str) > 0) // checks if it is an actor variable
			{
				output.append("self.");
			}
			output.append(t.str);
			type = symbol_type_map[symbol_name]; // we take the type of symbol/variable to be use in case of mismatch type
		}
		t = token_producer.get_next_token();
		while (t.str == "[")
		{
			// output.append(convert_brackets(t, token_producer, false, global_map, local_map, prefix).first);
			auto bracket_result = convert_brackets(t, token_producer, false, global_map, local_map, prefix).first;

			// Extract size name between brackets (e.g., INPUT_SIZE from "[INPUT_SIZE]")
			std::string inner = bracket_result.substr(1, bracket_result.length() - 2); // remove [ and ]
			std::string size_value;

			if (local_map.count(inner) > 0 && !local_map[inner].empty())
			{
				size_value = local_map[inner];
			}
			else if (global_map.count(inner) > 0 && !global_map[inner].empty())
			{
				size_value = global_map[inner];
			}
			else
			{
				size_value = inner; // fallback
			}

			type = "[" + type + "; " + size_value + "]";
		}
		if (t.str == ":=")
		{
			if (type_specified)
			{
				// output.insert(0, prefix + "static mut "); // unsafe in rust
				output.insert(0, prefix + "let mut ");
				output.append(": " + type + " = ");
			}
			else
			{
				output.insert(0, prefix);
				output.append(" = ");
			}

			t = token_producer.get_next_token();
			if (t.str == "\"")
			{
				output.append(convert_string(t, token_producer) + ";");
				t = token_producer.get_next_token();
			}
			else if (t.str == "[")
			{
				std::pair<std::string, bool> ret_val =
					convert_brackets(t, token_producer, true, global_map, local_map, prefix);
				if (ret_val.second)
				{
					// remove = sign
					output.erase(output.find_last_of("="));
					if (!type_specified)
					{ // if there is no type, this is not the declaration, thus the part before the equal sign can be skipped
						while (output.find("\t") != std::string::npos)
						{
							output.erase(output.find("\t"), 1);
						}
						output = convert_list_comprehension(ret_val.first, output, global_map, local_map, symbol_type_map, prefix);
					}
					else
					{
						output.append(";\n");
						output.append(convert_list_comprehension(ret_val.first, symbol_name, global_map, local_map, symbol_type_map, prefix));
					}
				}
				else
				{
					if (type_specified)
					{
						output.append(ret_val.first + ";\n");
					}
					else
					{
						output = convert_list_comprehension(ret_val.first, symbol_name, global_map, local_map, symbol_type_map, prefix);
					}
				}
				t = token_producer.get_next_token();
			}
			else if (t.str == "if")
			{
				auto tmp = convert_inline_if(t, token_producer);
				if (tmp.second)
				{
					if (type_specified)
					{
						// remove = because no immediate initialization is possible in c++
						output.erase(output.find(" = "));
						output.append(";\n");
					}
					else
					{
						output = "";
					}
					// convert the expression to an if statement
					output.append(convert_inline_if_with_list_assignment(tmp.first, global_map, local_map, symbol_type_map, prefix, symbol_name));
				}
				else
				{
					output.append(tmp.first);
					output.append(";\n");
				}
			}
			else
			{
				std::string value{};
				bool only_const_values{ true };
				Token previous_token;
				while ((t.str != ":") && (t.str != ",") && (t.str != ";") && (t.str != "do") && (t.str != "begin") && (t.str != "end"))
				{
					if (t.str == "=")
					{ // here can be no assignment, so it must be ==
						value.append(" == ");
						t = token_producer.get_next_token();
					}
					else if (t.str == "if")
					{
						value.append(convert_inline_if(t, token_producer).first);
						only_const_values = false;
					}
					else if (t.str == "")
					{
						throw Wrong_Token_Exception{ "Unexpected End of File." };
					}
					else
					{
						// we deal with mismatch type for indexing we set as type tp usize

						if (t.str == "]")
						{
							value.append(") as usize");
						}

						if (actor_var_map.count(t.str) > 0) // checks if it is an actor variable
						{
							value.append("self.");
						}

						value.append(t.str);

						if (t.str == "[")
						{
							value.append("(");
						}

						if (!is_const(t.str, global_map, local_map))
						{
							only_const_values = false;
							if ((global_map.count(t.str) > 0) && (global_map[t.str] == "function"))
							{
								previous_token = t;
								t = token_producer.get_next_token();
								value.append(convert_function_call_brakets(t, token_producer, actor_var_map));
							}
							else
							{
								previous_token = t;
								t = token_producer.get_next_token();
							}
						}
						else
						{
							previous_token = t;
							t = token_producer.get_next_token();
						}
					}
				}
				if (only_const_values)
				{
					int result = evaluate_constant_expression(value, global_map, local_map);
					output.append(std::to_string(result) + ";\n");
					local_map[symbol_name] = ""; // std::to_string(result); //insert into the map to find it, if it is used to calculate the size of a type!!!
				}
				else
				{
					output.append(value + " as " + type + ";\n");
					local_map[symbol_name] = ""; // insert into the map to symbolize that it is defined but not const!
				}
			}
		}
		else if (t.str == "=")
		{
			if (type_specified)
			{

				output.insert(0, prefix + "const ");
			}
			output.append(": " + type + " = ");
			t = token_producer.get_next_token();
			if (t.str == "\"")
			{
				output.append(convert_string(t, token_producer) + ";");
				t = token_producer.get_next_token();
			}
			else if (t.str == "[")
			{
				std::pair<std::string, bool> ret_val = convert_brackets(t, token_producer, true, global_map, local_map, prefix);
				if (ret_val.second)
				{
					// remove const, because a list cannot be filled by a loop if it is const!
					output.erase(output.find("const "), 6);
					// remove = sign
					output.erase(output.find_last_of("="));
					if (!type_specified)
					{ // if there is no type, this is not the declaration, thus the part before the equal sign can be skipped
						while (output.find("\t") != std::string::npos)
						{
							output.erase(output.find("\t"), 1);
						}
						output = convert_list_comprehension(ret_val.first, output, global_map, local_map, symbol_type_map, prefix);
					}
					else
					{
						output.append(";\n");
						output.append(convert_list_comprehension(ret_val.first, symbol_name, global_map, local_map, symbol_type_map, prefix));
					}
				}
				else
				{
					if (type_specified)
					{
						output.append(ret_val.first + ";\n");
					}
					else
					{
						output =
							convert_list_comprehension(ret_val.first, symbol_name, global_map, local_map, symbol_type_map, prefix);
					}
				}
				t = token_producer.get_next_token();
			}
			else if (t.str == "if")
			{
				auto tmp = convert_inline_if(t, token_producer);
				if (tmp.second)
				{
					if (type_specified)
					{
						// remove const and = because no immediate initialization is possible in c++
						output.erase(output.find("const"), 6);
						output.erase(output.find(" = "));
						output.append(";\n");
					}
					else
					{
						output = "";
					}
					// convert the expression to an if statement
					output.append(convert_inline_if_with_list_assignment(tmp.first, global_map, local_map, symbol_type_map, prefix, symbol_name));
				}
				else
				{
					output.append(tmp.first);
					output.append(";\n");
				}
			}
			else
			{
				std::string value{};
				bool only_const_values{ true };
				while ((t.str != ":") && (t.str != ",") && (t.str != ";") && (t.str != "do") && (t.str != "begin") && (t.str != "end"))
				{
					if (t.str == "=")
					{ // here can be no assignment, so it must be ==
						value.append(" == ");
						t = token_producer.get_next_token();
					}
					else if (t.str == "if")
					{
						value.append(convert_inline_if(t, token_producer).first);
						only_const_values = false;
					}
					else if (t.str == "")
					{
						throw Wrong_Token_Exception{ "Unexpected End of File." };
					}
					else
					{
						value.append(t.str);
						if (!is_const(t.str, global_map, local_map))
						{
							only_const_values = false;
							if ((global_map.count(t.str) > 0) && (global_map[t.str] == "function"))
							{
								t = token_producer.get_next_token();
								value.append(convert_function_call_brakets(t, token_producer, actor_var_map));
							}
							else
							{
								t = token_producer.get_next_token();
							}
						}
						else
						{
							t = token_producer.get_next_token();
						}
					}
				}
				if (only_const_values)
				{
					int result = evaluate_constant_expression(value, global_map, local_map);
					output.append(std::to_string(result) + ";\n");
					local_map[symbol_name] = std::to_string(result); // insert into the map to find it, if it is used to calculate the size of a type!!!
				}
				else
				{
					output.append(value + ";\n");
					local_map[symbol_name] = ""; // insert into the map to symbolize that it is defined but not const!
				}
			}
		}
		else
		{ // find expressions with an assignment
			if (!println && !print)
			{
				output.insert(0, prefix + "let ");
			}
			else
			{
				output.insert(0, prefix);
			}

			bool output_appended{ false };
			while ((t.str != ":") && (t.str != ";") && (t.str != ",") && (t.str != "end") && (t.str != "endfunction") && (t.str != "endprocedure") && (t.str != "endaction") && (t.str != "else") && (t.str != "do") && (t.str != "begin"))
			{
				if (t.str == "(")
				{
					output.append(convert_function_call_brakets(t, token_producer, actor_var_map, println || print));
					if (println)
					{
						output.append(");\n");
					}
				}
				else if (t.str == "")
				{
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else
				{
					if (t.str == "=")
					{ // here is no assignment, thus it must be comparsion
						output.append(" == ");
					}
					else
					{
						output.append(t.str);
					}
					t = token_producer.get_next_token();
				}
			}

			if (!println && !print)
			{
				output.append(": " + type + ";\n");
			}
		}
		if ((t.str == ";") || (t.str == ","))
		{
			t = token_producer.get_next_token(); // must drop this token, because ; and , can end one expression, but if it is the last expression in this block, there is no line termination
		}
		if ((symbol_name != symbol) && (symbol != "*"))
		{
			return ""; // stop is symbol is not requested - but must be inserted into the map before
		}

		return output;
	}

	std::string convert_list(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& symbol_type_map,
		std::set<std::string>& actor_var_map,
		std::string prefix)
	{
		return convert_list(t, token_producer, global_map, global_map, symbol_type_map, actor_var_map, "*", prefix);
	}

	std::string convert_list(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::map<std::string, std::string>& symbol_type_map,
		std::set<std::string>& actor_var_map,
		std::string symbol,
		std::string prefix)
	{
		/*
		For example: "int [5][10]" becomes "int my_array[5][10]"
		in Rust:   [[i8; 2]; 3] becomes let arr: [[i8; 2]; 3];
		*/

		std::string type{};
		Config* c = c->getInstance();

		// Returns something like [[i8; 2]; 3]
		std::string rust_type = convert_sub_list(t, token_producer, global_map, local_map, actor_var_map, symbol, type);

		std::string name = t.str; // Get the variable name
		local_map[name] = "";
		symbol_type_map[name] = type;

		t = token_producer.get_next_token(); // move past name

		std::string output;

		if (t.str == ":=")
		{
			output.append(prefix + "let mut " + name + ": " + rust_type + " = ");
			t = token_producer.get_next_token();
			auto ret_val = convert_brackets(t, token_producer, true, global_map, local_map, prefix);

			if (ret_val.second)
			{
				// List comprehension: declare then assign inside loop
				output = prefix + "let mut " + name + ": " + rust_type + ";";
				output += convert_list_comprehension(ret_val.first, name, global_map, local_map, symbol_type_map, prefix);
			}
			else
			{
				output.append(ret_val.first + ";");
			}

			t = token_producer.get_next_token();
		}
		else if (t.str == "=")
		{
			// Const assignment (not always useful in Rust)
			output.append(prefix + "let " + name + ": " + rust_type + " = ");
			t = token_producer.get_next_token();
			auto ret_val = convert_brackets(t, token_producer, true, global_map, local_map, prefix);

			if (ret_val.second)
			{
				// List comprehension
				output = prefix + "let mut " + name + ": " + rust_type + ";\n";
				output += convert_list_comprehension(ret_val.first, name, global_map, local_map, symbol_type_map, prefix);
			}
			else
			{
				output.append(ret_val.first + ";\n");
			}

			t = token_producer.get_next_token();
		}
		else
		{
			// Just declaration
			output.append(prefix + "let mut " + name + ": " + rust_type + ";\n");
		}

		if ((name != symbol) && (symbol != "*"))
		{
			return "";
		}

		return output;
	}

	std::string convert_type(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map)
	{
		return convert_type(t, token_producer, global_map, global_map);
	}

	std::string convert_type(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map)
	{
		if (t.str == "int")
		{
			t = token_producer.get_next_token();
			int value = evaluate_size(t, token_producer, global_map, local_map);
			if (value <= 8)
			{
				return "i8"; //"char";
			}
			else if (value <= 16)
			{
				return "i16"; //"short";
			}
			else if (value <= 32)
			{
				return "i32"; //"int";
			}
			else if (value <= 64)
			{
				return "i64"; //"long";
			}
			else
			{
				return "i32"; //"int";
			}
		}
		else if (t.str == "uint")
		{
			t = token_producer.get_next_token();
			int value = evaluate_size(t, token_producer, global_map, local_map);
			if (value <= 8)
			{
				return "u8"; //"unsigned char";
			}
			else if (value <= 16)
			{
				return "u16"; //"unsigned short";
			}
			else if (value <= 32)
			{
				return "u32"; //"unsigned int";
			}
			else if (value <= 64)
			{
				return "u64"; //"unsigned long";
			}
			else
			{
				return "u32"; //"unsigned int";
			}
		}
		else if (t.str == "bool")
		{
			t = token_producer.get_next_token();
			if (t.str == "(")
			{ // bool shouldnt consist of a size, but just in case it does the tokens will be dropped
				while (t.str != ")")
				{
					if (t.str == "")
					{
						throw Wrong_Token_Exception{ "Unexpected End of File." };
					}
					t = token_producer.get_next_token();
				}
				t = token_producer.get_next_token();
			}
			return "bool";
		}
		else if (t.str == "String")
		{
			t = token_producer.get_next_token();
			if (t.str == "(")
			{ // there shouldnt be a size specified, but if it is, drop it
				while (t.str != ")")
				{
					if (t.str == "")
					{
						throw Wrong_Token_Exception{ "Unexpected End of File." };
					}
					t = token_producer.get_next_token();
				}
				t = token_producer.get_next_token();
			}
			// a pointer to a constant C-style string
			// in rust the equivalent is a string slice that represents a view into a string.
			return "&str"; //"const char*";
		}
		else if (t.str == "float")
		{
			t = token_producer.get_next_token();
			int value = evaluate_size(t, token_producer, global_map, local_map);
			if (value <= 16)
			{
				// Rust does not have a native 16-bit floating-point type (f16) in the standard library
				return "f32"; //"half";
			}
			else if (value <= 32)
			{
				return "f32"; //"float";
			}
			else if (value <= 64)
			{
				return "f64"; //"double";
			}
			else
			{
				return "f32"; //"float";
			}
		}
		else if (t.str == "half")
		{
			t = token_producer.get_next_token();
			int value = evaluate_size(t, token_producer, global_map, local_map);
			if (value <= 16)
			{
				return "f32"; //"half";
			}
			else if (value <= 32)
			{
				return "f32"; //"float";
			}
			else if (value <= 64)
			{
				return "i64"; //"long";
			}
			else
			{
				return "f32"; //"half";
			}
		}
		else
		{
			throw Wrong_Token_Exception{ "Expected a type specifier, but found:" + t.str };
		}
	}

	/*
	need to be tested
	*/
	std::string convert_native_declaration(
		Token& t,
		Token_Container& token_producer,
		std::string symbol,
		Actor_Conversion_Data& actor_conversion_data,
		std::map<std::string, std::string>& global_map)
	{
		/*
		example output:
		@native function my_func(int x, float y) --> int
		endfunction
		c++ output:
		extern "C" int my_func(int x, float y);
		Rust output:
		extern "C" fn my_func(x: i32, y: f32) -> i32;
		*/
		std::string declaration;
		bool add{ false };
		bool function{ false };
		Config* c = c->getInstance();

		if (t.str == "@native")
		{
			t = token_producer.get_next_token();
			if ((t.str != "function") && (t.str != "procedure"))
			{
				throw Wrong_Token_Exception{ "Expected function but found " + t.str + "." };
			}
			if (t.str == "function")
			{
				function = true;
			}
			t = token_producer.get_next_token(); // name
			if ((t.str == symbol) || (symbol == "*"))
			{
				global_map[t.str] = "function";
				actor_conversion_data.add_native_function(t.str);
				add = true;
			}
			declaration.append(t.str);
			t = token_producer.get_next_token(); // shoudl be (

			if (t.str != ("("))
			{
				throw Wrong_Token_Exception{ "Expected \"(\" but found " + t.str + "." };
			}
			declaration.append(t.str);
			t = token_producer.get_next_token();

			while (t.str != ")")
			{
				if ((t.str == "uint") || (t.str == "int") || (t.str == "String") || (t.str == "bool") || (t.str == "half") || (t.str == "float"))
				{
					// declaration.append(convert_type(t, token_producer, global_map, global_map) + " ");
					// declaration.append(t.str); // must be the parameter name
					std::string rust_type = convert_type(t, token_producer, global_map, global_map);
					t = token_producer.get_next_token(); // param name
					declaration.append(t.str + ": " + rust_type);
					t = token_producer.get_next_token();
					if (t.str != ")")
					{
						declaration.append(", ");
					}
				}
				else if (t.str == "")
				{
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else
				{
					// This shouldn't happen
					std::cout << "Native function conversion failed at token " << t.str << std::endl;
					exit(5);
				}
				t = token_producer.get_next_token();
			}
			declaration.append(")");
			t = token_producer.get_next_token(); // -->
			if (function)
			{
				if (t.str != "-->")
				{
					throw Wrong_Token_Exception{ " Expected \"-->\" during native function declaration conversion but found \"" + t.str + "\"." };
				}
				t = token_producer.get_next_token(); // return type
				// must be the type
				std::string return_type = "-> " + convert_type(t, token_producer, global_map, global_map);

				declaration = "extern \"C\" fn " + declaration + " " + return_type + ";\n";
			}
			else
			{
				declaration = "extern \"C\" fn " + declaration + ";\n";
			}
			if (function)
			{
				if ((t.str != "end") && (t.str != "endfunction"))
				{
					throw Wrong_Token_Exception{ "Expected function end but found " + t.str };
				}
			}
			else
			{
				if ((t.str != "end") && (t.str != "endprocedure"))
				{
					throw Wrong_Token_Exception{ "Expected procedure end but found " + t.str };
				}
			}
			t = token_producer.get_next_token();
		}
		else
		{
			throw Wrong_Token_Exception{ "Expected a native declaration but found:" + t.str };
		}

		if (add)
		{
			return declaration + ";\n";
		}
		else
		{
			return std::string();
		}
	}

	std::string convert_native_declaration(
		Token& t,
		Token_Container& token_producer,
		std::string symbol,
		Actor_Conversion_Data& actor_conversion_data)
	{
		std::map<std::string, std::string> tmp_map;
		return convert_native_declaration(t, token_producer, symbol, actor_conversion_data, tmp_map);
	}

	std::string convert_scope(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::map<std::string, std::string>& symbol_type_map,
		std::set<std::string>& actor_var_map,
		std::string prefix = "")
	{
		std::string output{ prefix + "{\n" };
		std::map<std::string, std::string> scope_local_map{ local_map };
		std::map<std::string, std::string> scope_local_type_map{ symbol_type_map };
		t = token_producer.get_next_token(); // skip begin
		while (t.str != "end")
		{
			if ((t.str == "var") || (t.str == "do"))
			{
				t = token_producer.get_next_token();
			}
			else if (t.str == "begin")
			{
				output.append(convert_scope(t, token_producer, global_map, scope_local_map, scope_local_type_map, actor_var_map, prefix + "\t"));
			}
			else if ((t.str == "for") || (t.str == "foreach"))
			{
				output.append(convert_for(t, token_producer, global_map, scope_local_map, scope_local_type_map, actor_var_map, false, prefix + "\t"));
			}
			else if (t.str == "while")
			{
				output.append(convert_while(t, token_producer, global_map, scope_local_map, scope_local_type_map, actor_var_map, false, prefix + "\t"));
			}
			else if (t.str == "")
			{
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			else
			{
				output.append(convert_expression(t, token_producer, global_map, scope_local_map, scope_local_type_map, actor_var_map, "*", false, prefix + "\t"));
			}
		}
		output.append(prefix + "}\n");
		t = token_producer.get_next_token(); // skip end
		return output;
	}
	// parses the parameters and updates the maps

	std::string convert_actor_parameters(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& name_type_map,
		std::map<std::string, std::string>& default_value_map,
		std::set<std::string>& actor_var_map,
		std::string prefix)
	{
		/*
		c/c++ output:
		std::string actor_name;

		rust output:
		actor_name: String,
		inside of the struct declaration
		*/
		std::string result;

		if (t.str != "(")
		{
			throw Wrong_Token_Exception{ "Expected \'(\' but found " + t.str };
		}

		t = token_producer.get_next_token();
		// t must be a type now and we can start parsing
		while (t.str != ")")
		{
			std::string type;
			std::string name;
			std::string default_value;

			type = convert_type(t, token_producer, global_map);
			name = t.str;
			t = token_producer.get_next_token();

			if (t.str == "=")
			{
				// default value given
				t = token_producer.get_next_token();
				while ((t.str != ")") && (t.str != ","))
				{
					if (global_map.contains(t.str))
					{
						default_value.append(global_map[t.str]);
					}
					else
					{
						default_value.append(t.str);
					}
					t = token_producer.get_next_token();
				}
			}
			if (t.str == ",")
			{
				t = token_producer.get_next_token();
			}
			global_map[name] = default_value;
			if (!default_value.empty())
			{
				default_value_map[name] = default_value;
			}
			name_type_map[name] = type;

			// result.append(prefix + type + " " + name + ";\n");
			result.append(prefix + name + ": " + type + ",\n");
			actor_var_map.insert(name);
		}
		t = token_producer.get_next_token();
		return result;
	}
	static std::string get_value_based_on_type(const std::string& type_str)
	{
		if (type_str == "uint")
		{
			return "0";
		}
		else if (type_str == "int")
		{
			return "0";
		}
		else if (type_str == "string")
		{
			return "empty";
		}
		else if (type_str == "bool")
		{
			return "false";
		}
		else if (type_str == "half")
		{
			return "0";
		}
		else if (type_str == "float")
		{
			return "0";
		}
		else
		{
			throw std::invalid_argument("Unknown type: " + type_str);
		}
	}

	std::string convert_expression_act_var(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& symbol_type_map,
		std::string& symbol_name,
		std::string prefix,
		bool in_constructor)
	{
		return convert_expression_act_var(t, token_producer, global_map, global_map, symbol_type_map, symbol_name, "*", false, prefix, in_constructor);
	}

	std::string convert_expression_act_var(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::map<std::string, std::string>& symbol_type_map,
		std::string& symbol_name,
		std::string symbol,
		bool return_statement,
		std::string prefix,
		bool in_constructor)
	{
		/*
		some parts were not changed as to see their behavior first!
		*/
		std::set<std::string> empty_set;
		std::string output{};
		bool type_specified{ false };
		std::string type;
		std::string dummy_value;
		bool println{ false };
		bool print{ false };
		if ((t.str == "uint") || (t.str == "int") || (t.str == "String") || (t.str == "bool") || (t.str == "half") || (t.str == "float"))
		{
			// create dummy value in case needed later
			std::string tmmp = t.str;
			dummy_value = get_value_based_on_type(tmmp);

			type = convert_type(t, token_producer, global_map, local_map);
			// output.append(type + " ");
			type_specified = true;
		}
		else if (t.str == "List")
		{
			return convert_list(t, token_producer, global_map, local_map, symbol_type_map, empty_set, "*", prefix); // parsing for specific symbol isn't startet here, if specific list has to be found the function is started directly via the entry function of this flow
		}
		symbol_name = t.str;
		Config* c = c->getInstance();
		if (type_specified)
		{
			local_map[symbol_name] = ""; // insert to check for name collisions
			symbol_type_map[symbol_name] = type;
		}

		output.append(t.str + ": ");

		t = token_producer.get_next_token();

		while (t.str == "[")
		{
			// output.append(convert_brackets(t, token_producer, false, global_map, local_map, prefix).first);
			// type = convert_brackets(t, token_producer, false, global_map, local_map, prefix).first;
			auto bracket_result = convert_brackets(t, token_producer, false, global_map, local_map, prefix).first;

			// Extract size name between brackets (e.g., INPUT_SIZE from "[INPUT_SIZE]")
			std::string inner = bracket_result.substr(1, bracket_result.length() - 2); // remove [ and ]
			std::string size_value;

			if (local_map.count(inner) > 0 && !local_map[inner].empty())
			{
				size_value = local_map[inner];
			}
			else if (global_map.count(inner) > 0 && !global_map[inner].empty())
			{
				size_value = global_map[inner];
			}
			else
			{
				size_value = inner; // fallback, e.g., "[UNKNOWN]"
			}

			type = "[" + type + "; " + size_value + "]";
		}

		if (t.str == ":=")
		{
			output.insert(0, prefix);
			// output.append(" : ");
			t = token_producer.get_next_token();
			if (t.str == "\"")
			{
				output.append(convert_string(t, token_producer) + ";");
				t = token_producer.get_next_token();
			}
			else if (t.str == "[")
			{
				std::pair<std::string, bool> ret_val =
					convert_brackets(t, token_producer, true, global_map, local_map, prefix);
				if (ret_val.second)
				{
					// remove = sign
					output.erase(output.find_last_of("="));
					if (!type_specified)
					{ // if there is no type, this is not the declaration, thus the part before the equal sign can be skipped
						while (output.find("\t") != std::string::npos)
						{
							output.erase(output.find("\t"), 1);
						}
						output = convert_list_comprehension(ret_val.first, output, global_map, local_map, symbol_type_map, prefix);
					}
					else
					{
						output.append(";\n");
						output.append(convert_list_comprehension(ret_val.first, symbol_name, global_map, local_map, symbol_type_map, prefix));
					}
				}
				else
				{
					if (type_specified)
					{
						if (in_constructor)
						{
							output.append(ret_val.first + ",\n");
						}
						else
						{

							output.append(type + ",\n");
						}
					}
					else
					{
						output = convert_list_comprehension(ret_val.first, symbol_name, global_map, local_map, symbol_type_map, prefix);
					}
				}
				t = token_producer.get_next_token();
			}
			else if (t.str == "if")
			{
				auto tmp = convert_inline_if(t, token_producer);
				if (tmp.second)
				{
					if (type_specified)
					{
						// remove = because no immediate initialization is possible in c++
						output.erase(output.find(" = "));
						output.append(";\n");
					}
					else
					{
						output = "";
					}
					// convert the expression to an if statement
					output.append(convert_inline_if_with_list_assignment(tmp.first, global_map, local_map, symbol_type_map, prefix, symbol_name));
				}
				else
				{
					output.append(tmp.first);
					output.append(";\n");
				}
			}
			else
			{
				std::string value{};
				bool only_const_values = true;
				while ((t.str != ":") && (t.str != ",") && (t.str != ";") && (t.str != "do") && (t.str != "begin") && (t.str != "end"))
				{
					if (t.str == "=")
					{ // here can be no assignment, so it must be ==
						value.append(" == ");
						t = token_producer.get_next_token();
					}
					else if (t.str == "if")
					{
						value.append(convert_inline_if(t, token_producer).first);
						only_const_values = false;
					}
					else if (t.str == "")
					{
						throw Wrong_Token_Exception{ "Unexpected End of File." };
					}
					else
					{

						if (!is_const(t.str, global_map, local_map))
						{
							only_const_values = false;
							if ((global_map.count(t.str) > 0) && (global_map[t.str] == "function"))
							{

								t = token_producer.get_next_token();
								value.append(convert_function_call_brakets(t, token_producer, empty_set));
							}
							else
							{

								t = token_producer.get_next_token();
							}
						}
						else
						{

							t = token_producer.get_next_token();
						}
					}
				}
				if (in_constructor)
				{
					// output.append(value + ",\n");
					if (only_const_values)
					{
						int result = evaluate_constant_expression(value, global_map, local_map);
						output.append(std::to_string(result) + ",\n");
						local_map[symbol_name] = ""; // std::to_string(result); //insert into the map to find it, if it is used to calculate the size of a type!!!
					}
					else
					{
						output.append(value + ",\n");
						local_map[symbol_name] = ""; // insert into the map to symbolize that it is defined but not const!
					}
				}
				else
				{
					output.append(type + ",\n");
				}

				local_map[symbol_name] = ""; // insert into the map to symbolize that it is defined but not const!
			}
		}
		else if (t.str == "=")
		{
			if (type_specified)
			{

				output.insert(0, prefix);
			}
			// output.append(": ");
			t = token_producer.get_next_token();
			if (t.str == "\"")
			{
				// output.append(convert_string(t, token_producer) + ";");
				t = token_producer.get_next_token();
			}
			else if (t.str == "[")
			{
				std::pair<std::string, bool> ret_val = convert_brackets(t, token_producer, true, global_map, local_map, prefix);
				if (ret_val.second)
				{
					// remove const, because a list cannot be filled by a loop if it is const!
					output.erase(output.find("const "), 6);
					// remove = sign
					output.erase(output.find_last_of("="));
					if (!type_specified)
					{ // if there is no type, this is not the declaration, thus the part before the equal sign can be skipped
						while (output.find("\t") != std::string::npos)
						{
							output.erase(output.find("\t"), 1);
						}
						output = convert_list_comprehension(ret_val.first, output, global_map, local_map, symbol_type_map, prefix);
					}
					else
					{
						output.append(";\n");
						output.append(convert_list_comprehension(ret_val.first, symbol_name, global_map, local_map, symbol_type_map, prefix));
					}
				}
				else
				{
					if (type_specified)
					{
						output.append(type + ",\n");
					}
					else
					{
						output =
							convert_list_comprehension(ret_val.first, symbol_name, global_map, local_map, symbol_type_map, prefix);
					}
				}
				t = token_producer.get_next_token();
			}
			else
			{
				std::string value{};
				bool only_const_values{ true };
				while ((t.str != ":") && (t.str != ",") && (t.str != ";") && (t.str != "do") && (t.str != "begin") && (t.str != "end"))
				{
					if (t.str == "=")
					{ // here can be no assignment, so it must be ==
						value.append(" == ");
						t = token_producer.get_next_token();
					}
					else if (t.str == "if")
					{
						value.append(convert_inline_if(t, token_producer).first);
						only_const_values = false;
					}
					else if (t.str == "")
					{
						throw Wrong_Token_Exception{ "Unexpected End of File." };
					}
					else
					{
						value.append(t.str);
						if (!is_const(t.str, global_map, local_map))
						{
							only_const_values = false;
							if ((global_map.count(t.str) > 0) && (global_map[t.str] == "function"))
							{
								t = token_producer.get_next_token();
								value.append(convert_function_call_brakets(t, token_producer, empty_set));
							}
							else
							{
								t = token_producer.get_next_token();
							}
						}
						else
						{
							t = token_producer.get_next_token();
						}
					}
				}
				if (only_const_values)
				{
					int result = evaluate_constant_expression(value, global_map, local_map);
					output.append(type + ",\n");
					local_map[symbol_name] = std::to_string(result); // insert into the map to find it, if it is used to calculate the size of a type!!!
				}
				else
				{
					output.append(type + ",\n");
					local_map[symbol_name] = ""; // insert into the map to symbolize that it is defined but not const!
				}
			}
		}
		else
		{ // find expressions with an assignment
			output.insert(0, prefix);
			bool output_appended{ false };
			while ((t.str != ":") && (t.str != ";") && (t.str != ",") && (t.str != "end") && (t.str != "endfunction") && (t.str != "endprocedure") && (t.str != "endaction") && (t.str != "else") && (t.str != "do") && (t.str != "begin"))
			{
				// output.append("XXX");
				if (t.str == "(")
				{
					output.append(convert_function_call_brakets(t, token_producer, empty_set, println || print));
				}
				else if (t.str == "")
				{
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else
				{
					if (t.str == "=")
					{ // here is no assignment, thus it must be comparsion
					  // output.append(": ");
					}
					else
					{

						output.append(t.str);
					}
					t = token_producer.get_next_token();
				}
			}
			if (in_constructor)
			{
				output.append(dummy_value + ",\n");
			}
			else
			{
				output.append(type + ",\n");
			}
		}
		if ((t.str == ";") || (t.str == ","))
		{
			t = token_producer.get_next_token(); // must drop this token, because ; and , can end one expression, but if it is the last expression in this block, there is no line termination
		}
		if ((symbol_name != symbol) && (symbol != "*"))
		{
			return ""; // stop is symbol is not requested - but must be inserted into the map before
		}
		return output;
	}

	std::string convert_guard(
		Token& t,
		Token_Container& token_producer,
		std::set<std::string>& actor_var_map)
	{
		std::string output{};
		while (t.str != "")
		{
			if (actor_var_map.count(t.str) > 0)
			{
				output.append("self.");
			}
			output.append(t.str);
			t = token_producer.get_next_token();
		}

		return output;
	}
}