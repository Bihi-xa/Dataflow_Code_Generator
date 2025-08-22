#include "ABI_stdrust2.hpp"

void ABI_stdrust2::init_ABI_support(IR::Dataflow_Network *dpn)
{
	/* Intentially left empty */
}
// Atomic flags arent needed for Rust with Tokio
std::string ABI_stdrust2::atomic_include(void)
{
	return "";
}

std::string ABI_stdrust2::atomic_var_decl(
	std::string var,
	std::string prefix)
{
	return "";
}

std::string ABI_stdrust2::atomic_test_set(
	std::string var,
	std::string prefix)
{
	return "";
}

std::string ABI_stdrust2::atomic_clear(
	std::string var,
	std::string prefix)
{
	return "";
}

std::string ABI_stdrust2::thread_creation_include(void)
{
	return "use rayon::prelude::*;\n";
}

static unsigned thread_count = 0;
std::string ABI_stdrust2::thread_creation(
	std::string function,
	std::string prefix,
	std::string &identifier_out)
{
	// not required for this ABI
	return "";
}

std::string ABI_stdrust2::thread_start(
	std::string identifier,
	std::string prefix)
{
	// not required for this ABI
	return "";
}

std::string ABI_stdrust2::thread_join(
	std::string identifier,
	std::string prefix)
{
	// not required for this ABI
	return "";
}

std::string ABI_stdrust2::allocation_include(void)
{
	// not required for this ABI
	return "";
}

std::string ABI_stdrust2::allocation(
	std::string var,
	std::string size,
	std::string type,
	std::string prefix)
{
	// not required for this ABI.
	return "";
}