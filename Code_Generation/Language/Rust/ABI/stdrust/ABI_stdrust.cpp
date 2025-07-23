#include "ABI_stdrust.hpp"

void ABI_stdrust::init_ABI_support(IR::Dataflow_Network* dpn)
{
	/* Intentially left empty */
}
// Atomic flags arent needed for Rust with Tokio
std::string ABI_stdrust::atomic_include(void)
{
	return "";
}

std::string ABI_stdrust::atomic_var_decl(
	std::string var,
	std::string prefix)
{
	return "";
}

std::string ABI_stdrust::atomic_test_set(
	std::string var,
	std::string prefix)
{
	return "";
}

std::string ABI_stdrust::atomic_clear(
	std::string var,
	std::string prefix)
{
	return "";
}


std::string ABI_stdrust::thread_creation_include(void)
{
	return "use tokio::task::JoinHandle;\n";
}

static unsigned thread_count = 0;
std::string ABI_stdrust::thread_creation(
	std::string function,
	std::string prefix,
	std::string& identifier_out)
{
	// not required for this ABI
	return "";
}

std::string ABI_stdrust::thread_start(
	std::string identifier,
	std::string prefix)
{
	// not required for this ABI
	return "";
}

std::string ABI_stdrust::thread_join(
	std::string identifier,
	std::string prefix)
{
	// not required for this ABI
	return "";
}

std::string ABI_stdrust::allocation_include(void)
{
	// not required for this ABI
	return "";
}

std::string ABI_stdrust::allocation(
	std::string var,
	std::string size,
	std::string type,
	std::string prefix)
{
	// not required for this ABI.
	return "";
}