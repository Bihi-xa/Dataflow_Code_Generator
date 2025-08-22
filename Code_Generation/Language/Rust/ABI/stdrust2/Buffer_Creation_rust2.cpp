#include "Code_Generation/Code_Generation.hpp"
#include "ABI_stdrust2.hpp"
#include "Config/config.h"
#include "Config/debug.h"
#include <string>
#include <fstream>
#include <filesystem>

/* Generate the file Channel.hpp containing the Channel base class, the data channel derived class
 * and if required the control channel derived class that is not carring data explicitly.
 */
std::string crossbeam_channel =
    "use crossbeam::queue::ArrayQueue;\n"
    "use std::sync::{atomic::{AtomicBool, Ordering},Arc,};\n\n"
    "#[derive(Clone)]\n"
    "pub struct Channel<T> {\n"
    "    queue: Arc<ArrayQueue<T>>,\n"
    "    max_size: usize,\n"
    "    terminated: Arc<AtomicBool>,\n"
    "}\n"
    "#[allow(dead_code)]\n"
    "impl<T> Channel<T> {\n"
    "    pub fn new(max_size: usize) -> Self {\n"
    "        Channel {\n"
    "            queue: Arc::new(ArrayQueue::new(max_size)),\n"
    "            max_size,\n"
    "            terminated: Arc::new(AtomicBool::new(false)),\n"
    "        }\n"
    "    }\n"
    "\n"
    "    pub fn write(&self, value: T) -> Result<(), T> {\n"
    "        self.queue.push(value)\n"
    "    }\n"
    "\n"
    "    pub fn read(&self) -> Option<T> {\n"
    "        self.queue.pop()\n"
    "    }\n"
    "\n"
    "    pub fn terminate(&self) {\n"
    "        self.terminated.store(true, Ordering::Release);\n"
    "    }\n"
    "\n"
    "    pub fn is_terminated(&self) -> bool {\n"
    "        self.terminated.load(Ordering::Acquire)\n"
    "    }\n"
    "\n"
    "    pub fn size(&self) -> usize {\n"
    "        self.queue.len()\n"
    "    }\n"
    "\n"
    "    pub fn free(&self) -> usize {\n"
    "        self.max_size - self.size()\n"
    "    }\n"
    "\n"
    "    pub fn is_empty(&self) -> bool {\n"
    "        self.queue.is_empty()\n"
    "    }\n"
    "\n"
    "    pub fn is_full(&self) -> bool {\n"
    "        self.queue.is_full()\n"
    "    }\n"
    "\n"
    "    pub fn max_size(&self) -> usize {\n"
    "        self.max_size\n"
    "    }\n"
    "\n"
    "}";

std::pair<ABI_stdrust2::Header, ABI_stdrust2::Source>
ABI_stdrust2::generate_channel_code(bool cntrl_chan)
{
    Config *c = c->getInstance();

    std::filesystem::path path{c->get_target_dir()};
    path /= "src";
    path /= "channel.rs";

    std::ofstream output_file{path};
    if (output_file.fail())
    {
        throw Code_Generation::Code_Generation_Exception{"Cannot open the file " + path.string()};
    }
    output_file << crossbeam_channel;

    output_file.close();

    return std::make_pair("channel.rs", "");
}

std::pair<std::string, ABI_stdrust2::Impl_Type> ABI_stdrust2::channel_decl(
    std::string channel_name,
    std::string size,
    std::string type,
    bool static_def,
    std::string prefix)
{
    // p2: channel type
    std::string name = (channel_name == "in") ? "input" : channel_name;
    std::string p2, p1;
    p2 = "Channel<" + type + ">";
    p1 = prefix;
    p1.append(name + ": " + p2 + ", \n");
    return std::make_pair(p1, p2);
}

std::pair<std::string, ABI_stdrust2::Impl_Type> ABI_stdrust2::channel_decl_sen(
    std::string channel_name,
    std::string size,
    std::string type,
    bool static_def,
    std::string prefix)
{
    std::string name = (channel_name == "in") ? "input" : channel_name;
    // p2: channel type
    std::string p2, p1;
    p2 = "Channel<" + type + ">";
    p1 = prefix;
    p1.append(name + ": " + p2 + ", \n");
    return std::make_pair(p1, p2);
}

std::pair<std::string, ABI_stdrust2::Impl_Type> ABI_stdrust2::channel_decl_rec(
    std::string channel_name,
    std::string size,
    std::string type,
    bool static_def,
    std::string prefix)
{
    std::string name = (channel_name == "in") ? "input" : channel_name;
    // p2: channel type
    std::string p2, p1;
    p2 = "Channel<" + type + ">";
    p1 = prefix;
    p1.append(name + ": " + p2 + ", \n");
    return std::make_pair(p1, p2);
}

std::string ABI_stdrust2::channel_init(
    std::string channel_sender,
    std::string channel_receiver,
    Impl_Type t,
    std::string type,
    std::string sz,
    std::string prefix)
{

    std::string r = prefix + "let " + channel_sender + "_" + channel_receiver + " = Channel::new(" + sz + "); ";
    return r;
}

std::string ABI_stdrust2::channel_read(
    std::string channel)
{
    std::string name = (channel == "in") ? "input" : channel;

    return "self." + name + ".read().unwrap()";
}

std::string ABI_stdrust2::channel_write(
    std::string var,
    std::string channel,
    std::string prefix)
{
    return prefix + "self." + channel + ".write((" + var + ").try_into().unwrap());";
}
// no use atm
std::string ABI_stdrust2::channel_prefetch(
    std::string channel,
    std::string offset)
{
    // return channel + "->preview(" + offset + ")";
    return "";
}

std::string ABI_stdrust2::channel_size(
    std::string channel)
{
    std::string name = (channel == "in") ? "input" : channel;
    return "self." + name + ".size()";
}

std::string ABI_stdrust2::channel_free(
    std::string channel)
{
    std::string name = (channel == "in") ? "input" : channel;
    return "self." + name + ".free()";
}