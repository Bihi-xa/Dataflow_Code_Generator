add_subdirectory(ABI)

target_sources(Dataflow_Code_Generator PRIVATE
	Action_Conversion_Rust.cpp
	Converter_RVC_Rust.cpp
	Composit_Actor_Creation.cpp
	Core_Creation.cpp
	Generate_Rust.cpp
	Global_Scheduling.cpp
	Local_Scheduling.cpp
	ORCC_Compatibility.cpp
	Single_Actor_Creation.cpp)